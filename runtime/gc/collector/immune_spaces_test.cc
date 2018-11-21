/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <sys/mman.h>

#include "common_runtime_test.h"
#include "gc/collector/immune_spaces.h"
#include "gc/space/image_space.h"
#include "gc/space/space-inl.h"
#include "oat_file.h"
#include "thread-current-inl.h"

namespace art {
namespace mirror {
class Object;
}  // namespace mirror
namespace gc {
namespace collector {

class DummyOatFile : public OatFile {
 public:
  DummyOatFile(uint8_t* begin, uint8_t* end) : OatFile("Location", /*executable=*/ false) {
    begin_ = begin;
    end_ = end;
  }
};

class DummyImageSpace : public space::ImageSpace {
 public:
  DummyImageSpace(MemMap&& map,
                  std::unique_ptr<accounting::ContinuousSpaceBitmap> live_bitmap,
                  std::unique_ptr<DummyOatFile>&& oat_file,
                  MemMap&& oat_map)
      : ImageSpace("DummyImageSpace",
                   /*image_location=*/"",
                   std::move(map),
                   std::move(live_bitmap),
                   map.End()),
        oat_map_(std::move(oat_map)) {
    oat_file_ = std::move(oat_file);
    oat_file_non_owned_ = oat_file_.get();
  }

 private:
  MemMap oat_map_;
};

class ImmuneSpacesTest : public CommonRuntimeTest {
  static constexpr size_t kMaxBitmaps = 10;

 public:
  ImmuneSpacesTest() {}

  void ReserveBitmaps() {
    // Create a bunch of dummy bitmaps since these are required to create image spaces. The bitmaps
    // do not need to cover the image spaces though.
    for (size_t i = 0; i < kMaxBitmaps; ++i) {
      std::unique_ptr<accounting::ContinuousSpaceBitmap> bitmap(
          accounting::ContinuousSpaceBitmap::Create("bitmap",
                                                    reinterpret_cast<uint8_t*>(kPageSize),
                                                    kPageSize));
      CHECK(bitmap != nullptr);
      live_bitmaps_.push_back(std::move(bitmap));
    }
  }

  // Create an image space, the oat file is optional.
  DummyImageSpace* CreateImageSpace(size_t image_size,
                                    size_t oat_size,
                                    MemMap* image_reservation,
                                    MemMap* oat_reservation) {
    DCHECK(image_reservation != nullptr);
    DCHECK(oat_reservation != nullptr);
    std::string error_str;
    MemMap image_map = MemMap::MapAnonymous("DummyImageSpace",
                                            image_size,
                                            PROT_READ | PROT_WRITE,
                                            /*low_4gb=*/ true,
                                            /*reservation=*/ image_reservation,
                                            &error_str);
    if (!image_map.IsValid()) {
      LOG(ERROR) << error_str;
      return nullptr;
    }
    CHECK(!live_bitmaps_.empty());
    std::unique_ptr<accounting::ContinuousSpaceBitmap> live_bitmap(std::move(live_bitmaps_.back()));
    live_bitmaps_.pop_back();
    MemMap oat_map = MemMap::MapAnonymous("OatMap",
                                          oat_size,
                                          PROT_READ | PROT_WRITE,
                                          /*low_4gb=*/ true,
                                          /*reservation=*/ oat_reservation,
                                          &error_str);
    if (!oat_map.IsValid()) {
      LOG(ERROR) << error_str;
      return nullptr;
    }
    std::unique_ptr<DummyOatFile> oat_file(new DummyOatFile(oat_map.Begin(), oat_map.End()));
    // Create image header.
    ImageSection sections[ImageHeader::kSectionCount];
    new (image_map.Begin()) ImageHeader(
        /*image_begin=*/ PointerToLowMemUInt32(image_map.Begin()),
        /*image_size=*/ image_map.Size(),
        sections,
        /*image_roots=*/ PointerToLowMemUInt32(image_map.Begin()) + 1,
        /*oat_checksum=*/ 0u,
        // The oat file data in the header is always right after the image space.
        /*oat_file_begin=*/ PointerToLowMemUInt32(oat_map.Begin()),
        /*oat_data_begin=*/ PointerToLowMemUInt32(oat_map.Begin()),
        /*oat_data_end=*/ PointerToLowMemUInt32(oat_map.Begin() + oat_size),
        /*oat_file_end=*/ PointerToLowMemUInt32(oat_map.Begin() + oat_size),
        /*boot_image_begin=*/ 0u,
        /*boot_image_size=*/ 0u,
        /*pointer_size=*/ sizeof(void*),
        ImageHeader::kStorageModeUncompressed,
        /*data_size=*/ 0u);
    return new DummyImageSpace(std::move(image_map),
                               std::move(live_bitmap),
                               std::move(oat_file),
                               std::move(oat_map));
  }

 private:
  // Bitmap pool for pre-allocated dummy bitmaps. We need to pre-allocate them since we don't want
  // them to randomly get placed somewhere where we want an image space.
  std::vector<std::unique_ptr<accounting::ContinuousSpaceBitmap>> live_bitmaps_;
};

class DummySpace : public space::ContinuousSpace {
 public:
  DummySpace(uint8_t* begin, uint8_t* end)
      : ContinuousSpace("DummySpace",
                        space::kGcRetentionPolicyNeverCollect,
                        begin,
                        end,
                        /*limit=*/end) {}

  space::SpaceType GetType() const override {
    return space::kSpaceTypeMallocSpace;
  }

  bool CanMoveObjects() const override {
    return false;
  }

  accounting::ContinuousSpaceBitmap* GetLiveBitmap() const override {
    return nullptr;
  }

  accounting::ContinuousSpaceBitmap* GetMarkBitmap() const override {
    return nullptr;
  }
};

TEST_F(ImmuneSpacesTest, AppendBasic) {
  ImmuneSpaces spaces;
  uint8_t* const base = reinterpret_cast<uint8_t*>(0x1000);
  DummySpace a(base, base + 45 * KB);
  DummySpace b(a.Limit(), a.Limit() + 813 * KB);
  {
    WriterMutexLock mu(Thread::Current(), *Locks::heap_bitmap_lock_);
    spaces.AddSpace(&a);
    spaces.AddSpace(&b);
  }
  EXPECT_TRUE(spaces.ContainsSpace(&a));
  EXPECT_TRUE(spaces.ContainsSpace(&b));
  EXPECT_EQ(reinterpret_cast<uint8_t*>(spaces.GetLargestImmuneRegion().Begin()), a.Begin());
  EXPECT_EQ(reinterpret_cast<uint8_t*>(spaces.GetLargestImmuneRegion().End()), b.Limit());
}

// Tests [image][oat][space] producing a single large immune region.
TEST_F(ImmuneSpacesTest, AppendAfterImage) {
  ReserveBitmaps();
  ImmuneSpaces spaces;
  constexpr size_t kImageSize = 123 * kPageSize;
  constexpr size_t kImageOatSize = 321 * kPageSize;
  constexpr size_t kOtherSpaceSize = 100 * kPageSize;

  std::string error_str;
  MemMap reservation = MemMap::MapAnonymous("reserve",
                                            kImageSize + kImageOatSize + kOtherSpaceSize,
                                            PROT_READ | PROT_WRITE,
                                            /*low_4gb=*/ true,
                                            &error_str);
  ASSERT_TRUE(reservation.IsValid()) << "Failed to allocate memory region " << error_str;
  MemMap image_reservation = reservation.TakeReservedMemory(kImageSize);
  ASSERT_TRUE(image_reservation.IsValid());
  ASSERT_TRUE(reservation.IsValid());

  std::unique_ptr<DummyImageSpace> image_space(CreateImageSpace(kImageSize,
                                                                kImageOatSize,
                                                                &image_reservation,
                                                                &reservation));
  ASSERT_TRUE(image_space != nullptr);
  ASSERT_FALSE(image_reservation.IsValid());
  ASSERT_TRUE(reservation.IsValid());

  const ImageHeader& image_header = image_space->GetImageHeader();
  DummySpace space(image_header.GetOatFileEnd(), image_header.GetOatFileEnd() + kOtherSpaceSize);

  EXPECT_EQ(image_header.GetImageSize(), kImageSize);
  EXPECT_EQ(static_cast<size_t>(image_header.GetOatFileEnd() - image_header.GetOatFileBegin()),
            kImageOatSize);
  EXPECT_EQ(image_space->GetOatFile()->Size(), kImageOatSize);
  // Check that we do not include the oat if there is no space after.
  {
    WriterMutexLock mu(Thread::Current(), *Locks::heap_bitmap_lock_);
    spaces.AddSpace(image_space.get());
  }
  EXPECT_EQ(reinterpret_cast<uint8_t*>(spaces.GetLargestImmuneRegion().Begin()),
            image_space->Begin());
  EXPECT_EQ(reinterpret_cast<uint8_t*>(spaces.GetLargestImmuneRegion().End()),
            image_space->Limit());
  // Add another space and ensure it gets appended.
  EXPECT_NE(image_space->Limit(), space.Begin());
  {
    WriterMutexLock mu(Thread::Current(), *Locks::heap_bitmap_lock_);
    spaces.AddSpace(&space);
  }
  EXPECT_TRUE(spaces.ContainsSpace(image_space.get()));
  EXPECT_TRUE(spaces.ContainsSpace(&space));
  // CreateLargestImmuneRegion should have coalesced the two spaces since the oat code after the
  // image prevents gaps.
  // Check that we have a continuous region.
  EXPECT_EQ(reinterpret_cast<uint8_t*>(spaces.GetLargestImmuneRegion().Begin()),
            image_space->Begin());
  EXPECT_EQ(reinterpret_cast<uint8_t*>(spaces.GetLargestImmuneRegion().End()), space.Limit());
}

// Test [image1][image2][image1 oat][image2 oat][image3] producing a single large immune region.
TEST_F(ImmuneSpacesTest, MultiImage) {
  ReserveBitmaps();
  // Image 2 needs to be smaller or else it may be chosen for immune region.
  constexpr size_t kImage1Size = kPageSize * 17;
  constexpr size_t kImage2Size = kPageSize * 13;
  constexpr size_t kImage3Size = kPageSize * 3;
  constexpr size_t kImage1OatSize = kPageSize * 5;
  constexpr size_t kImage2OatSize = kPageSize * 8;
  constexpr size_t kImage3OatSize = kPageSize;
  constexpr size_t kImageBytes = kImage1Size + kImage2Size + kImage3Size;
  constexpr size_t kMemorySize = kImageBytes + kImage1OatSize + kImage2OatSize + kImage3OatSize;
  std::string error_str;
  MemMap reservation = MemMap::MapAnonymous("reserve",
                                            kMemorySize,
                                            PROT_READ | PROT_WRITE,
                                            /*low_4gb=*/ true,
                                            &error_str);
  ASSERT_TRUE(reservation.IsValid()) << "Failed to allocate memory region " << error_str;
  MemMap image_reservation = reservation.TakeReservedMemory(kImage1Size + kImage2Size);
  ASSERT_TRUE(image_reservation.IsValid());
  ASSERT_TRUE(reservation.IsValid());

  std::unique_ptr<DummyImageSpace> space1(CreateImageSpace(kImage1Size,
                                                           kImage1OatSize,
                                                           &image_reservation,
                                                           &reservation));
  ASSERT_TRUE(space1 != nullptr);
  ASSERT_TRUE(image_reservation.IsValid());
  ASSERT_TRUE(reservation.IsValid());

  std::unique_ptr<DummyImageSpace> space2(CreateImageSpace(kImage2Size,
                                                           kImage2OatSize,
                                                           &image_reservation,
                                                           &reservation));
  ASSERT_TRUE(space2 != nullptr);
  ASSERT_FALSE(image_reservation.IsValid());
  ASSERT_TRUE(reservation.IsValid());

  // Finally put a 3rd image space.
  image_reservation = reservation.TakeReservedMemory(kImage3Size);
  ASSERT_TRUE(image_reservation.IsValid());
  ASSERT_TRUE(reservation.IsValid());
  std::unique_ptr<DummyImageSpace> space3(CreateImageSpace(kImage3Size,
                                                           kImage3OatSize,
                                                           &image_reservation,
                                                           &reservation));
  ASSERT_TRUE(space3 != nullptr);
  ASSERT_FALSE(image_reservation.IsValid());
  ASSERT_FALSE(reservation.IsValid());

  // Check that we do not include the oat if there is no space after.
  ImmuneSpaces spaces;
  {
    WriterMutexLock mu(Thread::Current(), *Locks::heap_bitmap_lock_);
    LOG(INFO) << "Adding space1 " << reinterpret_cast<const void*>(space1->Begin());
    spaces.AddSpace(space1.get());
    LOG(INFO) << "Adding space2 " << reinterpret_cast<const void*>(space2->Begin());
    spaces.AddSpace(space2.get());
  }
  // There are no more heap bytes, the immune region should only be the first 2 image spaces and
  // should exclude the image oat files.
  EXPECT_EQ(reinterpret_cast<uint8_t*>(spaces.GetLargestImmuneRegion().Begin()),
            space1->Begin());
  EXPECT_EQ(reinterpret_cast<uint8_t*>(spaces.GetLargestImmuneRegion().End()),
            space2->Limit());

  // Add another space after the oat files, now it should contain the entire memory region.
  {
    WriterMutexLock mu(Thread::Current(), *Locks::heap_bitmap_lock_);
    LOG(INFO) << "Adding space3 " << reinterpret_cast<const void*>(space3->Begin());
    spaces.AddSpace(space3.get());
  }
  EXPECT_EQ(reinterpret_cast<uint8_t*>(spaces.GetLargestImmuneRegion().Begin()),
            space1->Begin());
  EXPECT_EQ(reinterpret_cast<uint8_t*>(spaces.GetLargestImmuneRegion().End()),
            space3->Limit());

  // Add a smaller non-adjacent space and ensure it does not become part of the immune region.
  // Image size is kImageBytes - kPageSize
  // Oat size is kPageSize.
  // Guard pages to ensure it is not adjacent to an existing immune region.
  // Layout:  [guard page][image][oat][guard page]
  constexpr size_t kGuardSize = kPageSize;
  constexpr size_t kImage4Size = kImageBytes - kPageSize;
  constexpr size_t kImage4OatSize = kPageSize;

  reservation = MemMap::MapAnonymous("reserve",
                                     kImage4Size + kImage4OatSize + kGuardSize * 2,
                                     PROT_READ | PROT_WRITE,
                                     /*low_4gb=*/ true,
                                     &error_str);
  ASSERT_TRUE(reservation.IsValid()) << "Failed to allocate memory region " << error_str;
  MemMap guard = reservation.TakeReservedMemory(kGuardSize);
  ASSERT_TRUE(guard.IsValid());
  ASSERT_TRUE(reservation.IsValid());
  guard.Reset();  // Release the guard memory.
  image_reservation = reservation.TakeReservedMemory(kImage4Size);
  ASSERT_TRUE(image_reservation.IsValid());
  ASSERT_TRUE(reservation.IsValid());
  std::unique_ptr<DummyImageSpace> space4(CreateImageSpace(kImage4Size,
                                                           kImage4OatSize,
                                                           &image_reservation,
                                                           &reservation));
  ASSERT_TRUE(space4 != nullptr);
  ASSERT_FALSE(image_reservation.IsValid());
  ASSERT_TRUE(reservation.IsValid());
  ASSERT_EQ(reservation.Size(), kGuardSize);
  reservation.Reset();  // Release the guard memory.
  {
    WriterMutexLock mu(Thread::Current(), *Locks::heap_bitmap_lock_);
    LOG(INFO) << "Adding space4 " << reinterpret_cast<const void*>(space4->Begin());
    spaces.AddSpace(space4.get());
  }
  EXPECT_EQ(reinterpret_cast<uint8_t*>(spaces.GetLargestImmuneRegion().Begin()),
            space1->Begin());
  EXPECT_EQ(reinterpret_cast<uint8_t*>(spaces.GetLargestImmuneRegion().End()),
            space3->Limit());

  // Add a larger non-adjacent space and ensure it becomes the new largest immune region.
  // Image size is kImageBytes + kPageSize
  // Oat size is kPageSize.
  // Guard pages to ensure it is not adjacent to an existing immune region.
  // Layout:  [guard page][image][oat][guard page]
  constexpr size_t kImage5Size = kImageBytes + kPageSize;
  constexpr size_t kImage5OatSize = kPageSize;
  reservation = MemMap::MapAnonymous("reserve",
                                     kImage5Size + kImage5OatSize + kGuardSize * 2,
                                     PROT_READ | PROT_WRITE,
                                     /*low_4gb=*/ true,
                                     &error_str);
  ASSERT_TRUE(reservation.IsValid()) << "Failed to allocate memory region " << error_str;
  guard = reservation.TakeReservedMemory(kGuardSize);
  ASSERT_TRUE(guard.IsValid());
  ASSERT_TRUE(reservation.IsValid());
  guard.Reset();  // Release the guard memory.
  image_reservation = reservation.TakeReservedMemory(kImage5Size);
  ASSERT_TRUE(image_reservation.IsValid());
  ASSERT_TRUE(reservation.IsValid());
  std::unique_ptr<DummyImageSpace> space5(CreateImageSpace(kImage5Size,
                                                           kImage5OatSize,
                                                           &image_reservation,
                                                           &reservation));
  ASSERT_TRUE(space5 != nullptr);
  ASSERT_FALSE(image_reservation.IsValid());
  ASSERT_TRUE(reservation.IsValid());
  ASSERT_EQ(reservation.Size(), kGuardSize);
  reservation.Reset();  // Release the guard memory.
  {
    WriterMutexLock mu(Thread::Current(), *Locks::heap_bitmap_lock_);
    LOG(INFO) << "Adding space5 " << reinterpret_cast<const void*>(space5->Begin());
    spaces.AddSpace(space5.get());
  }
  EXPECT_EQ(reinterpret_cast<uint8_t*>(spaces.GetLargestImmuneRegion().Begin()), space5->Begin());
  EXPECT_EQ(reinterpret_cast<uint8_t*>(spaces.GetLargestImmuneRegion().End()), space5->Limit());
}

}  // namespace collector
}  // namespace gc
}  // namespace art
