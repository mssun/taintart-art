/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include "image_writer.h"

#include <lz4.h>
#include <lz4hc.h>
#include <sys/stat.h>
#include <zlib.h>

#include <memory>
#include <numeric>
#include <unordered_set>
#include <vector>

#include "art_field-inl.h"
#include "art_method-inl.h"
#include "base/callee_save_type.h"
#include "base/enums.h"
#include "base/globals.h"
#include "base/logging.h"  // For VLOG.
#include "base/unix_file/fd_file.h"
#include "class_linker-inl.h"
#include "class_root.h"
#include "compiled_method.h"
#include "dex/dex_file-inl.h"
#include "dex/dex_file_types.h"
#include "driver/compiler_options.h"
#include "elf_file.h"
#include "elf_utils.h"
#include "gc/accounting/card_table-inl.h"
#include "gc/accounting/heap_bitmap.h"
#include "gc/accounting/space_bitmap-inl.h"
#include "gc/collector/concurrent_copying.h"
#include "gc/heap-visit-objects-inl.h"
#include "gc/heap.h"
#include "gc/space/large_object_space.h"
#include "gc/space/space-inl.h"
#include "gc/verification.h"
#include "handle_scope-inl.h"
#include "image.h"
#include "imt_conflict_table.h"
#include "intern_table-inl.h"
#include "jni/jni_internal.h"
#include "linear_alloc.h"
#include "lock_word.h"
#include "mirror/array-inl.h"
#include "mirror/class-inl.h"
#include "mirror/class_ext.h"
#include "mirror/class_loader.h"
#include "mirror/dex_cache-inl.h"
#include "mirror/dex_cache.h"
#include "mirror/executable.h"
#include "mirror/method.h"
#include "mirror/object-inl.h"
#include "mirror/object-refvisitor-inl.h"
#include "mirror/object_array-alloc-inl.h"
#include "mirror/object_array-inl.h"
#include "mirror/string-inl.h"
#include "oat.h"
#include "oat_file.h"
#include "oat_file_manager.h"
#include "optimizing/intrinsic_objects.h"
#include "runtime.h"
#include "scoped_thread_state_change-inl.h"
#include "subtype_check.h"
#include "utils/dex_cache_arrays_layout-inl.h"
#include "well_known_classes.h"

using ::art::mirror::Class;
using ::art::mirror::DexCache;
using ::art::mirror::Object;
using ::art::mirror::ObjectArray;
using ::art::mirror::String;

namespace art {
namespace linker {

static ArrayRef<const uint8_t> MaybeCompressData(ArrayRef<const uint8_t> source,
                                                 ImageHeader::StorageMode image_storage_mode,
                                                 /*out*/ std::vector<uint8_t>* storage) {
  const uint64_t compress_start_time = NanoTime();

  switch (image_storage_mode) {
    case ImageHeader::kStorageModeLZ4: {
      storage->resize(LZ4_compressBound(source.size()));
      size_t data_size = LZ4_compress_default(
          reinterpret_cast<char*>(const_cast<uint8_t*>(source.data())),
          reinterpret_cast<char*>(storage->data()),
          source.size(),
          storage->size());
      storage->resize(data_size);
      break;
    }
    case ImageHeader::kStorageModeLZ4HC: {
      // Bound is same as non HC.
      storage->resize(LZ4_compressBound(source.size()));
      size_t data_size = LZ4_compress_HC(
          reinterpret_cast<const char*>(const_cast<uint8_t*>(source.data())),
          reinterpret_cast<char*>(storage->data()),
          source.size(),
          storage->size(),
          LZ4HC_CLEVEL_MAX);
      storage->resize(data_size);
      break;
    }
    case ImageHeader::kStorageModeUncompressed: {
      return source;
    }
    default: {
      LOG(FATAL) << "Unsupported";
      UNREACHABLE();
    }
  }

  DCHECK(image_storage_mode == ImageHeader::kStorageModeLZ4 ||
         image_storage_mode == ImageHeader::kStorageModeLZ4HC);
  VLOG(compiler) << "Compressed from " << source.size() << " to " << storage->size() << " in "
                 << PrettyDuration(NanoTime() - compress_start_time);
  if (kIsDebugBuild) {
    std::vector<uint8_t> decompressed(source.size());
    const size_t decompressed_size = LZ4_decompress_safe(
        reinterpret_cast<char*>(storage->data()),
        reinterpret_cast<char*>(decompressed.data()),
        storage->size(),
        decompressed.size());
    CHECK_EQ(decompressed_size, decompressed.size());
    CHECK_EQ(memcmp(source.data(), decompressed.data(), source.size()), 0) << image_storage_mode;
  }
  return ArrayRef<const uint8_t>(*storage);
}

// Separate objects into multiple bins to optimize dirty memory use.
static constexpr bool kBinObjects = true;

ObjPtr<mirror::ClassLoader> ImageWriter::GetAppClassLoader() const
    REQUIRES_SHARED(Locks::mutator_lock_) {
  return compiler_options_.IsAppImage()
      ? ObjPtr<mirror::ClassLoader>::DownCast(Thread::Current()->DecodeJObject(app_class_loader_))
      : nullptr;
}

// Return true if an object is already in an image space.
bool ImageWriter::IsInBootImage(const void* obj) const {
  gc::Heap* const heap = Runtime::Current()->GetHeap();
  if (compiler_options_.IsBootImage()) {
    DCHECK(heap->GetBootImageSpaces().empty());
    return false;
  }
  for (gc::space::ImageSpace* boot_image_space : heap->GetBootImageSpaces()) {
    const uint8_t* image_begin = boot_image_space->Begin();
    // Real image end including ArtMethods and ArtField sections.
    const uint8_t* image_end = image_begin + boot_image_space->GetImageHeader().GetImageSize();
    if (image_begin <= obj && obj < image_end) {
      return true;
    }
  }
  return false;
}

bool ImageWriter::IsInBootOatFile(const void* ptr) const {
  gc::Heap* const heap = Runtime::Current()->GetHeap();
  if (compiler_options_.IsBootImage()) {
    DCHECK(heap->GetBootImageSpaces().empty());
    return false;
  }
  for (gc::space::ImageSpace* boot_image_space : heap->GetBootImageSpaces()) {
    const ImageHeader& image_header = boot_image_space->GetImageHeader();
    if (image_header.GetOatFileBegin() <= ptr && ptr < image_header.GetOatFileEnd()) {
      return true;
    }
  }
  return false;
}

static void ClearDexFileCookies() REQUIRES_SHARED(Locks::mutator_lock_) {
  auto visitor = [](Object* obj) REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK(obj != nullptr);
    Class* klass = obj->GetClass();
    if (klass == WellKnownClasses::ToClass(WellKnownClasses::dalvik_system_DexFile)) {
      ArtField* field = jni::DecodeArtField(WellKnownClasses::dalvik_system_DexFile_cookie);
      // Null out the cookie to enable determinism. b/34090128
      field->SetObject</*kTransactionActive*/false>(obj, nullptr);
    }
  };
  Runtime::Current()->GetHeap()->VisitObjects(visitor);
}

bool ImageWriter::PrepareImageAddressSpace(TimingLogger* timings) {
  target_ptr_size_ = InstructionSetPointerSize(compiler_options_.GetInstructionSet());

  Thread* const self = Thread::Current();

  gc::Heap* const heap = Runtime::Current()->GetHeap();
  {
    ScopedObjectAccess soa(self);
    {
      TimingLogger::ScopedTiming t("PruneNonImageClasses", timings);
      PruneNonImageClasses();  // Remove junk
    }

    if (compiler_options_.IsAppImage()) {
      TimingLogger::ScopedTiming t("ClearDexFileCookies", timings);
      // Clear dex file cookies for app images to enable app image determinism. This is required
      // since the cookie field contains long pointers to DexFiles which are not deterministic.
      // b/34090128
      ClearDexFileCookies();
    }
  }

  {
    TimingLogger::ScopedTiming t("CollectGarbage", timings);
    heap->CollectGarbage(/* clear_soft_references */ false);  // Remove garbage.
  }

  if (kIsDebugBuild) {
    ScopedObjectAccess soa(self);
    CheckNonImageClassesRemoved();
  }

  // Used to store information that will later be used to calculate image
  // offsets to string references in the AppImage.
  std::vector<HeapReferencePointerInfo> string_ref_info;
  if (ClassLinker::kAppImageMayContainStrings && compiler_options_.IsAppImage()) {
    // Count the number of string fields so we can allocate the appropriate
    // amount of space in the image section.
    TimingLogger::ScopedTiming t("AppImage:CollectStringReferenceInfo", timings);
    ScopedObjectAccess soa(self);

    if (kIsDebugBuild) {
      VerifyNativeGCRootInvariants();
      CHECK_EQ(image_infos_.size(), 1u);
    }

    string_ref_info = CollectStringReferenceInfo();
    image_infos_.back().num_string_references_ = string_ref_info.size();
  }

  {
    TimingLogger::ScopedTiming t("CalculateNewObjectOffsets", timings);
    ScopedObjectAccess soa(self);
    CalculateNewObjectOffsets();
  }

  // Obtain class count for debugging purposes
  if (VLOG_IS_ON(compiler) && compiler_options_.IsAppImage()) {
    ScopedObjectAccess soa(self);

    size_t app_image_class_count  = 0;

    for (ImageInfo& info : image_infos_) {
      info.class_table_->Visit([&](ObjPtr<mirror::Class> klass)
                                   REQUIRES_SHARED(Locks::mutator_lock_) {
        if (!IsInBootImage(klass.Ptr())) {
          ++app_image_class_count;
        }

        // Indicate that we would like to continue visiting classes.
        return true;
      });
    }

    VLOG(compiler) << "Dex2Oat:AppImage:classCount = " << app_image_class_count;
  }

  if (ClassLinker::kAppImageMayContainStrings && compiler_options_.IsAppImage()) {
    // Use the string reference information obtained earlier to calculate image
    // offsets.  These will later be written to the image by Write/CopyMetadata.
    TimingLogger::ScopedTiming t("AppImage:CalculateImageOffsets", timings);
    ScopedObjectAccess soa(self);

    size_t managed_string_refs = 0;
    size_t native_string_refs = 0;

    /*
     * Iterate over the string reference info and calculate image offsets.
     * The first element of the pair is either the object the reference belongs
     * to or the beginning of the native reference array it is located in.  In
     * the first case the second element is the offset of the field relative to
     * the object's base address.  In the second case, it is the index of the
     * StringDexCacheType object in the array.
     */
    for (const HeapReferencePointerInfo& ref_info : string_ref_info) {
      uint32_t base_offset;

      if (HasDexCacheStringNativeRefTag(ref_info.first)) {
        ++native_string_refs;
        auto* obj_ptr = reinterpret_cast<mirror::Object*>(ClearDexCacheNativeRefTags(
            ref_info.first));
        base_offset = SetDexCacheStringNativeRefTag(GetImageOffset(obj_ptr));
      } else if (HasDexCachePreResolvedStringNativeRefTag(ref_info.first)) {
        ++native_string_refs;
        auto* obj_ptr = reinterpret_cast<mirror::Object*>(ClearDexCacheNativeRefTags(
            ref_info.first));
        base_offset = SetDexCachePreResolvedStringNativeRefTag(GetImageOffset(obj_ptr));
      } else {
        ++managed_string_refs;
        base_offset = GetImageOffset(reinterpret_cast<mirror::Object*>(ref_info.first));
      }

      string_reference_offsets_.emplace_back(base_offset, ref_info.second);
    }

    CHECK_EQ(image_infos_.back().num_string_references_,
             string_reference_offsets_.size());

    VLOG(compiler) << "Dex2Oat:AppImage:stringReferences = " << string_reference_offsets_.size();
    VLOG(compiler) << "Dex2Oat:AppImage:managedStringReferences = " << managed_string_refs;
    VLOG(compiler) << "Dex2Oat:AppImage:nativeStringReferences = " << native_string_refs;
  }

  // This needs to happen after CalculateNewObjectOffsets since it relies on intern_table_bytes_ and
  // bin size sums being calculated.
  TimingLogger::ScopedTiming t("AllocMemory", timings);
  return AllocMemory();
}

class ImageWriter::CollectStringReferenceVisitor {
 public:
  explicit CollectStringReferenceVisitor(const ImageWriter& image_writer)
      : image_writer_(image_writer),
        curr_obj_(nullptr),
        string_ref_info_(0),
        dex_cache_string_ref_counter_(0) {}

  // Used to prevent repeated null checks in the code that calls the visitor.
  ALWAYS_INLINE
  void VisitRootIfNonNull(mirror::CompressedReference<mirror::Object>* root) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    if (!root->IsNull()) {
      VisitRoot(root);
    }
  }

  /*
   * Counts the number of native references to strings reachable through
   * DexCache objects for verification later.
   */
  ALWAYS_INLINE
  void VisitRoot(mirror::CompressedReference<mirror::Object>* root) const
      REQUIRES_SHARED(Locks::mutator_lock_)  {
    ObjPtr<mirror::Object> referred_obj = root->AsMirrorPtr();

    if (curr_obj_->IsDexCache() &&
        image_writer_.IsValidAppImageStringReference(referred_obj)) {
      ++dex_cache_string_ref_counter_;
    }
  }

  // Collects info for managed fields that reference managed Strings.
  ALWAYS_INLINE
  void operator() (ObjPtr<mirror::Object> obj,
                   MemberOffset member_offset,
                   bool is_static ATTRIBUTE_UNUSED) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    ObjPtr<mirror::Object> referred_obj =
        obj->GetFieldObject<mirror::Object, kVerifyNone, kWithoutReadBarrier>(
            member_offset);

    if (image_writer_.IsValidAppImageStringReference(referred_obj)) {
      string_ref_info_.emplace_back(reinterpret_cast<uintptr_t>(obj.Ptr()),
                                    member_offset.Uint32Value());
    }
  }

  ALWAYS_INLINE
  void operator() (ObjPtr<mirror::Class> klass ATTRIBUTE_UNUSED,
                   ObjPtr<mirror::Reference> ref) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    operator()(ref, mirror::Reference::ReferentOffset(), /* is_static */ false);
  }

  void AddStringRefInfo(uint32_t first, uint32_t second) {
    string_ref_info_.emplace_back(first, second);
  }

  std::vector<HeapReferencePointerInfo>&& MoveRefInfo() {
    return std::move(string_ref_info_);
  }

  // Used by the wrapper function to obtain a native reference count.
  size_t GetDexCacheStringRefCount() const {
    return dex_cache_string_ref_counter_;
  }

  void SetObject(ObjPtr<mirror::Object> obj) {
    curr_obj_ = obj;
    dex_cache_string_ref_counter_ = 0;
  }

 private:
  const ImageWriter& image_writer_;
  ObjPtr<mirror::Object> curr_obj_;
  mutable std::vector<HeapReferencePointerInfo> string_ref_info_;
  mutable size_t dex_cache_string_ref_counter_;
};

std::vector<ImageWriter::HeapReferencePointerInfo> ImageWriter::CollectStringReferenceInfo() const
    REQUIRES_SHARED(Locks::mutator_lock_) {
  gc::Heap* const heap = Runtime::Current()->GetHeap();
  CollectStringReferenceVisitor visitor(*this);

  /*
   * References to managed strings can occur either in the managed heap or in
   * native memory regions.  Information about managed references is collected
   * by the CollectStringReferenceVisitor and directly added to the internal
   * info vector.
   *
   * Native references to managed strings can only occur through DexCache
   * objects.  This is verified by VerifyNativeGCRootInvariants().  Due to the
   * fact that these native references are encapsulated in std::atomic objects
   * the VisitReferences() function can't pass the visiting object the address
   * of the reference.  Instead, the VisitReferences() function loads the
   * reference into a temporary variable and passes that address to the
   * visitor.  As a consequence of this we can't uniquely identify the location
   * of the string reference in the visitor.
   *
   * Due to these limitations, the visitor will only count the number of
   * managed strings reachable through the native references of a DexCache
   * object.  If there are any such strings, this function will then iterate
   * over the native references, test the string for membership in the
   * AppImage, and add the tagged DexCache pointer and string array offset to
   * the info vector if necessary.
   */
  heap->VisitObjects([this, &visitor](ObjPtr<mirror::Object> object)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    if (!IsInBootImage(object.Ptr())) {
      visitor.SetObject(object);

      if (object->IsDexCache()) {
        object->VisitReferences</* kVisitNativeRoots= */ true,
                                kVerifyNone,
                                kWithoutReadBarrier>(visitor, visitor);

        if (visitor.GetDexCacheStringRefCount() > 0) {
          size_t string_info_collected = 0;

          ObjPtr<mirror::DexCache> dex_cache = object->AsDexCache();
          DCHECK_LE(visitor.GetDexCacheStringRefCount(), dex_cache->NumStrings());

          for (uint32_t index = 0; index < dex_cache->NumStrings(); ++index) {
            // GetResolvedString() can't be used here due to the circular
            // nature of the cache and the collision detection this requires.
            ObjPtr<mirror::String> referred_string =
                dex_cache->GetStrings()[index].load().object.Read();

            if (IsValidAppImageStringReference(referred_string)) {
              ++string_info_collected;
              visitor.AddStringRefInfo(
                  SetDexCacheStringNativeRefTag(reinterpret_cast<uintptr_t>(object.Ptr())), index);
            }
          }

          // Visit all of the preinitialized strings.
          GcRoot<mirror::String>* preresolved_strings = dex_cache->GetPreResolvedStrings();
          for (size_t index = 0; index < dex_cache->NumPreResolvedStrings(); ++index) {
            ObjPtr<mirror::String> referred_string = preresolved_strings[index].Read();
            if (IsValidAppImageStringReference(referred_string)) {
              ++string_info_collected;
              visitor.AddStringRefInfo(SetDexCachePreResolvedStringNativeRefTag(
                reinterpret_cast<uintptr_t>(object.Ptr())),
                index);
            }
          }

          DCHECK_EQ(string_info_collected, visitor.GetDexCacheStringRefCount());
        }
      } else {
        object->VisitReferences</* kVisitNativeRoots= */ false,
                                kVerifyNone,
                                kWithoutReadBarrier>(visitor, visitor);
      }
    }
  });

  return visitor.MoveRefInfo();
}

class ImageWriter::NativeGCRootInvariantVisitor {
 public:
  explicit NativeGCRootInvariantVisitor(const ImageWriter& image_writer) :
    curr_obj_(nullptr), class_violation_(false), class_loader_violation_(false),
    image_writer_(image_writer) {}

  ALWAYS_INLINE
  void VisitRootIfNonNull(mirror::CompressedReference<mirror::Object>* root) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    if (!root->IsNull()) {
      VisitRoot(root);
    }
  }

  ALWAYS_INLINE
  void VisitRoot(mirror::CompressedReference<mirror::Object>* root) const
      REQUIRES_SHARED(Locks::mutator_lock_)  {
    ObjPtr<mirror::Object> referred_obj = root->AsMirrorPtr();

    if (curr_obj_->IsClass()) {
      class_violation_ = class_violation_ ||
                         image_writer_.IsValidAppImageStringReference(referred_obj);

    } else if (curr_obj_->IsClassLoader()) {
      class_loader_violation_ = class_loader_violation_ ||
                                image_writer_.IsValidAppImageStringReference(referred_obj);

    } else if (!curr_obj_->IsDexCache()) {
      LOG(FATAL) << "Dex2Oat:AppImage | " <<
                    "Native reference to String found in unexpected object type.";
    }
  }

  ALWAYS_INLINE
  void operator() (ObjPtr<mirror::Object> obj ATTRIBUTE_UNUSED,
                   MemberOffset member_offset ATTRIBUTE_UNUSED,
                   bool is_static ATTRIBUTE_UNUSED) const
      REQUIRES_SHARED(Locks::mutator_lock_) {}

  ALWAYS_INLINE
  void operator() (ObjPtr<mirror::Class> klass ATTRIBUTE_UNUSED,
                   ObjPtr<mirror::Reference> ref ATTRIBUTE_UNUSED) const
      REQUIRES_SHARED(Locks::mutator_lock_) {}

  // Returns true iff the only reachable native string references are through DexCache objects.
  bool InvariantsHold() const {
    return !(class_violation_ || class_loader_violation_);
  }

  ObjPtr<mirror::Object> curr_obj_;
  mutable bool class_violation_;
  mutable bool class_loader_violation_;

 private:
  const ImageWriter& image_writer_;
};

void ImageWriter::VerifyNativeGCRootInvariants() const REQUIRES_SHARED(Locks::mutator_lock_) {
  gc::Heap* const heap = Runtime::Current()->GetHeap();

  NativeGCRootInvariantVisitor visitor(*this);

  heap->VisitObjects([this, &visitor](ObjPtr<mirror::Object> object)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    visitor.curr_obj_ = object;

    if (!IsInBootImage(object.Ptr())) {
      object->VisitReferences</* kVisitNativeReferences= */ true,
                              kVerifyNone,
                              kWithoutReadBarrier>(visitor, visitor);
    }
  });

  bool error = false;
  std::ostringstream error_str;

  /*
   * Build the error string
   */

  if (UNLIKELY(visitor.class_violation_)) {
    error_str << "Class";
    error = true;
  }

  if (UNLIKELY(visitor.class_loader_violation_)) {
    if (error) {
      error_str << ", ";
    }

    error_str << "ClassLoader";
  }

  CHECK(visitor.InvariantsHold()) <<
    "Native GC root invariant failure. String ref invariants don't hold for the following " <<
    "object types: " << error_str.str();
}

void ImageWriter::CopyMetadata() {
  DCHECK(compiler_options_.IsAppImage());
  CHECK_EQ(image_infos_.size(), 1u);

  const ImageInfo& image_info = image_infos_.back();
  std::vector<ImageSection> image_sections = image_info.CreateImageSections().second;

  auto* sfo_section_base = reinterpret_cast<AppImageReferenceOffsetInfo*>(
      image_info.image_.Begin() +
      image_sections[ImageHeader::kSectionStringReferenceOffsets].Offset());

  std::copy(string_reference_offsets_.begin(),
            string_reference_offsets_.end(),
            sfo_section_base);
}

bool ImageWriter::IsValidAppImageStringReference(ObjPtr<mirror::Object> referred_obj) const {
  return referred_obj != nullptr &&
         !IsInBootImage(referred_obj.Ptr()) &&
         referred_obj->IsString();
}

// Helper class that erases the image file if it isn't properly flushed and closed.
class ImageWriter::ImageFileGuard {
 public:
  ImageFileGuard() noexcept = default;
  ImageFileGuard(ImageFileGuard&& other) noexcept = default;
  ImageFileGuard& operator=(ImageFileGuard&& other) noexcept = default;

  ~ImageFileGuard() {
    if (image_file_ != nullptr) {
      // Failure, erase the image file.
      image_file_->Erase();
    }
  }

  void reset(File* image_file) {
    image_file_.reset(image_file);
  }

  bool operator==(std::nullptr_t) {
    return image_file_ == nullptr;
  }

  bool operator!=(std::nullptr_t) {
    return image_file_ != nullptr;
  }

  File* operator->() const {
    return image_file_.get();
  }

  bool WriteHeaderAndClose(const std::string& image_filename, const ImageHeader* image_header) {
    // The header is uncompressed since it contains whether the image is compressed or not.
    if (!image_file_->PwriteFully(image_header, sizeof(ImageHeader), 0)) {
      PLOG(ERROR) << "Failed to write image file header " << image_filename;
      return false;
    }

    // FlushCloseOrErase() takes care of erasing, so the destructor does not need
    // to do that whether the FlushCloseOrErase() succeeds or fails.
    std::unique_ptr<File> image_file = std::move(image_file_);
    if (image_file->FlushCloseOrErase() != 0) {
      PLOG(ERROR) << "Failed to flush and close image file " << image_filename;
      return false;
    }

    return true;
  }

 private:
  std::unique_ptr<File> image_file_;
};

bool ImageWriter::Write(int image_fd,
                        const std::vector<std::string>& image_filenames,
                        const std::vector<std::string>& oat_filenames) {
  // If image_fd or oat_fd are not kInvalidFd then we may have empty strings in image_filenames or
  // oat_filenames.
  CHECK(!image_filenames.empty());
  if (image_fd != kInvalidFd) {
    CHECK_EQ(image_filenames.size(), 1u);
  }
  CHECK(!oat_filenames.empty());
  CHECK_EQ(image_filenames.size(), oat_filenames.size());

  Thread* self = Thread::Current();
  {
    // Preload deterministic contents to the dex cache arrays we're going to write.
    ScopedObjectAccess soa(self);
    ObjPtr<mirror::ClassLoader> class_loader = GetAppClassLoader();
    std::vector<ObjPtr<mirror::DexCache>> dex_caches = FindDexCaches(self);
    for (ObjPtr<mirror::DexCache> dex_cache : dex_caches) {
      if (IsInBootImage(dex_cache.Ptr())) {
        continue;  // Boot image DexCache is not written to the app image.
      }
      PreloadDexCache(dex_cache, class_loader);
    }
  }

  {
    ScopedObjectAccess soa(self);
    for (size_t i = 0; i < oat_filenames.size(); ++i) {
      CreateHeader(i);
      CopyAndFixupNativeData(i);
    }
  }

  {
    // TODO: heap validation can't handle these fix up passes.
    ScopedObjectAccess soa(self);
    Runtime::Current()->GetHeap()->DisableObjectValidation();
    CopyAndFixupObjects();
  }

  if (compiler_options_.IsAppImage()) {
    CopyMetadata();
  }

  // Primary image header shall be written last for two reasons. First, this ensures
  // that we shall not end up with a valid primary image and invalid secondary image.
  // Second, its checksum shall include the checksums of the secondary images (XORed).
  // This way only the primary image checksum needs to be checked to determine whether
  // any of the images or oat files are out of date. (Oat file checksums are included
  // in the image checksum calculation.)
  ImageHeader* primary_header = reinterpret_cast<ImageHeader*>(image_infos_[0].image_.Begin());
  ImageFileGuard primary_image_file;
  for (size_t i = 0; i < image_filenames.size(); ++i) {
    const std::string& image_filename = image_filenames[i];
    ImageInfo& image_info = GetImageInfo(i);
    ImageFileGuard image_file;
    if (image_fd != kInvalidFd) {
      if (image_filename.empty()) {
        image_file.reset(new File(image_fd, unix_file::kCheckSafeUsage));
        // Empty the file in case it already exists.
        if (image_file != nullptr) {
          TEMP_FAILURE_RETRY(image_file->SetLength(0));
          TEMP_FAILURE_RETRY(image_file->Flush());
        }
      } else {
        LOG(ERROR) << "image fd " << image_fd << " name " << image_filename;
      }
    } else {
      image_file.reset(OS::CreateEmptyFile(image_filename.c_str()));
    }

    if (image_file == nullptr) {
      LOG(ERROR) << "Failed to open image file " << image_filename;
      return false;
    }

    if (!compiler_options_.IsAppImage() && fchmod(image_file->Fd(), 0644) != 0) {
      PLOG(ERROR) << "Failed to make image file world readable: " << image_filename;
      return EXIT_FAILURE;
    }

    // Image data size excludes the bitmap and the header.
    ImageHeader* const image_header = reinterpret_cast<ImageHeader*>(image_info.image_.Begin());
    ArrayRef<const uint8_t> raw_image_data(image_info.image_.Begin() + sizeof(ImageHeader),
                                           image_header->GetImageSize() - sizeof(ImageHeader));

    CHECK_EQ(image_header->storage_mode_, image_storage_mode_);
    std::vector<uint8_t> compressed_data;
    ArrayRef<const uint8_t> image_data =
        MaybeCompressData(raw_image_data, image_storage_mode_, &compressed_data);
    image_header->data_size_ = image_data.size();  // Fill in the data size.

    // Write out the image + fields + methods.
    if (!image_file->PwriteFully(image_data.data(), image_data.size(), sizeof(ImageHeader))) {
      PLOG(ERROR) << "Failed to write image file data " << image_filename;
      return false;
    }

    // Write out the image bitmap at the page aligned start of the image end, also uncompressed for
    // convenience.
    const ImageSection& bitmap_section = image_header->GetImageBitmapSection();
    // Align up since data size may be unaligned if the image is compressed.
    size_t bitmap_position_in_file = RoundUp(sizeof(ImageHeader) + image_data.size(), kPageSize);
    if (image_storage_mode_ == ImageHeader::kDefaultStorageMode) {
      CHECK_EQ(bitmap_position_in_file, bitmap_section.Offset());
    }
    if (!image_file->PwriteFully(image_info.image_bitmap_->Begin(),
                                 bitmap_section.Size(),
                                 bitmap_position_in_file)) {
      PLOG(ERROR) << "Failed to write image file bitmap " << image_filename;
      return false;
    }

    int err = image_file->Flush();
    if (err < 0) {
      PLOG(ERROR) << "Failed to flush image file " << image_filename << " with result " << err;
      return false;
    }

    // Calculate the image checksum.
    uint32_t image_checksum = adler32(0L, Z_NULL, 0);
    image_checksum = adler32(image_checksum,
                             reinterpret_cast<const uint8_t*>(image_header),
                             sizeof(ImageHeader));
    image_checksum = adler32(image_checksum, image_data.data(), image_data.size());
    image_checksum = adler32(image_checksum,
                             reinterpret_cast<const uint8_t*>(image_info.image_bitmap_->Begin()),
                             bitmap_section.Size());
    image_header->SetImageChecksum(image_checksum);

    if (VLOG_IS_ON(compiler)) {
      size_t separately_written_section_size = bitmap_section.Size() + sizeof(ImageHeader);

      size_t total_uncompressed_size = raw_image_data.size() + separately_written_section_size,
             total_compressed_size   = image_data.size() + separately_written_section_size;

      VLOG(compiler) << "Dex2Oat:uncompressedImageSize = " << total_uncompressed_size;
      if (total_uncompressed_size != total_compressed_size) {
        VLOG(compiler) << "Dex2Oat:compressedImageSize = " << total_compressed_size;
      }
    }

    CHECK_EQ(bitmap_position_in_file + bitmap_section.Size(),
             static_cast<size_t>(image_file->GetLength()));

    // Write header last in case the compiler gets killed in the middle of image writing.
    // We do not want to have a corrupted image with a valid header.
    // Delay the writing of the primary image header until after writing secondary images.
    if (i == 0u) {
      primary_image_file = std::move(image_file);
    } else {
      if (!image_file.WriteHeaderAndClose(image_filename, image_header)) {
        return false;
      }
      // Update the primary image checksum with the secondary image checksum.
      primary_header->SetImageChecksum(primary_header->GetImageChecksum() ^ image_checksum);
    }
  }
  DCHECK(primary_image_file != nullptr);
  if (!primary_image_file.WriteHeaderAndClose(image_filenames[0], primary_header)) {
    return false;
  }

  return true;
}

void ImageWriter::SetImageOffset(mirror::Object* object, size_t offset) {
  DCHECK(object != nullptr);
  DCHECK_NE(offset, 0U);

  // The object is already deflated from when we set the bin slot. Just overwrite the lock word.
  object->SetLockWord(LockWord::FromForwardingAddress(offset), false);
  DCHECK_EQ(object->GetLockWord(false).ReadBarrierState(), 0u);
  DCHECK(IsImageOffsetAssigned(object));
}

void ImageWriter::UpdateImageOffset(mirror::Object* obj, uintptr_t offset) {
  DCHECK(IsImageOffsetAssigned(obj)) << obj << " " << offset;
  obj->SetLockWord(LockWord::FromForwardingAddress(offset), false);
  DCHECK_EQ(obj->GetLockWord(false).ReadBarrierState(), 0u);
}

void ImageWriter::AssignImageOffset(mirror::Object* object, ImageWriter::BinSlot bin_slot) {
  DCHECK(object != nullptr);
  DCHECK_NE(image_objects_offset_begin_, 0u);

  size_t oat_index = GetOatIndex(object);
  ImageInfo& image_info = GetImageInfo(oat_index);
  size_t bin_slot_offset = image_info.GetBinSlotOffset(bin_slot.GetBin());
  size_t new_offset = bin_slot_offset + bin_slot.GetIndex();
  DCHECK_ALIGNED(new_offset, kObjectAlignment);

  SetImageOffset(object, new_offset);
  DCHECK_LT(new_offset, image_info.image_end_);
}

bool ImageWriter::IsImageOffsetAssigned(mirror::Object* object) const {
  // Will also return true if the bin slot was assigned since we are reusing the lock word.
  DCHECK(object != nullptr);
  return object->GetLockWord(false).GetState() == LockWord::kForwardingAddress;
}

size_t ImageWriter::GetImageOffset(mirror::Object* object) const {
  DCHECK(object != nullptr);
  DCHECK(IsImageOffsetAssigned(object));
  LockWord lock_word = object->GetLockWord(false);
  size_t offset = lock_word.ForwardingAddress();
  size_t oat_index = GetOatIndex(object);
  const ImageInfo& image_info = GetImageInfo(oat_index);
  DCHECK_LT(offset, image_info.image_end_);
  return offset;
}

void ImageWriter::SetImageBinSlot(mirror::Object* object, BinSlot bin_slot) {
  DCHECK(object != nullptr);
  DCHECK(!IsImageOffsetAssigned(object));
  DCHECK(!IsImageBinSlotAssigned(object));

  // Before we stomp over the lock word, save the hash code for later.
  LockWord lw(object->GetLockWord(false));
  switch (lw.GetState()) {
    case LockWord::kFatLocked:
      FALLTHROUGH_INTENDED;
    case LockWord::kThinLocked: {
      std::ostringstream oss;
      bool thin = (lw.GetState() == LockWord::kThinLocked);
      oss << (thin ? "Thin" : "Fat")
          << " locked object " << object << "(" << object->PrettyTypeOf()
          << ") found during object copy";
      if (thin) {
        oss << ". Lock owner:" << lw.ThinLockOwner();
      }
      LOG(FATAL) << oss.str();
      break;
    }
    case LockWord::kUnlocked:
      // No hash, don't need to save it.
      break;
    case LockWord::kHashCode:
      DCHECK(saved_hashcode_map_.find(object) == saved_hashcode_map_.end());
      saved_hashcode_map_.emplace(object, lw.GetHashCode());
      break;
    default:
      LOG(FATAL) << "Unreachable.";
      UNREACHABLE();
  }
  object->SetLockWord(LockWord::FromForwardingAddress(bin_slot.Uint32Value()), false);
  DCHECK_EQ(object->GetLockWord(false).ReadBarrierState(), 0u);
  DCHECK(IsImageBinSlotAssigned(object));
}

void ImageWriter::PrepareDexCacheArraySlots() {
  // Prepare dex cache array starts based on the ordering specified in the CompilerOptions.
  // Set the slot size early to avoid DCHECK() failures in IsImageBinSlotAssigned()
  // when AssignImageBinSlot() assigns their indexes out or order.
  for (const DexFile* dex_file : compiler_options_.GetDexFilesForOatFile()) {
    auto it = dex_file_oat_index_map_.find(dex_file);
    DCHECK(it != dex_file_oat_index_map_.end()) << dex_file->GetLocation();
    ImageInfo& image_info = GetImageInfo(it->second);
    image_info.dex_cache_array_starts_.Put(
        dex_file, image_info.GetBinSlotSize(Bin::kDexCacheArray));
    DexCacheArraysLayout layout(target_ptr_size_, dex_file);
    image_info.IncrementBinSlotSize(Bin::kDexCacheArray, layout.Size());
  }

  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  Thread* const self = Thread::Current();
  ReaderMutexLock mu(self, *Locks::dex_lock_);
  for (const ClassLinker::DexCacheData& data : class_linker->GetDexCachesData()) {
    ObjPtr<mirror::DexCache> dex_cache =
        ObjPtr<mirror::DexCache>::DownCast(self->DecodeJObject(data.weak_root));
    if (dex_cache == nullptr || IsInBootImage(dex_cache.Ptr())) {
      continue;
    }
    const DexFile* dex_file = dex_cache->GetDexFile();
    CHECK(dex_file_oat_index_map_.find(dex_file) != dex_file_oat_index_map_.end())
        << "Dex cache should have been pruned " << dex_file->GetLocation()
        << "; possibly in class path";
    DexCacheArraysLayout layout(target_ptr_size_, dex_file);
    DCHECK(layout.Valid());
    size_t oat_index = GetOatIndexForDexCache(dex_cache);
    ImageInfo& image_info = GetImageInfo(oat_index);
    uint32_t start = image_info.dex_cache_array_starts_.Get(dex_file);
    DCHECK_EQ(dex_file->NumTypeIds() != 0u, dex_cache->GetResolvedTypes() != nullptr);
    AddDexCacheArrayRelocation(dex_cache->GetResolvedTypes(),
                               start + layout.TypesOffset(),
                               oat_index);
    DCHECK_EQ(dex_file->NumMethodIds() != 0u, dex_cache->GetResolvedMethods() != nullptr);
    AddDexCacheArrayRelocation(dex_cache->GetResolvedMethods(),
                               start + layout.MethodsOffset(),
                               oat_index);
    DCHECK_EQ(dex_file->NumFieldIds() != 0u, dex_cache->GetResolvedFields() != nullptr);
    AddDexCacheArrayRelocation(dex_cache->GetResolvedFields(),
                               start + layout.FieldsOffset(),
                               oat_index);
    DCHECK_EQ(dex_file->NumStringIds() != 0u, dex_cache->GetStrings() != nullptr);
    AddDexCacheArrayRelocation(dex_cache->GetStrings(), start + layout.StringsOffset(), oat_index);

    AddDexCacheArrayRelocation(dex_cache->GetResolvedMethodTypes(),
                               start + layout.MethodTypesOffset(),
                               oat_index);
    AddDexCacheArrayRelocation(dex_cache->GetResolvedCallSites(),
                                start + layout.CallSitesOffset(),
                                oat_index);

    // Preresolved strings aren't part of the special layout.
    GcRoot<mirror::String>* preresolved_strings = dex_cache->GetPreResolvedStrings();
    if (preresolved_strings != nullptr) {
      DCHECK(!IsInBootImage(preresolved_strings));
      // Add the array to the metadata section.
      const size_t count = dex_cache->NumPreResolvedStrings();
      auto bin = BinTypeForNativeRelocationType(NativeObjectRelocationType::kGcRootPointer);
      for (size_t i = 0; i < count; ++i) {
        native_object_relocations_.emplace(&preresolved_strings[i],
            NativeObjectRelocation { oat_index,
                                     image_info.GetBinSlotSize(bin),
                                     NativeObjectRelocationType::kGcRootPointer });
        image_info.IncrementBinSlotSize(bin, sizeof(GcRoot<mirror::Object>));
      }
    }
  }
}

void ImageWriter::AddDexCacheArrayRelocation(void* array,
                                             size_t offset,
                                             size_t oat_index) {
  if (array != nullptr) {
    DCHECK(!IsInBootImage(array));
    native_object_relocations_.emplace(array,
        NativeObjectRelocation { oat_index, offset, NativeObjectRelocationType::kDexCacheArray });
  }
}

void ImageWriter::AddMethodPointerArray(mirror::PointerArray* arr) {
  DCHECK(arr != nullptr);
  if (kIsDebugBuild) {
    for (size_t i = 0, len = arr->GetLength(); i < len; i++) {
      ArtMethod* method = arr->GetElementPtrSize<ArtMethod*>(i, target_ptr_size_);
      if (method != nullptr && !method->IsRuntimeMethod()) {
        ObjPtr<mirror::Class> klass = method->GetDeclaringClass();
        CHECK(klass == nullptr || KeepClass(klass))
            << Class::PrettyClass(klass) << " should be a kept class";
      }
    }
  }
  // kBinArtMethodClean picked arbitrarily, just required to differentiate between ArtFields and
  // ArtMethods.
  pointer_arrays_.emplace(arr, Bin::kArtMethodClean);
}

void ImageWriter::AssignImageBinSlot(mirror::Object* object, size_t oat_index) {
  DCHECK(object != nullptr);
  size_t object_size = object->SizeOf();

  // The magic happens here. We segregate objects into different bins based
  // on how likely they are to get dirty at runtime.
  //
  // Likely-to-dirty objects get packed together into the same bin so that
  // at runtime their page dirtiness ratio (how many dirty objects a page has) is
  // maximized.
  //
  // This means more pages will stay either clean or shared dirty (with zygote) and
  // the app will use less of its own (private) memory.
  Bin bin = Bin::kRegular;

  if (kBinObjects) {
    //
    // Changing the bin of an object is purely a memory-use tuning.
    // It has no change on runtime correctness.
    //
    // Memory analysis has determined that the following types of objects get dirtied
    // the most:
    //
    // * Dex cache arrays are stored in a special bin. The arrays for each dex cache have
    //   a fixed layout which helps improve generated code (using PC-relative addressing),
    //   so we pre-calculate their offsets separately in PrepareDexCacheArraySlots().
    //   Since these arrays are huge, most pages do not overlap other objects and it's not
    //   really important where they are for the clean/dirty separation. Due to their
    //   special PC-relative addressing, we arbitrarily keep them at the end.
    // * Class'es which are verified [their clinit runs only at runtime]
    //   - classes in general [because their static fields get overwritten]
    //   - initialized classes with all-final statics are unlikely to be ever dirty,
    //     so bin them separately
    // * Art Methods that are:
    //   - native [their native entry point is not looked up until runtime]
    //   - have declaring classes that aren't initialized
    //            [their interpreter/quick entry points are trampolines until the class
    //             becomes initialized]
    //
    // We also assume the following objects get dirtied either never or extremely rarely:
    //  * Strings (they are immutable)
    //  * Art methods that aren't native and have initialized declared classes
    //
    // We assume that "regular" bin objects are highly unlikely to become dirtied,
    // so packing them together will not result in a noticeably tighter dirty-to-clean ratio.
    //
    if (object->IsClass()) {
      bin = Bin::kClassVerified;
      mirror::Class* klass = object->AsClass();

      // Add non-embedded vtable to the pointer array table if there is one.
      auto* vtable = klass->GetVTable();
      if (vtable != nullptr) {
        AddMethodPointerArray(vtable);
      }
      auto* iftable = klass->GetIfTable();
      if (iftable != nullptr) {
        for (int32_t i = 0; i < klass->GetIfTableCount(); ++i) {
          if (iftable->GetMethodArrayCount(i) > 0) {
            AddMethodPointerArray(iftable->GetMethodArray(i));
          }
        }
      }

      // Move known dirty objects into their own sections. This includes:
      //   - classes with dirty static fields.
      if (dirty_image_objects_ != nullptr &&
          dirty_image_objects_->find(klass->PrettyDescriptor()) != dirty_image_objects_->end()) {
        bin = Bin::kKnownDirty;
      } else if (klass->GetStatus() == ClassStatus::kInitialized) {
        bin = Bin::kClassInitialized;

        // If the class's static fields are all final, put it into a separate bin
        // since it's very likely it will stay clean.
        uint32_t num_static_fields = klass->NumStaticFields();
        if (num_static_fields == 0) {
          bin = Bin::kClassInitializedFinalStatics;
        } else {
          // Maybe all the statics are final?
          bool all_final = true;
          for (uint32_t i = 0; i < num_static_fields; ++i) {
            ArtField* field = klass->GetStaticField(i);
            if (!field->IsFinal()) {
              all_final = false;
              break;
            }
          }

          if (all_final) {
            bin = Bin::kClassInitializedFinalStatics;
          }
        }
      }
    } else if (object->GetClass<kVerifyNone>()->IsStringClass()) {
      bin = Bin::kString;  // Strings are almost always immutable (except for object header).
    } else if (object->GetClass<kVerifyNone>() == GetClassRoot<mirror::Object>()) {
      // Instance of java lang object, probably a lock object. This means it will be dirty when we
      // synchronize on it.
      bin = Bin::kMiscDirty;
    } else if (object->IsDexCache()) {
      // Dex file field becomes dirty when the image is loaded.
      bin = Bin::kMiscDirty;
    }
    // else bin = kBinRegular
  }

  // Assign the oat index too.
  DCHECK(oat_index_map_.find(object) == oat_index_map_.end());
  oat_index_map_.emplace(object, oat_index);

  ImageInfo& image_info = GetImageInfo(oat_index);

  size_t offset_delta = RoundUp(object_size, kObjectAlignment);  // 64-bit alignment
  // How many bytes the current bin is at (aligned).
  size_t current_offset = image_info.GetBinSlotSize(bin);
  // Move the current bin size up to accommodate the object we just assigned a bin slot.
  image_info.IncrementBinSlotSize(bin, offset_delta);

  BinSlot new_bin_slot(bin, current_offset);
  SetImageBinSlot(object, new_bin_slot);

  image_info.IncrementBinSlotCount(bin, 1u);

  // Grow the image closer to the end by the object we just assigned.
  image_info.image_end_ += offset_delta;
}

bool ImageWriter::WillMethodBeDirty(ArtMethod* m) const {
  if (m->IsNative()) {
    return true;
  }
  ObjPtr<mirror::Class> declaring_class = m->GetDeclaringClass();
  // Initialized is highly unlikely to dirty since there's no entry points to mutate.
  return declaring_class == nullptr || declaring_class->GetStatus() != ClassStatus::kInitialized;
}

bool ImageWriter::IsImageBinSlotAssigned(mirror::Object* object) const {
  DCHECK(object != nullptr);

  // We always stash the bin slot into a lockword, in the 'forwarding address' state.
  // If it's in some other state, then we haven't yet assigned an image bin slot.
  if (object->GetLockWord(false).GetState() != LockWord::kForwardingAddress) {
    return false;
  } else if (kIsDebugBuild) {
    LockWord lock_word = object->GetLockWord(false);
    size_t offset = lock_word.ForwardingAddress();
    BinSlot bin_slot(offset);
    size_t oat_index = GetOatIndex(object);
    const ImageInfo& image_info = GetImageInfo(oat_index);
    DCHECK_LT(bin_slot.GetIndex(), image_info.GetBinSlotSize(bin_slot.GetBin()))
        << "bin slot offset should not exceed the size of that bin";
  }
  return true;
}

ImageWriter::BinSlot ImageWriter::GetImageBinSlot(mirror::Object* object) const {
  DCHECK(object != nullptr);
  DCHECK(IsImageBinSlotAssigned(object));

  LockWord lock_word = object->GetLockWord(false);
  size_t offset = lock_word.ForwardingAddress();  // TODO: ForwardingAddress should be uint32_t
  DCHECK_LE(offset, std::numeric_limits<uint32_t>::max());

  BinSlot bin_slot(static_cast<uint32_t>(offset));
  size_t oat_index = GetOatIndex(object);
  const ImageInfo& image_info = GetImageInfo(oat_index);
  DCHECK_LT(bin_slot.GetIndex(), image_info.GetBinSlotSize(bin_slot.GetBin()));

  return bin_slot;
}

bool ImageWriter::AllocMemory() {
  for (ImageInfo& image_info : image_infos_) {
    const size_t length = RoundUp(image_info.CreateImageSections().first, kPageSize);

    std::string error_msg;
    image_info.image_ = MemMap::MapAnonymous("image writer image",
                                             length,
                                             PROT_READ | PROT_WRITE,
                                             /*low_4gb=*/ false,
                                             &error_msg);
    if (UNLIKELY(!image_info.image_.IsValid())) {
      LOG(ERROR) << "Failed to allocate memory for image file generation: " << error_msg;
      return false;
    }

    // Create the image bitmap, only needs to cover mirror object section which is up to image_end_.
    CHECK_LE(image_info.image_end_, length);
    image_info.image_bitmap_.reset(gc::accounting::ContinuousSpaceBitmap::Create(
        "image bitmap", image_info.image_.Begin(), RoundUp(image_info.image_end_, kPageSize)));
    if (image_info.image_bitmap_.get() == nullptr) {
      LOG(ERROR) << "Failed to allocate memory for image bitmap";
      return false;
    }
  }
  return true;
}

static bool IsBootClassLoaderClass(ObjPtr<mirror::Class> klass)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  return klass->GetClassLoader() == nullptr;
}

bool ImageWriter::IsBootClassLoaderNonImageClass(mirror::Class* klass) {
  return IsBootClassLoaderClass(klass) && !IsInBootImage(klass);
}

// This visitor follows the references of an instance, recursively then prune this class
// if a type of any field is pruned.
class ImageWriter::PruneObjectReferenceVisitor {
 public:
  PruneObjectReferenceVisitor(ImageWriter* image_writer,
                        bool* early_exit,
                        std::unordered_set<mirror::Object*>* visited,
                        bool* result)
      : image_writer_(image_writer), early_exit_(early_exit), visited_(visited), result_(result) {}

  ALWAYS_INLINE void VisitRootIfNonNull(
      mirror::CompressedReference<mirror::Object>* root ATTRIBUTE_UNUSED) const
      REQUIRES_SHARED(Locks::mutator_lock_) { }

  ALWAYS_INLINE void VisitRoot(
      mirror::CompressedReference<mirror::Object>* root ATTRIBUTE_UNUSED) const
      REQUIRES_SHARED(Locks::mutator_lock_) { }

  ALWAYS_INLINE void operator() (ObjPtr<mirror::Object> obj,
                                 MemberOffset offset,
                                 bool is_static ATTRIBUTE_UNUSED) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    mirror::Object* ref =
        obj->GetFieldObject<mirror::Object, kVerifyNone, kWithoutReadBarrier>(offset);
    if (ref == nullptr || visited_->find(ref) != visited_->end()) {
      return;
    }

    ObjPtr<mirror::ObjectArray<mirror::Class>> class_roots =
        Runtime::Current()->GetClassLinker()->GetClassRoots();
    ObjPtr<mirror::Class> klass = ref->IsClass() ? ref->AsClass() : ref->GetClass();
    if (klass == GetClassRoot<mirror::Method>(class_roots) ||
        klass == GetClassRoot<mirror::Constructor>(class_roots)) {
      // Prune all classes using reflection because the content they held will not be fixup.
      *result_ = true;
    }

    if (ref->IsClass()) {
      *result_ = *result_ ||
          image_writer_->PruneAppImageClassInternal(ref->AsClass(), early_exit_, visited_);
    } else {
      // Record the object visited in case of circular reference.
      visited_->emplace(ref);
      *result_ = *result_ ||
          image_writer_->PruneAppImageClassInternal(klass, early_exit_, visited_);
      ref->VisitReferences(*this, *this);
      // Clean up before exit for next call of this function.
      visited_->erase(ref);
    }
  }

  ALWAYS_INLINE void operator() (ObjPtr<mirror::Class> klass ATTRIBUTE_UNUSED,
                                 ObjPtr<mirror::Reference> ref) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    operator()(ref, mirror::Reference::ReferentOffset(), /* is_static */ false);
  }

  ALWAYS_INLINE bool GetResult() const {
    return result_;
  }

 private:
  ImageWriter* image_writer_;
  bool* early_exit_;
  std::unordered_set<mirror::Object*>* visited_;
  bool* const result_;
};


bool ImageWriter::PruneAppImageClass(ObjPtr<mirror::Class> klass) {
  bool early_exit = false;
  std::unordered_set<mirror::Object*> visited;
  return PruneAppImageClassInternal(klass, &early_exit, &visited);
}

bool ImageWriter::PruneAppImageClassInternal(
    ObjPtr<mirror::Class> klass,
    bool* early_exit,
    std::unordered_set<mirror::Object*>* visited) {
  DCHECK(early_exit != nullptr);
  DCHECK(visited != nullptr);
  DCHECK(compiler_options_.IsAppImage());
  if (klass == nullptr || IsInBootImage(klass.Ptr())) {
    return false;
  }
  auto found = prune_class_memo_.find(klass.Ptr());
  if (found != prune_class_memo_.end()) {
    // Already computed, return the found value.
    return found->second;
  }
  // Circular dependencies, return false but do not store the result in the memoization table.
  if (visited->find(klass.Ptr()) != visited->end()) {
    *early_exit = true;
    return false;
  }
  visited->emplace(klass.Ptr());
  bool result = IsBootClassLoaderClass(klass);
  std::string temp;
  // Prune if not an image class, this handles any broken sets of image classes such as having a
  // class in the set but not it's superclass.
  result = result || !compiler_options_.IsImageClass(klass->GetDescriptor(&temp));
  bool my_early_exit = false;  // Only for ourselves, ignore caller.
  // Remove classes that failed to verify since we don't want to have java.lang.VerifyError in the
  // app image.
  if (klass->IsErroneous()) {
    result = true;
  } else {
    ObjPtr<mirror::ClassExt> ext(klass->GetExtData());
    CHECK(ext.IsNull() || ext->GetVerifyError() == nullptr) << klass->PrettyClass();
  }
  if (!result) {
    // Check interfaces since these wont be visited through VisitReferences.)
    mirror::IfTable* if_table = klass->GetIfTable();
    for (size_t i = 0, num_interfaces = klass->GetIfTableCount(); i < num_interfaces; ++i) {
      result = result || PruneAppImageClassInternal(if_table->GetInterface(i),
                                                    &my_early_exit,
                                                    visited);
    }
  }
  if (klass->IsObjectArrayClass()) {
    result = result || PruneAppImageClassInternal(klass->GetComponentType(),
                                                  &my_early_exit,
                                                  visited);
  }
  // Check static fields and their classes.
  if (klass->IsResolved() && klass->NumReferenceStaticFields() != 0) {
    size_t num_static_fields = klass->NumReferenceStaticFields();
    // Presumably GC can happen when we are cross compiling, it should not cause performance
    // problems to do pointer size logic.
    MemberOffset field_offset = klass->GetFirstReferenceStaticFieldOffset(
        Runtime::Current()->GetClassLinker()->GetImagePointerSize());
    for (size_t i = 0u; i < num_static_fields; ++i) {
      mirror::Object* ref = klass->GetFieldObject<mirror::Object>(field_offset);
      if (ref != nullptr) {
        if (ref->IsClass()) {
          result = result || PruneAppImageClassInternal(ref->AsClass(),
                                                        &my_early_exit,
                                                        visited);
        } else {
          mirror::Class* type = ref->GetClass();
          result = result || PruneAppImageClassInternal(type,
                                                        &my_early_exit,
                                                        visited);
          if (!result) {
            // For non-class case, also go through all the types mentioned by it's fields'
            // references recursively to decide whether to keep this class.
            bool tmp = false;
            PruneObjectReferenceVisitor visitor(this, &my_early_exit, visited, &tmp);
            ref->VisitReferences(visitor, visitor);
            result = result || tmp;
          }
        }
      }
      field_offset = MemberOffset(field_offset.Uint32Value() +
                                  sizeof(mirror::HeapReference<mirror::Object>));
    }
  }
  result = result || PruneAppImageClassInternal(klass->GetSuperClass(),
                                                &my_early_exit,
                                                visited);
  // Remove the class if the dex file is not in the set of dex files. This happens for classes that
  // are from uses-library if there is no profile. b/30688277
  mirror::DexCache* dex_cache = klass->GetDexCache();
  if (dex_cache != nullptr) {
    result = result ||
        dex_file_oat_index_map_.find(dex_cache->GetDexFile()) == dex_file_oat_index_map_.end();
  }
  // Erase the element we stored earlier since we are exiting the function.
  auto it = visited->find(klass.Ptr());
  DCHECK(it != visited->end());
  visited->erase(it);
  // Only store result if it is true or none of the calls early exited due to circular
  // dependencies. If visited is empty then we are the root caller, in this case the cycle was in
  // a child call and we can remember the result.
  if (result == true || !my_early_exit || visited->empty()) {
    prune_class_memo_[klass.Ptr()] = result;
  }
  *early_exit |= my_early_exit;
  return result;
}

bool ImageWriter::KeepClass(ObjPtr<mirror::Class> klass) {
  if (klass == nullptr) {
    return false;
  }
  if (!compiler_options_.IsBootImage() &&
      Runtime::Current()->GetHeap()->ObjectIsInBootImageSpace(klass)) {
    // Already in boot image, return true.
    return true;
  }
  std::string temp;
  if (!compiler_options_.IsImageClass(klass->GetDescriptor(&temp))) {
    return false;
  }
  if (compiler_options_.IsAppImage()) {
    // For app images, we need to prune boot loader classes that are not in the boot image since
    // these may have already been loaded when the app image is loaded.
    // Keep classes in the boot image space since we don't want to re-resolve these.
    return !PruneAppImageClass(klass);
  }
  return true;
}

class ImageWriter::PruneClassesVisitor : public ClassVisitor {
 public:
  PruneClassesVisitor(ImageWriter* image_writer, ObjPtr<mirror::ClassLoader> class_loader)
      : image_writer_(image_writer),
        class_loader_(class_loader),
        classes_to_prune_(),
        defined_class_count_(0u) { }

  bool operator()(ObjPtr<mirror::Class> klass) override REQUIRES_SHARED(Locks::mutator_lock_) {
    if (!image_writer_->KeepClass(klass.Ptr())) {
      classes_to_prune_.insert(klass.Ptr());
      if (klass->GetClassLoader() == class_loader_) {
        ++defined_class_count_;
      }
    }
    return true;
  }

  size_t Prune() REQUIRES_SHARED(Locks::mutator_lock_) {
    ClassTable* class_table =
        Runtime::Current()->GetClassLinker()->ClassTableForClassLoader(class_loader_);
    for (mirror::Class* klass : classes_to_prune_) {
      std::string storage;
      const char* descriptor = klass->GetDescriptor(&storage);
      bool result = class_table->Remove(descriptor);
      DCHECK(result);
      DCHECK(!class_table->Remove(descriptor)) << descriptor;
    }
    return defined_class_count_;
  }

 private:
  ImageWriter* const image_writer_;
  const ObjPtr<mirror::ClassLoader> class_loader_;
  std::unordered_set<mirror::Class*> classes_to_prune_;
  size_t defined_class_count_;
};

class ImageWriter::PruneClassLoaderClassesVisitor : public ClassLoaderVisitor {
 public:
  explicit PruneClassLoaderClassesVisitor(ImageWriter* image_writer)
      : image_writer_(image_writer), removed_class_count_(0) {}

  void Visit(ObjPtr<mirror::ClassLoader> class_loader) override
      REQUIRES_SHARED(Locks::mutator_lock_) {
    PruneClassesVisitor classes_visitor(image_writer_, class_loader);
    ClassTable* class_table =
        Runtime::Current()->GetClassLinker()->ClassTableForClassLoader(class_loader);
    class_table->Visit(classes_visitor);
    removed_class_count_ += classes_visitor.Prune();
  }

  size_t GetRemovedClassCount() const {
    return removed_class_count_;
  }

 private:
  ImageWriter* const image_writer_;
  size_t removed_class_count_;
};

void ImageWriter::VisitClassLoaders(ClassLoaderVisitor* visitor) {
  WriterMutexLock mu(Thread::Current(), *Locks::classlinker_classes_lock_);
  visitor->Visit(nullptr);  // Visit boot class loader.
  Runtime::Current()->GetClassLinker()->VisitClassLoaders(visitor);
}

void ImageWriter::PruneDexCache(ObjPtr<mirror::DexCache> dex_cache,
                                ObjPtr<mirror::ClassLoader> class_loader) {
  Runtime* runtime = Runtime::Current();
  ClassLinker* class_linker = runtime->GetClassLinker();
  const DexFile& dex_file = *dex_cache->GetDexFile();
  // Prune methods.
  dex::TypeIndex last_class_idx;  // Initialized to invalid index.
  ObjPtr<mirror::Class> last_class = nullptr;
  mirror::MethodDexCacheType* resolved_methods = dex_cache->GetResolvedMethods();
  for (size_t slot_idx = 0, num = dex_cache->NumResolvedMethods(); slot_idx != num; ++slot_idx) {
    auto pair =
        mirror::DexCache::GetNativePairPtrSize(resolved_methods, slot_idx, target_ptr_size_);
    uint32_t stored_index = pair.index;
    ArtMethod* method = pair.object;
    if (method == nullptr) {
      continue;  // Empty entry.
    }
    // Check if the referenced class is in the image. Note that we want to check the referenced
    // class rather than the declaring class to preserve the semantics, i.e. using a MethodId
    // results in resolving the referenced class and that can for example throw OOME.
    const DexFile::MethodId& method_id = dex_file.GetMethodId(stored_index);
    if (method_id.class_idx_ != last_class_idx) {
      last_class_idx = method_id.class_idx_;
      last_class = class_linker->LookupResolvedType(last_class_idx, dex_cache, class_loader);
      if (last_class != nullptr && !KeepClass(last_class)) {
        last_class = nullptr;
      }
    }
    if (last_class == nullptr) {
      dex_cache->ClearResolvedMethod(stored_index, target_ptr_size_);
    }
  }
  // Prune fields.
  mirror::FieldDexCacheType* resolved_fields = dex_cache->GetResolvedFields();
  last_class_idx = dex::TypeIndex();  // Initialized to invalid index.
  last_class = nullptr;
  for (size_t slot_idx = 0, num = dex_cache->NumResolvedFields(); slot_idx != num; ++slot_idx) {
    auto pair = mirror::DexCache::GetNativePairPtrSize(resolved_fields, slot_idx, target_ptr_size_);
    uint32_t stored_index = pair.index;
    ArtField* field = pair.object;
    if (field == nullptr) {
      continue;  // Empty entry.
    }
    // Check if the referenced class is in the image. Note that we want to check the referenced
    // class rather than the declaring class to preserve the semantics, i.e. using a FieldId
    // results in resolving the referenced class and that can for example throw OOME.
    const DexFile::FieldId& field_id = dex_file.GetFieldId(stored_index);
    if (field_id.class_idx_ != last_class_idx) {
      last_class_idx = field_id.class_idx_;
      last_class = class_linker->LookupResolvedType(last_class_idx, dex_cache, class_loader);
      if (last_class != nullptr && !KeepClass(last_class)) {
        last_class = nullptr;
      }
    }
    if (last_class == nullptr) {
      dex_cache->ClearResolvedField(stored_index, target_ptr_size_);
    }
  }
  // Prune types.
  for (size_t slot_idx = 0, num = dex_cache->NumResolvedTypes(); slot_idx != num; ++slot_idx) {
    mirror::TypeDexCachePair pair =
        dex_cache->GetResolvedTypes()[slot_idx].load(std::memory_order_relaxed);
    uint32_t stored_index = pair.index;
    ObjPtr<mirror::Class> klass = pair.object.Read();
    if (klass != nullptr && !KeepClass(klass)) {
      dex_cache->ClearResolvedType(dex::TypeIndex(stored_index));
    }
  }
  // Strings do not need pruning.
}

void ImageWriter::PreloadDexCache(ObjPtr<mirror::DexCache> dex_cache,
                                  ObjPtr<mirror::ClassLoader> class_loader) {
  // To ensure deterministic contents of the hash-based arrays, each slot shall contain
  // the candidate with the lowest index. As we're processing entries in increasing index
  // order, this means trying to look up the entry for the current index if the slot is
  // empty or if it contains a higher index.

  Runtime* runtime = Runtime::Current();
  ClassLinker* class_linker = runtime->GetClassLinker();
  const DexFile& dex_file = *dex_cache->GetDexFile();
  // Preload the methods array and make the contents deterministic.
  mirror::MethodDexCacheType* resolved_methods = dex_cache->GetResolvedMethods();
  dex::TypeIndex last_class_idx;  // Initialized to invalid index.
  ObjPtr<mirror::Class> last_class = nullptr;
  for (size_t i = 0, num = dex_cache->GetDexFile()->NumMethodIds(); i != num; ++i) {
    uint32_t slot_idx = dex_cache->MethodSlotIndex(i);
    auto pair =
        mirror::DexCache::GetNativePairPtrSize(resolved_methods, slot_idx, target_ptr_size_);
    uint32_t stored_index = pair.index;
    ArtMethod* method = pair.object;
    if (method != nullptr && i > stored_index) {
      continue;  // Already checked.
    }
    // Check if the referenced class is in the image. Note that we want to check the referenced
    // class rather than the declaring class to preserve the semantics, i.e. using a MethodId
    // results in resolving the referenced class and that can for example throw OOME.
    const DexFile::MethodId& method_id = dex_file.GetMethodId(i);
    if (method_id.class_idx_ != last_class_idx) {
      last_class_idx = method_id.class_idx_;
      last_class = class_linker->LookupResolvedType(last_class_idx, dex_cache, class_loader);
    }
    if (method == nullptr || i < stored_index) {
      if (last_class != nullptr) {
        // Try to resolve the method with the class linker, which will insert
        // it into the dex cache if successful.
        method = class_linker->FindResolvedMethod(last_class, dex_cache, class_loader, i);
        DCHECK(method == nullptr || dex_cache->GetResolvedMethod(i, target_ptr_size_) == method);
      }
    } else {
      DCHECK_EQ(i, stored_index);
      DCHECK(last_class != nullptr);
    }
  }
  // Preload the fields array and make the contents deterministic.
  mirror::FieldDexCacheType* resolved_fields = dex_cache->GetResolvedFields();
  last_class_idx = dex::TypeIndex();  // Initialized to invalid index.
  last_class = nullptr;
  for (size_t i = 0, end = dex_file.NumFieldIds(); i < end; ++i) {
    uint32_t slot_idx = dex_cache->FieldSlotIndex(i);
    auto pair = mirror::DexCache::GetNativePairPtrSize(resolved_fields, slot_idx, target_ptr_size_);
    uint32_t stored_index = pair.index;
    ArtField* field = pair.object;
    if (field != nullptr && i > stored_index) {
      continue;  // Already checked.
    }
    // Check if the referenced class is in the image. Note that we want to check the referenced
    // class rather than the declaring class to preserve the semantics, i.e. using a FieldId
    // results in resolving the referenced class and that can for example throw OOME.
    const DexFile::FieldId& field_id = dex_file.GetFieldId(i);
    if (field_id.class_idx_ != last_class_idx) {
      last_class_idx = field_id.class_idx_;
      last_class = class_linker->LookupResolvedType(last_class_idx, dex_cache, class_loader);
      if (last_class != nullptr && !KeepClass(last_class)) {
        last_class = nullptr;
      }
    }
    if (field == nullptr || i < stored_index) {
      if (last_class != nullptr) {
        // Try to resolve the field with the class linker, which will insert
        // it into the dex cache if successful.
        field = class_linker->FindResolvedFieldJLS(last_class, dex_cache, class_loader, i);
        DCHECK(field == nullptr || dex_cache->GetResolvedField(i, target_ptr_size_) == field);
      }
    } else {
      DCHECK_EQ(i, stored_index);
      DCHECK(last_class != nullptr);
    }
  }
  // Preload the types array and make the contents deterministic.
  // This is done after fields and methods as their lookup can touch the types array.
  for (size_t i = 0, end = dex_cache->GetDexFile()->NumTypeIds(); i < end; ++i) {
    dex::TypeIndex type_idx(i);
    uint32_t slot_idx = dex_cache->TypeSlotIndex(type_idx);
    mirror::TypeDexCachePair pair =
        dex_cache->GetResolvedTypes()[slot_idx].load(std::memory_order_relaxed);
    uint32_t stored_index = pair.index;
    ObjPtr<mirror::Class> klass = pair.object.Read();
    if (klass == nullptr || i < stored_index) {
      klass = class_linker->LookupResolvedType(type_idx, dex_cache, class_loader);
      DCHECK(klass == nullptr || dex_cache->GetResolvedType(type_idx) == klass);
    }
  }
  // Preload the strings array and make the contents deterministic.
  for (size_t i = 0, end = dex_cache->GetDexFile()->NumStringIds(); i < end; ++i) {
    dex::StringIndex string_idx(i);
    uint32_t slot_idx = dex_cache->StringSlotIndex(string_idx);
    mirror::StringDexCachePair pair =
        dex_cache->GetStrings()[slot_idx].load(std::memory_order_relaxed);
    uint32_t stored_index = pair.index;
    ObjPtr<mirror::String> string = pair.object.Read();
    if (string == nullptr || i < stored_index) {
      string = class_linker->LookupString(string_idx, dex_cache);
      DCHECK(string == nullptr || dex_cache->GetResolvedString(string_idx) == string);
    }
  }
}

void ImageWriter::PruneNonImageClasses() {
  Runtime* runtime = Runtime::Current();
  ClassLinker* class_linker = runtime->GetClassLinker();
  Thread* self = Thread::Current();
  ScopedAssertNoThreadSuspension sa(__FUNCTION__);

  // Prune uses-library dex caches. Only prune the uses-library dex caches since we want to make
  // sure the other ones don't get unloaded before the OatWriter runs.
  class_linker->VisitClassTables(
      [&](ClassTable* table) REQUIRES_SHARED(Locks::mutator_lock_) {
    table->RemoveStrongRoots(
        [&](GcRoot<mirror::Object> root) REQUIRES_SHARED(Locks::mutator_lock_) {
      ObjPtr<mirror::Object> obj = root.Read();
      if (obj->IsDexCache()) {
        // Return true if the dex file is not one of the ones in the map.
        return dex_file_oat_index_map_.find(obj->AsDexCache()->GetDexFile()) ==
            dex_file_oat_index_map_.end();
      }
      // Return false to avoid removing.
      return false;
    });
  });

  // Remove the undesired classes from the class roots.
  {
    PruneClassLoaderClassesVisitor class_loader_visitor(this);
    VisitClassLoaders(&class_loader_visitor);
    VLOG(compiler) << "Pruned " << class_loader_visitor.GetRemovedClassCount() << " classes";
  }

  // Clear references to removed classes from the DexCaches.
  std::vector<ObjPtr<mirror::DexCache>> dex_caches = FindDexCaches(self);
  for (ObjPtr<mirror::DexCache> dex_cache : dex_caches) {
    // Pass the class loader associated with the DexCache. This can either be
    // the app's `class_loader` or `nullptr` if boot class loader.
    PruneDexCache(dex_cache, IsInBootImage(dex_cache.Ptr()) ? nullptr : GetAppClassLoader());
  }

  // Drop the array class cache in the ClassLinker, as these are roots holding those classes live.
  class_linker->DropFindArrayClassCache();

  // Clear to save RAM.
  prune_class_memo_.clear();
}

std::vector<ObjPtr<mirror::DexCache>> ImageWriter::FindDexCaches(Thread* self) {
  std::vector<ObjPtr<mirror::DexCache>> dex_caches;
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  ReaderMutexLock mu2(self, *Locks::dex_lock_);
  dex_caches.reserve(class_linker->GetDexCachesData().size());
  for (const ClassLinker::DexCacheData& data : class_linker->GetDexCachesData()) {
    if (self->IsJWeakCleared(data.weak_root)) {
      continue;
    }
    dex_caches.push_back(self->DecodeJObject(data.weak_root)->AsDexCache());
  }
  return dex_caches;
}

void ImageWriter::CheckNonImageClassesRemoved() {
  auto visitor = [&](Object* obj) REQUIRES_SHARED(Locks::mutator_lock_) {
    if (obj->IsClass() && !IsInBootImage(obj)) {
      Class* klass = obj->AsClass();
      if (!KeepClass(klass)) {
        DumpImageClasses();
        CHECK(KeepClass(klass))
            << Runtime::Current()->GetHeap()->GetVerification()->FirstPathFromRootSet(klass);
      }
    }
  };
  gc::Heap* heap = Runtime::Current()->GetHeap();
  heap->VisitObjects(visitor);
}

void ImageWriter::DumpImageClasses() {
  for (const std::string& image_class : compiler_options_.GetImageClasses()) {
    LOG(INFO) << " " << image_class;
  }
}

mirror::String* ImageWriter::FindInternedString(mirror::String* string) {
  Thread* const self = Thread::Current();
  for (const ImageInfo& image_info : image_infos_) {
    ObjPtr<mirror::String> const found = image_info.intern_table_->LookupStrong(self, string);
    DCHECK(image_info.intern_table_->LookupWeak(self, string) == nullptr)
        << string->ToModifiedUtf8();
    if (found != nullptr) {
      return found.Ptr();
    }
  }
  if (!compiler_options_.IsBootImage()) {
    Runtime* const runtime = Runtime::Current();
    ObjPtr<mirror::String> found = runtime->GetInternTable()->LookupStrong(self, string);
    // If we found it in the runtime intern table it could either be in the boot image or interned
    // during app image compilation. If it was in the boot image return that, otherwise return null
    // since it belongs to another image space.
    if (found != nullptr && runtime->GetHeap()->ObjectIsInBootImageSpace(found.Ptr())) {
      return found.Ptr();
    }
    DCHECK(runtime->GetInternTable()->LookupWeak(self, string) == nullptr)
        << string->ToModifiedUtf8();
  }
  return nullptr;
}

ObjPtr<mirror::ObjectArray<mirror::Object>> ImageWriter::CollectDexCaches(Thread* self,
                                                                          size_t oat_index) const {
  std::unordered_set<const DexFile*> image_dex_files;
  for (auto& pair : dex_file_oat_index_map_) {
    const DexFile* image_dex_file = pair.first;
    size_t image_oat_index = pair.second;
    if (oat_index == image_oat_index) {
      image_dex_files.insert(image_dex_file);
    }
  }

  // build an Object[] of all the DexCaches used in the source_space_.
  // Since we can't hold the dex lock when allocating the dex_caches
  // ObjectArray, we lock the dex lock twice, first to get the number
  // of dex caches first and then lock it again to copy the dex
  // caches. We check that the number of dex caches does not change.
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  size_t dex_cache_count = 0;
  {
    ReaderMutexLock mu(self, *Locks::dex_lock_);
    // Count number of dex caches not in the boot image.
    for (const ClassLinker::DexCacheData& data : class_linker->GetDexCachesData()) {
      ObjPtr<mirror::DexCache> dex_cache =
          ObjPtr<mirror::DexCache>::DownCast(self->DecodeJObject(data.weak_root));
      if (dex_cache == nullptr) {
        continue;
      }
      const DexFile* dex_file = dex_cache->GetDexFile();
      if (!IsInBootImage(dex_cache.Ptr())) {
        dex_cache_count += image_dex_files.find(dex_file) != image_dex_files.end() ? 1u : 0u;
      }
    }
  }
  ObjPtr<ObjectArray<Object>> dex_caches = ObjectArray<Object>::Alloc(
      self, GetClassRoot<ObjectArray<Object>>(class_linker), dex_cache_count);
  CHECK(dex_caches != nullptr) << "Failed to allocate a dex cache array.";
  {
    ReaderMutexLock mu(self, *Locks::dex_lock_);
    size_t non_image_dex_caches = 0;
    // Re-count number of non image dex caches.
    for (const ClassLinker::DexCacheData& data : class_linker->GetDexCachesData()) {
      ObjPtr<mirror::DexCache> dex_cache =
          ObjPtr<mirror::DexCache>::DownCast(self->DecodeJObject(data.weak_root));
      if (dex_cache == nullptr) {
        continue;
      }
      const DexFile* dex_file = dex_cache->GetDexFile();
      if (!IsInBootImage(dex_cache.Ptr())) {
        non_image_dex_caches += image_dex_files.find(dex_file) != image_dex_files.end() ? 1u : 0u;
      }
    }
    CHECK_EQ(dex_cache_count, non_image_dex_caches)
        << "The number of non-image dex caches changed.";
    size_t i = 0;
    for (const ClassLinker::DexCacheData& data : class_linker->GetDexCachesData()) {
      ObjPtr<mirror::DexCache> dex_cache =
          ObjPtr<mirror::DexCache>::DownCast(self->DecodeJObject(data.weak_root));
      if (dex_cache == nullptr) {
        continue;
      }
      const DexFile* dex_file = dex_cache->GetDexFile();
      if (!IsInBootImage(dex_cache.Ptr()) &&
          image_dex_files.find(dex_file) != image_dex_files.end()) {
        dex_caches->Set<false>(i, dex_cache.Ptr());
        ++i;
      }
    }
  }
  return dex_caches;
}

ObjPtr<ObjectArray<Object>> ImageWriter::CreateImageRoots(
    size_t oat_index,
    Handle<mirror::ObjectArray<mirror::Object>> boot_image_live_objects) const {
  Runtime* runtime = Runtime::Current();
  ClassLinker* class_linker = runtime->GetClassLinker();
  Thread* self = Thread::Current();
  StackHandleScope<2> hs(self);

  Handle<ObjectArray<Object>> dex_caches(hs.NewHandle(CollectDexCaches(self, oat_index)));

  // build an Object[] of the roots needed to restore the runtime
  int32_t image_roots_size = ImageHeader::NumberOfImageRoots(compiler_options_.IsAppImage());
  Handle<ObjectArray<Object>> image_roots(hs.NewHandle(ObjectArray<Object>::Alloc(
      self, GetClassRoot<ObjectArray<Object>>(class_linker), image_roots_size)));
  image_roots->Set<false>(ImageHeader::kDexCaches, dex_caches.Get());
  image_roots->Set<false>(ImageHeader::kClassRoots, class_linker->GetClassRoots());
  image_roots->Set<false>(ImageHeader::kOomeWhenThrowingException,
                          runtime->GetPreAllocatedOutOfMemoryErrorWhenThrowingException());
  image_roots->Set<false>(ImageHeader::kOomeWhenThrowingOome,
                          runtime->GetPreAllocatedOutOfMemoryErrorWhenThrowingOOME());
  image_roots->Set<false>(ImageHeader::kOomeWhenHandlingStackOverflow,
                          runtime->GetPreAllocatedOutOfMemoryErrorWhenHandlingStackOverflow());
  image_roots->Set<false>(ImageHeader::kNoClassDefFoundError,
                          runtime->GetPreAllocatedNoClassDefFoundError());
  if (!compiler_options_.IsAppImage()) {
    DCHECK(boot_image_live_objects != nullptr);
    image_roots->Set<false>(ImageHeader::kBootImageLiveObjects, boot_image_live_objects.Get());
  } else {
    DCHECK(boot_image_live_objects == nullptr);
  }
  for (int32_t i = 0; i != image_roots_size; ++i) {
    if (compiler_options_.IsAppImage() && i == ImageHeader::kAppImageClassLoader) {
      // image_roots[ImageHeader::kAppImageClassLoader] will be set later for app image.
      continue;
    }
    CHECK(image_roots->Get(i) != nullptr);
  }
  return image_roots.Get();
}

mirror::Object* ImageWriter::TryAssignBinSlot(WorkStack& work_stack,
                                              mirror::Object* obj,
                                              size_t oat_index) {
  if (obj == nullptr || IsInBootImage(obj)) {
    // Object is null or already in the image, there is no work to do.
    return obj;
  }
  if (!IsImageBinSlotAssigned(obj)) {
    // We want to intern all strings but also assign offsets for the source string. Since the
    // pruning phase has already happened, if we intern a string to one in the image we still
    // end up copying an unreachable string.
    if (obj->IsString()) {
      // Need to check if the string is already interned in another image info so that we don't have
      // the intern tables of two different images contain the same string.
      mirror::String* interned = FindInternedString(obj->AsString());
      if (interned == nullptr) {
        // Not in another image space, insert to our table.
        interned =
            GetImageInfo(oat_index).intern_table_->InternStrongImageString(obj->AsString()).Ptr();
        DCHECK_EQ(interned, obj);
      }
    } else if (obj->IsDexCache()) {
      oat_index = GetOatIndexForDexCache(obj->AsDexCache());
    } else if (obj->IsClass()) {
      // Visit and assign offsets for fields and field arrays.
      mirror::Class* as_klass = obj->AsClass();
      mirror::DexCache* dex_cache = as_klass->GetDexCache();
      DCHECK(!as_klass->IsErroneous()) << as_klass->GetStatus();
      if (compiler_options_.IsAppImage()) {
        // Extra sanity, no boot loader classes should be left!
        CHECK(!IsBootClassLoaderClass(as_klass)) << as_klass->PrettyClass();
      }
      LengthPrefixedArray<ArtField>* fields[] = {
          as_klass->GetSFieldsPtr(), as_klass->GetIFieldsPtr(),
      };
      // Overwrite the oat index value since the class' dex cache is more accurate of where it
      // belongs.
      oat_index = GetOatIndexForDexCache(dex_cache);
      ImageInfo& image_info = GetImageInfo(oat_index);
      if (!compiler_options_.IsAppImage()) {
        // Note: Avoid locking to prevent lock order violations from root visiting;
        // image_info.class_table_ is only accessed from the image writer.
        image_info.class_table_->InsertWithoutLocks(as_klass);
      }
      for (LengthPrefixedArray<ArtField>* cur_fields : fields) {
        // Total array length including header.
        if (cur_fields != nullptr) {
          const size_t header_size = LengthPrefixedArray<ArtField>::ComputeSize(0);
          // Forward the entire array at once.
          auto it = native_object_relocations_.find(cur_fields);
          CHECK(it == native_object_relocations_.end()) << "Field array " << cur_fields
                                                  << " already forwarded";
          size_t offset = image_info.GetBinSlotSize(Bin::kArtField);
          DCHECK(!IsInBootImage(cur_fields));
          native_object_relocations_.emplace(
              cur_fields,
              NativeObjectRelocation {
                  oat_index, offset, NativeObjectRelocationType::kArtFieldArray
              });
          offset += header_size;
          // Forward individual fields so that we can quickly find where they belong.
          for (size_t i = 0, count = cur_fields->size(); i < count; ++i) {
            // Need to forward arrays separate of fields.
            ArtField* field = &cur_fields->At(i);
            auto it2 = native_object_relocations_.find(field);
            CHECK(it2 == native_object_relocations_.end()) << "Field at index=" << i
                << " already assigned " << field->PrettyField() << " static=" << field->IsStatic();
            DCHECK(!IsInBootImage(field));
            native_object_relocations_.emplace(
                field,
                NativeObjectRelocation { oat_index,
                                         offset,
                                         NativeObjectRelocationType::kArtField });
            offset += sizeof(ArtField);
          }
          image_info.IncrementBinSlotSize(
              Bin::kArtField, header_size + cur_fields->size() * sizeof(ArtField));
          DCHECK_EQ(offset, image_info.GetBinSlotSize(Bin::kArtField));
        }
      }
      // Visit and assign offsets for methods.
      size_t num_methods = as_klass->NumMethods();
      if (num_methods != 0) {
        bool any_dirty = false;
        for (auto& m : as_klass->GetMethods(target_ptr_size_)) {
          if (WillMethodBeDirty(&m)) {
            any_dirty = true;
            break;
          }
        }
        NativeObjectRelocationType type = any_dirty
            ? NativeObjectRelocationType::kArtMethodDirty
            : NativeObjectRelocationType::kArtMethodClean;
        Bin bin_type = BinTypeForNativeRelocationType(type);
        // Forward the entire array at once, but header first.
        const size_t method_alignment = ArtMethod::Alignment(target_ptr_size_);
        const size_t method_size = ArtMethod::Size(target_ptr_size_);
        const size_t header_size = LengthPrefixedArray<ArtMethod>::ComputeSize(0,
                                                                               method_size,
                                                                               method_alignment);
        LengthPrefixedArray<ArtMethod>* array = as_klass->GetMethodsPtr();
        auto it = native_object_relocations_.find(array);
        CHECK(it == native_object_relocations_.end())
            << "Method array " << array << " already forwarded";
        size_t offset = image_info.GetBinSlotSize(bin_type);
        DCHECK(!IsInBootImage(array));
        native_object_relocations_.emplace(array,
            NativeObjectRelocation {
                oat_index,
                offset,
                any_dirty ? NativeObjectRelocationType::kArtMethodArrayDirty
                          : NativeObjectRelocationType::kArtMethodArrayClean });
        image_info.IncrementBinSlotSize(bin_type, header_size);
        for (auto& m : as_klass->GetMethods(target_ptr_size_)) {
          AssignMethodOffset(&m, type, oat_index);
        }
        (any_dirty ? dirty_methods_ : clean_methods_) += num_methods;
      }
      // Assign offsets for all runtime methods in the IMT since these may hold conflict tables
      // live.
      if (as_klass->ShouldHaveImt()) {
        ImTable* imt = as_klass->GetImt(target_ptr_size_);
        if (TryAssignImTableOffset(imt, oat_index)) {
          // Since imt's can be shared only do this the first time to not double count imt method
          // fixups.
          for (size_t i = 0; i < ImTable::kSize; ++i) {
            ArtMethod* imt_method = imt->Get(i, target_ptr_size_);
            DCHECK(imt_method != nullptr);
            if (imt_method->IsRuntimeMethod() &&
                !IsInBootImage(imt_method) &&
                !NativeRelocationAssigned(imt_method)) {
              AssignMethodOffset(imt_method, NativeObjectRelocationType::kRuntimeMethod, oat_index);
            }
          }
        }
      }
    } else if (obj->IsClassLoader()) {
      // Register the class loader if it has a class table.
      // The fake boot class loader should not get registered.
      mirror::ClassLoader* class_loader = obj->AsClassLoader();
      if (class_loader->GetClassTable() != nullptr) {
        DCHECK(compiler_options_.IsAppImage());
        if (class_loader == GetAppClassLoader()) {
          ImageInfo& image_info = GetImageInfo(oat_index);
          // Note: Avoid locking to prevent lock order violations from root visiting;
          // image_info.class_table_ table is only accessed from the image writer
          // and class_loader->GetClassTable() is iterated but not modified.
          image_info.class_table_->CopyWithoutLocks(*class_loader->GetClassTable());
        }
      }
    }
    AssignImageBinSlot(obj, oat_index);
    work_stack.emplace(obj, oat_index);
  }
  if (obj->IsString()) {
    // Always return the interned string if there exists one.
    mirror::String* interned = FindInternedString(obj->AsString());
    if (interned != nullptr) {
      return interned;
    }
  }
  return obj;
}

bool ImageWriter::NativeRelocationAssigned(void* ptr) const {
  return native_object_relocations_.find(ptr) != native_object_relocations_.end();
}

bool ImageWriter::TryAssignImTableOffset(ImTable* imt, size_t oat_index) {
  // No offset, or already assigned.
  if (imt == nullptr || IsInBootImage(imt) || NativeRelocationAssigned(imt)) {
    return false;
  }
  // If the method is a conflict method we also want to assign the conflict table offset.
  ImageInfo& image_info = GetImageInfo(oat_index);
  const size_t size = ImTable::SizeInBytes(target_ptr_size_);
  native_object_relocations_.emplace(
      imt,
      NativeObjectRelocation {
          oat_index,
          image_info.GetBinSlotSize(Bin::kImTable),
          NativeObjectRelocationType::kIMTable});
  image_info.IncrementBinSlotSize(Bin::kImTable, size);
  return true;
}

void ImageWriter::TryAssignConflictTableOffset(ImtConflictTable* table, size_t oat_index) {
  // No offset, or already assigned.
  if (table == nullptr || NativeRelocationAssigned(table)) {
    return;
  }
  CHECK(!IsInBootImage(table));
  // If the method is a conflict method we also want to assign the conflict table offset.
  ImageInfo& image_info = GetImageInfo(oat_index);
  const size_t size = table->ComputeSize(target_ptr_size_);
  native_object_relocations_.emplace(
      table,
      NativeObjectRelocation {
          oat_index,
          image_info.GetBinSlotSize(Bin::kIMTConflictTable),
          NativeObjectRelocationType::kIMTConflictTable});
  image_info.IncrementBinSlotSize(Bin::kIMTConflictTable, size);
}

void ImageWriter::AssignMethodOffset(ArtMethod* method,
                                     NativeObjectRelocationType type,
                                     size_t oat_index) {
  DCHECK(!IsInBootImage(method));
  CHECK(!NativeRelocationAssigned(method)) << "Method " << method << " already assigned "
      << ArtMethod::PrettyMethod(method);
  if (method->IsRuntimeMethod()) {
    TryAssignConflictTableOffset(method->GetImtConflictTable(target_ptr_size_), oat_index);
  }
  ImageInfo& image_info = GetImageInfo(oat_index);
  Bin bin_type = BinTypeForNativeRelocationType(type);
  size_t offset = image_info.GetBinSlotSize(bin_type);
  native_object_relocations_.emplace(method, NativeObjectRelocation { oat_index, offset, type });
  image_info.IncrementBinSlotSize(bin_type, ArtMethod::Size(target_ptr_size_));
}

void ImageWriter::UnbinObjectsIntoOffset(mirror::Object* obj) {
  DCHECK(!IsInBootImage(obj));
  CHECK(obj != nullptr);

  // We know the bin slot, and the total bin sizes for all objects by now,
  // so calculate the object's final image offset.

  DCHECK(IsImageBinSlotAssigned(obj));
  BinSlot bin_slot = GetImageBinSlot(obj);
  // Change the lockword from a bin slot into an offset
  AssignImageOffset(obj, bin_slot);
}

class ImageWriter::VisitReferencesVisitor {
 public:
  VisitReferencesVisitor(ImageWriter* image_writer, WorkStack* work_stack, size_t oat_index)
      : image_writer_(image_writer), work_stack_(work_stack), oat_index_(oat_index) {}

  // Fix up separately since we also need to fix up method entrypoints.
  ALWAYS_INLINE void VisitRootIfNonNull(mirror::CompressedReference<mirror::Object>* root) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    if (!root->IsNull()) {
      VisitRoot(root);
    }
  }

  ALWAYS_INLINE void VisitRoot(mirror::CompressedReference<mirror::Object>* root) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    root->Assign(VisitReference(root->AsMirrorPtr()));
  }

  ALWAYS_INLINE void operator() (ObjPtr<mirror::Object> obj,
                                 MemberOffset offset,
                                 bool is_static ATTRIBUTE_UNUSED) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    mirror::Object* ref =
        obj->GetFieldObject<mirror::Object, kVerifyNone, kWithoutReadBarrier>(offset);
    obj->SetFieldObject</*kTransactionActive*/false>(offset, VisitReference(ref));
  }

  ALWAYS_INLINE void operator() (ObjPtr<mirror::Class> klass ATTRIBUTE_UNUSED,
                                 ObjPtr<mirror::Reference> ref) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    operator()(ref, mirror::Reference::ReferentOffset(), /* is_static */ false);
  }

 private:
  mirror::Object* VisitReference(mirror::Object* ref) const REQUIRES_SHARED(Locks::mutator_lock_) {
    return image_writer_->TryAssignBinSlot(*work_stack_, ref, oat_index_);
  }

  ImageWriter* const image_writer_;
  WorkStack* const work_stack_;
  const size_t oat_index_;
};

class ImageWriter::GetRootsVisitor : public RootVisitor  {
 public:
  explicit GetRootsVisitor(std::vector<mirror::Object*>* roots) : roots_(roots) {}

  void VisitRoots(mirror::Object*** roots,
                  size_t count,
                  const RootInfo& info ATTRIBUTE_UNUSED) override
      REQUIRES_SHARED(Locks::mutator_lock_) {
    for (size_t i = 0; i < count; ++i) {
      roots_->push_back(*roots[i]);
    }
  }

  void VisitRoots(mirror::CompressedReference<mirror::Object>** roots,
                  size_t count,
                  const RootInfo& info ATTRIBUTE_UNUSED) override
      REQUIRES_SHARED(Locks::mutator_lock_) {
    for (size_t i = 0; i < count; ++i) {
      roots_->push_back(roots[i]->AsMirrorPtr());
    }
  }

 private:
  std::vector<mirror::Object*>* const roots_;
};

void ImageWriter::ProcessWorkStack(WorkStack* work_stack) {
  while (!work_stack->empty()) {
    std::pair<mirror::Object*, size_t> pair(work_stack->top());
    work_stack->pop();
    VisitReferencesVisitor visitor(this, work_stack, /*oat_index*/ pair.second);
    // Walk references and assign bin slots for them.
    pair.first->VisitReferences</*kVisitNativeRoots*/true, kVerifyNone, kWithoutReadBarrier>(
        visitor,
        visitor);
  }
}

void ImageWriter::CalculateNewObjectOffsets() {
  Thread* const self = Thread::Current();
  Runtime* const runtime = Runtime::Current();
  VariableSizedHandleScope handles(self);
  MutableHandle<ObjectArray<Object>> boot_image_live_objects = handles.NewHandle(
      compiler_options_.IsAppImage()
          ? nullptr
          : IntrinsicObjects::AllocateBootImageLiveObjects(self, runtime->GetClassLinker()));
  std::vector<Handle<ObjectArray<Object>>> image_roots;
  for (size_t i = 0, size = oat_filenames_.size(); i != size; ++i) {
    image_roots.push_back(handles.NewHandle(CreateImageRoots(i, boot_image_live_objects)));
  }

  gc::Heap* const heap = runtime->GetHeap();

  // Leave space for the header, but do not write it yet, we need to
  // know where image_roots is going to end up
  image_objects_offset_begin_ = RoundUp(sizeof(ImageHeader), kObjectAlignment);  // 64-bit-alignment

  const size_t method_alignment = ArtMethod::Alignment(target_ptr_size_);
  // Write the image runtime methods.
  image_methods_[ImageHeader::kResolutionMethod] = runtime->GetResolutionMethod();
  image_methods_[ImageHeader::kImtConflictMethod] = runtime->GetImtConflictMethod();
  image_methods_[ImageHeader::kImtUnimplementedMethod] = runtime->GetImtUnimplementedMethod();
  image_methods_[ImageHeader::kSaveAllCalleeSavesMethod] =
      runtime->GetCalleeSaveMethod(CalleeSaveType::kSaveAllCalleeSaves);
  image_methods_[ImageHeader::kSaveRefsOnlyMethod] =
      runtime->GetCalleeSaveMethod(CalleeSaveType::kSaveRefsOnly);
  image_methods_[ImageHeader::kSaveRefsAndArgsMethod] =
      runtime->GetCalleeSaveMethod(CalleeSaveType::kSaveRefsAndArgs);
  image_methods_[ImageHeader::kSaveEverythingMethod] =
      runtime->GetCalleeSaveMethod(CalleeSaveType::kSaveEverything);
  image_methods_[ImageHeader::kSaveEverythingMethodForClinit] =
      runtime->GetCalleeSaveMethod(CalleeSaveType::kSaveEverythingForClinit);
  image_methods_[ImageHeader::kSaveEverythingMethodForSuspendCheck] =
      runtime->GetCalleeSaveMethod(CalleeSaveType::kSaveEverythingForSuspendCheck);
  // Visit image methods first to have the main runtime methods in the first image.
  for (auto* m : image_methods_) {
    CHECK(m != nullptr);
    CHECK(m->IsRuntimeMethod());
    DCHECK_EQ(!compiler_options_.IsBootImage(), IsInBootImage(m))
        << "Trampolines should be in boot image";
    if (!IsInBootImage(m)) {
      AssignMethodOffset(m, NativeObjectRelocationType::kRuntimeMethod, GetDefaultOatIndex());
    }
  }

  // Deflate monitors before we visit roots since deflating acquires the monitor lock. Acquiring
  // this lock while holding other locks may cause lock order violations.
  {
    auto deflate_monitor = [](mirror::Object* obj) REQUIRES_SHARED(Locks::mutator_lock_) {
      Monitor::Deflate(Thread::Current(), obj);
    };
    heap->VisitObjects(deflate_monitor);
  }

  // From this point on, there shall be no GC anymore and no objects shall be allocated.
  // We can now assign a BitSlot to each object and store it in its lockword.

  // Work list of <object, oat_index> for objects. Everything on the stack must already be
  // assigned a bin slot.
  WorkStack work_stack;

  // Special case interned strings to put them in the image they are likely to be resolved from.
  for (const DexFile* dex_file : compiler_options_.GetDexFilesForOatFile()) {
    auto it = dex_file_oat_index_map_.find(dex_file);
    DCHECK(it != dex_file_oat_index_map_.end()) << dex_file->GetLocation();
    const size_t oat_index = it->second;
    InternTable* const intern_table = runtime->GetInternTable();
    for (size_t i = 0, count = dex_file->NumStringIds(); i < count; ++i) {
      uint32_t utf16_length;
      const char* utf8_data = dex_file->StringDataAndUtf16LengthByIdx(dex::StringIndex(i),
                                                                      &utf16_length);
      mirror::String* string = intern_table->LookupStrong(self, utf16_length, utf8_data).Ptr();
      TryAssignBinSlot(work_stack, string, oat_index);
    }
  }

  // Get the GC roots and then visit them separately to avoid lock violations since the root visitor
  // visits roots while holding various locks.
  {
    std::vector<mirror::Object*> roots;
    GetRootsVisitor root_visitor(&roots);
    runtime->VisitRoots(&root_visitor);
    for (mirror::Object* obj : roots) {
      TryAssignBinSlot(work_stack, obj, GetDefaultOatIndex());
    }
  }
  ProcessWorkStack(&work_stack);

  // For app images, there may be objects that are only held live by the boot image. One
  // example is finalizer references. Forward these objects so that EnsureBinSlotAssignedCallback
  // does not fail any checks.
  if (compiler_options_.IsAppImage()) {
    for (gc::space::ImageSpace* space : heap->GetBootImageSpaces()) {
      DCHECK(space->IsImageSpace());
      gc::accounting::ContinuousSpaceBitmap* live_bitmap = space->GetLiveBitmap();
      live_bitmap->VisitMarkedRange(reinterpret_cast<uintptr_t>(space->Begin()),
                                    reinterpret_cast<uintptr_t>(space->Limit()),
                                    [this, &work_stack](mirror::Object* obj)
          REQUIRES_SHARED(Locks::mutator_lock_) {
        VisitReferencesVisitor visitor(this, &work_stack, GetDefaultOatIndex());
        // Visit all references and try to assign bin slots for them (calls TryAssignBinSlot).
        obj->VisitReferences</*kVisitNativeRoots*/true, kVerifyNone, kWithoutReadBarrier>(
            visitor,
            visitor);
      });
    }
    // Process the work stack in case anything was added by TryAssignBinSlot.
    ProcessWorkStack(&work_stack);

    // Store the class loader in the class roots.
    CHECK_EQ(image_roots.size(), 1u);
    image_roots[0]->Set<false>(ImageHeader::kAppImageClassLoader, GetAppClassLoader());
  }

  // Verify that all objects have assigned image bin slots.
  {
    auto ensure_bin_slots_assigned = [&](mirror::Object* obj)
        REQUIRES_SHARED(Locks::mutator_lock_) {
      if (!Runtime::Current()->GetHeap()->ObjectIsInBootImageSpace(obj)) {
        CHECK(IsImageBinSlotAssigned(obj)) << mirror::Object::PrettyTypeOf(obj) << " " << obj;
      }
    };
    heap->VisitObjects(ensure_bin_slots_assigned);
  }

  // Calculate size of the dex cache arrays slot and prepare offsets.
  PrepareDexCacheArraySlots();

  // Calculate the sizes of the intern tables, class tables, and fixup tables.
  for (ImageInfo& image_info : image_infos_) {
    // Calculate how big the intern table will be after being serialized.
    InternTable* const intern_table = image_info.intern_table_.get();
    CHECK_EQ(intern_table->WeakSize(), 0u) << " should have strong interned all the strings";
    if (intern_table->StrongSize() != 0u) {
      image_info.intern_table_bytes_ = intern_table->WriteToMemory(nullptr);
    }

    // Calculate the size of the class table.
    ReaderMutexLock mu(self, *Locks::classlinker_classes_lock_);
    DCHECK_EQ(image_info.class_table_->NumReferencedZygoteClasses(), 0u);
    if (image_info.class_table_->NumReferencedNonZygoteClasses() != 0u) {
      image_info.class_table_bytes_ += image_info.class_table_->WriteToMemory(nullptr);
    }
  }

  // Calculate bin slot offsets.
  for (ImageInfo& image_info : image_infos_) {
    size_t bin_offset = image_objects_offset_begin_;
    for (size_t i = 0; i != kNumberOfBins; ++i) {
      switch (static_cast<Bin>(i)) {
        case Bin::kArtMethodClean:
        case Bin::kArtMethodDirty: {
          bin_offset = RoundUp(bin_offset, method_alignment);
          break;
        }
        case Bin::kDexCacheArray:
          bin_offset = RoundUp(bin_offset, DexCacheArraysLayout::Alignment(target_ptr_size_));
          break;
        case Bin::kImTable:
        case Bin::kIMTConflictTable: {
          bin_offset = RoundUp(bin_offset, static_cast<size_t>(target_ptr_size_));
          break;
        }
        default: {
          // Normal alignment.
        }
      }
      image_info.bin_slot_offsets_[i] = bin_offset;
      bin_offset += image_info.bin_slot_sizes_[i];
    }
    // NOTE: There may be additional padding between the bin slots and the intern table.
    DCHECK_EQ(image_info.image_end_,
              image_info.GetBinSizeSum(Bin::kMirrorCount) + image_objects_offset_begin_);
  }

  // Calculate image offsets.
  size_t image_offset = 0;
  for (ImageInfo& image_info : image_infos_) {
    image_info.image_begin_ = global_image_begin_ + image_offset;
    image_info.image_offset_ = image_offset;
    image_info.image_size_ = RoundUp(image_info.CreateImageSections().first, kPageSize);
    // There should be no gaps until the next image.
    image_offset += image_info.image_size_;
  }

  // Transform each object's bin slot into an offset which will be used to do the final copy.
  {
    auto unbin_objects_into_offset = [&](mirror::Object* obj)
        REQUIRES_SHARED(Locks::mutator_lock_) {
      if (!IsInBootImage(obj)) {
        UnbinObjectsIntoOffset(obj);
      }
    };
    heap->VisitObjects(unbin_objects_into_offset);
  }

  size_t i = 0;
  for (ImageInfo& image_info : image_infos_) {
    image_info.image_roots_address_ = PointerToLowMemUInt32(GetImageAddress(image_roots[i].Get()));
    i++;
  }

  // Update the native relocations by adding their bin sums.
  for (auto& pair : native_object_relocations_) {
    NativeObjectRelocation& relocation = pair.second;
    Bin bin_type = BinTypeForNativeRelocationType(relocation.type);
    ImageInfo& image_info = GetImageInfo(relocation.oat_index);
    relocation.offset += image_info.GetBinSlotOffset(bin_type);
  }

  // Remember the boot image live objects as raw pointer. No GC can happen anymore.
  boot_image_live_objects_ = boot_image_live_objects.Get();
}

std::pair<size_t, std::vector<ImageSection>> ImageWriter::ImageInfo::CreateImageSections() const {
  std::vector<ImageSection> sections(ImageHeader::kSectionCount);

  // Do not round up any sections here that are represented by the bins since it
  // will break offsets.

  /*
   * Objects section
   */
  sections[ImageHeader::kSectionObjects] =
      ImageSection(0u, image_end_);

  /*
   * Field section
   */
  sections[ImageHeader::kSectionArtFields] =
      ImageSection(GetBinSlotOffset(Bin::kArtField), GetBinSlotSize(Bin::kArtField));

  /*
   * Method section
   */
  sections[ImageHeader::kSectionArtMethods] =
      ImageSection(GetBinSlotOffset(Bin::kArtMethodClean),
                   GetBinSlotSize(Bin::kArtMethodClean) +
                   GetBinSlotSize(Bin::kArtMethodDirty));

  /*
   * IMT section
   */
  sections[ImageHeader::kSectionImTables] =
      ImageSection(GetBinSlotOffset(Bin::kImTable), GetBinSlotSize(Bin::kImTable));

  /*
   * Conflict Tables section
   */
  sections[ImageHeader::kSectionIMTConflictTables] =
      ImageSection(GetBinSlotOffset(Bin::kIMTConflictTable), GetBinSlotSize(Bin::kIMTConflictTable));

  /*
   * Runtime Methods section
   */
  sections[ImageHeader::kSectionRuntimeMethods] =
      ImageSection(GetBinSlotOffset(Bin::kRuntimeMethod), GetBinSlotSize(Bin::kRuntimeMethod));

  /*
   * DexCache Arrays section.
   */
  const ImageSection& dex_cache_arrays_section =
      sections[ImageHeader::kSectionDexCacheArrays] =
          ImageSection(GetBinSlotOffset(Bin::kDexCacheArray),
                       GetBinSlotSize(Bin::kDexCacheArray));

  /*
   * Interned Strings section
   */

  // Round up to the alignment the string table expects. See HashSet::WriteToMemory.
  size_t cur_pos = RoundUp(dex_cache_arrays_section.End(), sizeof(uint64_t));

  const ImageSection& interned_strings_section =
      sections[ImageHeader::kSectionInternedStrings] =
          ImageSection(cur_pos, intern_table_bytes_);

  /*
   * Class Table section
   */

  // Obtain the new position and round it up to the appropriate alignment.
  cur_pos = RoundUp(interned_strings_section.End(), sizeof(uint64_t));

  const ImageSection& class_table_section =
      sections[ImageHeader::kSectionClassTable] =
          ImageSection(cur_pos, class_table_bytes_);

  /*
   * String Field Offsets section
   */

  // Round up to the alignment of the offsets we are going to store.
  cur_pos = RoundUp(class_table_section.End(), sizeof(uint32_t));

  // The size of string_reference_offsets_ can't be used here because it hasn't
  // been filled with AppImageReferenceOffsetInfo objects yet.  The
  // num_string_references_ value is calculated separately, before we can
  // compute the actual offsets.
  const ImageSection& string_reference_offsets =
      sections[ImageHeader::kSectionStringReferenceOffsets] =
          ImageSection(cur_pos,
                       sizeof(typename decltype(string_reference_offsets_)::value_type) *
                           num_string_references_);

  /*
   * Metadata section.
   */

  // Round up to the alignment of the offsets we are going to store.
  cur_pos = RoundUp(string_reference_offsets.End(),
                    mirror::DexCache::PreResolvedStringsAlignment());

  const ImageSection& metadata_section =
      sections[ImageHeader::kSectionMetadata] =
          ImageSection(cur_pos, GetBinSlotSize(Bin::kMetadata));

  // Return the number of bytes described by these sections, and the sections
  // themselves.
  return make_pair(metadata_section.End(), std::move(sections));
}

void ImageWriter::CreateHeader(size_t oat_index) {
  ImageInfo& image_info = GetImageInfo(oat_index);
  const uint8_t* oat_file_begin = image_info.oat_file_begin_;
  const uint8_t* oat_file_end = oat_file_begin + image_info.oat_loaded_size_;
  const uint8_t* oat_data_end = image_info.oat_data_begin_ + image_info.oat_size_;

  // Create the image sections.
  auto section_info_pair = image_info.CreateImageSections();
  const size_t image_end = section_info_pair.first;
  std::vector<ImageSection>& sections = section_info_pair.second;

  // Finally bitmap section.
  const size_t bitmap_bytes = image_info.image_bitmap_->Size();
  auto* bitmap_section = &sections[ImageHeader::kSectionImageBitmap];
  *bitmap_section = ImageSection(RoundUp(image_end, kPageSize), RoundUp(bitmap_bytes, kPageSize));
  if (VLOG_IS_ON(compiler)) {
    LOG(INFO) << "Creating header for " << oat_filenames_[oat_index];
    size_t idx = 0;
    for (const ImageSection& section : sections) {
      LOG(INFO) << static_cast<ImageHeader::ImageSections>(idx) << " " << section;
      ++idx;
    }
    LOG(INFO) << "Methods: clean=" << clean_methods_ << " dirty=" << dirty_methods_;
    LOG(INFO) << "Image roots address=" << std::hex << image_info.image_roots_address_ << std::dec;
    LOG(INFO) << "Image begin=" << std::hex << reinterpret_cast<uintptr_t>(global_image_begin_)
              << " Image offset=" << image_info.image_offset_ << std::dec;
    LOG(INFO) << "Oat file begin=" << std::hex << reinterpret_cast<uintptr_t>(oat_file_begin)
              << " Oat data begin=" << reinterpret_cast<uintptr_t>(image_info.oat_data_begin_)
              << " Oat data end=" << reinterpret_cast<uintptr_t>(oat_data_end)
              << " Oat file end=" << reinterpret_cast<uintptr_t>(oat_file_end);
  }
  // Store boot image info for app image so that we can relocate.
  uint32_t boot_image_begin = 0;
  uint32_t boot_image_end = 0;
  uint32_t boot_oat_begin = 0;
  uint32_t boot_oat_end = 0;
  gc::Heap* const heap = Runtime::Current()->GetHeap();
  heap->GetBootImagesSize(&boot_image_begin, &boot_image_end, &boot_oat_begin, &boot_oat_end);

  // Create the header, leave 0 for data size since we will fill this in as we are writing the
  // image.
  new (image_info.image_.Begin()) ImageHeader(
      PointerToLowMemUInt32(image_info.image_begin_),
      image_end,
      sections.data(),
      image_info.image_roots_address_,
      image_info.oat_checksum_,
      PointerToLowMemUInt32(oat_file_begin),
      PointerToLowMemUInt32(image_info.oat_data_begin_),
      PointerToLowMemUInt32(oat_data_end),
      PointerToLowMemUInt32(oat_file_end),
      boot_image_begin,
      boot_oat_end - boot_image_begin,
      static_cast<uint32_t>(target_ptr_size_),
      image_storage_mode_,
      /*data_size*/0u);
}

ArtMethod* ImageWriter::GetImageMethodAddress(ArtMethod* method) {
  NativeObjectRelocation relocation = GetNativeRelocation(method);
  const ImageInfo& image_info = GetImageInfo(relocation.oat_index);
  CHECK_GE(relocation.offset, image_info.image_end_) << "ArtMethods should be after Objects";
  return reinterpret_cast<ArtMethod*>(image_info.image_begin_ + relocation.offset);
}

const void* ImageWriter::GetIntrinsicReferenceAddress(uint32_t intrinsic_data) {
  DCHECK(compiler_options_.IsBootImage());
  switch (IntrinsicObjects::DecodePatchType(intrinsic_data)) {
    case IntrinsicObjects::PatchType::kIntegerValueOfArray: {
      const uint8_t* base_address =
          reinterpret_cast<const uint8_t*>(GetImageAddress(boot_image_live_objects_));
      MemberOffset data_offset =
          IntrinsicObjects::GetIntegerValueOfArrayDataOffset(boot_image_live_objects_);
      return base_address + data_offset.Uint32Value();
    }
    case IntrinsicObjects::PatchType::kIntegerValueOfObject: {
      uint32_t index = IntrinsicObjects::DecodePatchIndex(intrinsic_data);
      ObjPtr<mirror::Object> value =
          IntrinsicObjects::GetIntegerValueOfObject(boot_image_live_objects_, index);
      return GetImageAddress(value.Ptr());
    }
  }
  LOG(FATAL) << "UNREACHABLE";
  UNREACHABLE();
}


class ImageWriter::FixupRootVisitor : public RootVisitor {
 public:
  explicit FixupRootVisitor(ImageWriter* image_writer) : image_writer_(image_writer) {
  }

  void VisitRoots(mirror::Object*** roots ATTRIBUTE_UNUSED,
                  size_t count ATTRIBUTE_UNUSED,
                  const RootInfo& info ATTRIBUTE_UNUSED)
      override REQUIRES_SHARED(Locks::mutator_lock_) {
    LOG(FATAL) << "Unsupported";
  }

  void VisitRoots(mirror::CompressedReference<mirror::Object>** roots,
                  size_t count,
                  const RootInfo& info ATTRIBUTE_UNUSED)
      override REQUIRES_SHARED(Locks::mutator_lock_) {
    for (size_t i = 0; i < count; ++i) {
      // Copy the reference. Since we do not have the address for recording the relocation,
      // it needs to be recorded explicitly by the user of FixupRootVisitor.
      ObjPtr<mirror::Object> old_ptr = roots[i]->AsMirrorPtr();
      roots[i]->Assign(image_writer_->GetImageAddress(old_ptr.Ptr()));
    }
  }

 private:
  ImageWriter* const image_writer_;
};

void ImageWriter::CopyAndFixupImTable(ImTable* orig, ImTable* copy) {
  for (size_t i = 0; i < ImTable::kSize; ++i) {
    ArtMethod* method = orig->Get(i, target_ptr_size_);
    void** address = reinterpret_cast<void**>(copy->AddressOfElement(i, target_ptr_size_));
    CopyAndFixupPointer(address, method);
    DCHECK_EQ(copy->Get(i, target_ptr_size_), NativeLocationInImage(method));
  }
}

void ImageWriter::CopyAndFixupImtConflictTable(ImtConflictTable* orig, ImtConflictTable* copy) {
  const size_t count = orig->NumEntries(target_ptr_size_);
  for (size_t i = 0; i < count; ++i) {
    ArtMethod* interface_method = orig->GetInterfaceMethod(i, target_ptr_size_);
    ArtMethod* implementation_method = orig->GetImplementationMethod(i, target_ptr_size_);
    CopyAndFixupPointer(copy->AddressOfInterfaceMethod(i, target_ptr_size_), interface_method);
    CopyAndFixupPointer(
        copy->AddressOfImplementationMethod(i, target_ptr_size_), implementation_method);
    DCHECK_EQ(copy->GetInterfaceMethod(i, target_ptr_size_),
              NativeLocationInImage(interface_method));
    DCHECK_EQ(copy->GetImplementationMethod(i, target_ptr_size_),
              NativeLocationInImage(implementation_method));
  }
}

void ImageWriter::CopyAndFixupNativeData(size_t oat_index) {
  const ImageInfo& image_info = GetImageInfo(oat_index);
  // Copy ArtFields and methods to their locations and update the array for convenience.
  for (auto& pair : native_object_relocations_) {
    NativeObjectRelocation& relocation = pair.second;
    // Only work with fields and methods that are in the current oat file.
    if (relocation.oat_index != oat_index) {
      continue;
    }
    auto* dest = image_info.image_.Begin() + relocation.offset;
    DCHECK_GE(dest, image_info.image_.Begin() + image_info.image_end_);
    DCHECK(!IsInBootImage(pair.first));
    switch (relocation.type) {
      case NativeObjectRelocationType::kArtField: {
        memcpy(dest, pair.first, sizeof(ArtField));
        CopyAndFixupReference(
            reinterpret_cast<ArtField*>(dest)->GetDeclaringClassAddressWithoutBarrier(),
            reinterpret_cast<ArtField*>(pair.first)->GetDeclaringClass());
        break;
      }
      case NativeObjectRelocationType::kRuntimeMethod:
      case NativeObjectRelocationType::kArtMethodClean:
      case NativeObjectRelocationType::kArtMethodDirty: {
        CopyAndFixupMethod(reinterpret_cast<ArtMethod*>(pair.first),
                           reinterpret_cast<ArtMethod*>(dest),
                           oat_index);
        break;
      }
      // For arrays, copy just the header since the elements will get copied by their corresponding
      // relocations.
      case NativeObjectRelocationType::kArtFieldArray: {
        memcpy(dest, pair.first, LengthPrefixedArray<ArtField>::ComputeSize(0));
        break;
      }
      case NativeObjectRelocationType::kArtMethodArrayClean:
      case NativeObjectRelocationType::kArtMethodArrayDirty: {
        size_t size = ArtMethod::Size(target_ptr_size_);
        size_t alignment = ArtMethod::Alignment(target_ptr_size_);
        memcpy(dest, pair.first, LengthPrefixedArray<ArtMethod>::ComputeSize(0, size, alignment));
        // Clear padding to avoid non-deterministic data in the image.
        // Historical note: We also did that to placate Valgrind.
        reinterpret_cast<LengthPrefixedArray<ArtMethod>*>(dest)->ClearPadding(size, alignment);
        break;
      }
      case NativeObjectRelocationType::kDexCacheArray:
        // Nothing to copy here, everything is done in FixupDexCache().
        break;
      case NativeObjectRelocationType::kIMTable: {
        ImTable* orig_imt = reinterpret_cast<ImTable*>(pair.first);
        ImTable* dest_imt = reinterpret_cast<ImTable*>(dest);
        CopyAndFixupImTable(orig_imt, dest_imt);
        break;
      }
      case NativeObjectRelocationType::kIMTConflictTable: {
        auto* orig_table = reinterpret_cast<ImtConflictTable*>(pair.first);
        CopyAndFixupImtConflictTable(
            orig_table,
            new(dest)ImtConflictTable(orig_table->NumEntries(target_ptr_size_), target_ptr_size_));
        break;
      }
      case NativeObjectRelocationType::kGcRootPointer: {
        auto* orig_pointer = reinterpret_cast<GcRoot<mirror::Object>*>(pair.first);
        auto* dest_pointer = reinterpret_cast<GcRoot<mirror::Object>*>(dest);
        CopyAndFixupReference(dest_pointer->AddressWithoutBarrier(), orig_pointer->Read());
        break;
      }
    }
  }
  // Fixup the image method roots.
  auto* image_header = reinterpret_cast<ImageHeader*>(image_info.image_.Begin());
  for (size_t i = 0; i < ImageHeader::kImageMethodsCount; ++i) {
    ArtMethod* method = image_methods_[i];
    CHECK(method != nullptr);
    CopyAndFixupPointer(
        reinterpret_cast<void**>(&image_header->image_methods_[i]), method, PointerSize::k32);
  }
  FixupRootVisitor root_visitor(this);

  // Write the intern table into the image.
  if (image_info.intern_table_bytes_ > 0) {
    const ImageSection& intern_table_section = image_header->GetInternedStringsSection();
    InternTable* const intern_table = image_info.intern_table_.get();
    uint8_t* const intern_table_memory_ptr =
        image_info.image_.Begin() + intern_table_section.Offset();
    const size_t intern_table_bytes = intern_table->WriteToMemory(intern_table_memory_ptr);
    CHECK_EQ(intern_table_bytes, image_info.intern_table_bytes_);
    // Fixup the pointers in the newly written intern table to contain image addresses.
    InternTable temp_intern_table;
    // Note that we require that ReadFromMemory does not make an internal copy of the elements so
    // that the VisitRoots() will update the memory directly rather than the copies.
    // This also relies on visit roots not doing any verification which could fail after we update
    // the roots to be the image addresses.
    temp_intern_table.AddTableFromMemory(intern_table_memory_ptr,
                                         VoidFunctor(),
                                         /*is_boot_image=*/ false);
    CHECK_EQ(temp_intern_table.Size(), intern_table->Size());
    temp_intern_table.VisitRoots(&root_visitor, kVisitRootFlagAllRoots);
    // Record relocations. (The root visitor does not get to see the slot addresses.)
    MutexLock lock(Thread::Current(), *Locks::intern_table_lock_);
    DCHECK(!temp_intern_table.strong_interns_.tables_.empty());
    DCHECK(!temp_intern_table.strong_interns_.tables_[0].Empty());  // Inserted at the beginning.
  }
  // Write the class table(s) into the image. class_table_bytes_ may be 0 if there are multiple
  // class loaders. Writing multiple class tables into the image is currently unsupported.
  if (image_info.class_table_bytes_ > 0u) {
    const ImageSection& class_table_section = image_header->GetClassTableSection();
    uint8_t* const class_table_memory_ptr =
        image_info.image_.Begin() + class_table_section.Offset();
    Thread* self = Thread::Current();
    ReaderMutexLock mu(self, *Locks::classlinker_classes_lock_);

    ClassTable* table = image_info.class_table_.get();
    CHECK(table != nullptr);
    const size_t class_table_bytes = table->WriteToMemory(class_table_memory_ptr);
    CHECK_EQ(class_table_bytes, image_info.class_table_bytes_);
    // Fixup the pointers in the newly written class table to contain image addresses. See
    // above comment for intern tables.
    ClassTable temp_class_table;
    temp_class_table.ReadFromMemory(class_table_memory_ptr);
    CHECK_EQ(temp_class_table.NumReferencedZygoteClasses(),
             table->NumReferencedNonZygoteClasses() + table->NumReferencedZygoteClasses());
    UnbufferedRootVisitor visitor(&root_visitor, RootInfo(kRootUnknown));
    temp_class_table.VisitRoots(visitor);
    // Record relocations. (The root visitor does not get to see the slot addresses.)
    // Note that the low bits in the slots contain bits of the descriptors' hash codes
    // but the relocation works fine for these "adjusted" references.
    ReaderMutexLock lock(self, temp_class_table.lock_);
    DCHECK(!temp_class_table.classes_.empty());
    DCHECK(!temp_class_table.classes_[0].empty());  // The ClassSet was inserted at the beginning.
  }
}

void ImageWriter::CopyAndFixupObjects() {
  auto visitor = [&](Object* obj) REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK(obj != nullptr);
    CopyAndFixupObject(obj);
  };
  Runtime::Current()->GetHeap()->VisitObjects(visitor);
  // We no longer need the hashcode map, values have already been copied to target objects.
  saved_hashcode_map_.clear();
}

void ImageWriter::FixupPointerArray(mirror::Object* dst,
                                    mirror::PointerArray* arr,
                                    Bin array_type) {
  CHECK(arr->IsIntArray() || arr->IsLongArray()) << arr->GetClass()->PrettyClass() << " " << arr;
  // Fixup int and long pointers for the ArtMethod or ArtField arrays.
  const size_t num_elements = arr->GetLength();
  CopyAndFixupReference(
      dst->GetFieldObjectReferenceAddr<kVerifyNone>(Class::ClassOffset()), arr->GetClass());
  auto* dest_array = down_cast<mirror::PointerArray*>(dst);
  for (size_t i = 0, count = num_elements; i < count; ++i) {
    void* elem = arr->GetElementPtrSize<void*>(i, target_ptr_size_);
    if (kIsDebugBuild && elem != nullptr && !IsInBootImage(elem)) {
      auto it = native_object_relocations_.find(elem);
      if (UNLIKELY(it == native_object_relocations_.end())) {
        if (it->second.IsArtMethodRelocation()) {
          auto* method = reinterpret_cast<ArtMethod*>(elem);
          LOG(FATAL) << "No relocation entry for ArtMethod " << method->PrettyMethod() << " @ "
                     << method << " idx=" << i << "/" << num_elements << " with declaring class "
                     << Class::PrettyClass(method->GetDeclaringClass());
        } else {
          CHECK_EQ(array_type, Bin::kArtField);
          auto* field = reinterpret_cast<ArtField*>(elem);
          LOG(FATAL) << "No relocation entry for ArtField " << field->PrettyField() << " @ "
              << field << " idx=" << i << "/" << num_elements << " with declaring class "
              << Class::PrettyClass(field->GetDeclaringClass());
        }
        UNREACHABLE();
      }
    }
    CopyAndFixupPointer(dest_array->ElementAddress(i, target_ptr_size_), elem);
  }
}

void ImageWriter::CopyAndFixupObject(Object* obj) {
  if (IsInBootImage(obj)) {
    return;
  }
  size_t offset = GetImageOffset(obj);
  size_t oat_index = GetOatIndex(obj);
  ImageInfo& image_info = GetImageInfo(oat_index);
  auto* dst = reinterpret_cast<Object*>(image_info.image_.Begin() + offset);
  DCHECK_LT(offset, image_info.image_end_);
  const auto* src = reinterpret_cast<const uint8_t*>(obj);

  image_info.image_bitmap_->Set(dst);  // Mark the obj as live.

  const size_t n = obj->SizeOf();
  DCHECK_LE(offset + n, image_info.image_.Size());
  memcpy(dst, src, n);

  // Write in a hash code of objects which have inflated monitors or a hash code in their monitor
  // word.
  const auto it = saved_hashcode_map_.find(obj);
  dst->SetLockWord(it != saved_hashcode_map_.end() ?
      LockWord::FromHashCode(it->second, 0u) : LockWord::Default(), false);
  if (kUseBakerReadBarrier && gc::collector::ConcurrentCopying::kGrayDirtyImmuneObjects) {
    // Treat all of the objects in the image as marked to avoid unnecessary dirty pages. This is
    // safe since we mark all of the objects that may reference non immune objects as gray.
    CHECK(dst->AtomicSetMarkBit(0, 1));
  }
  FixupObject(obj, dst);
}

// Rewrite all the references in the copied object to point to their image address equivalent
class ImageWriter::FixupVisitor {
 public:
  FixupVisitor(ImageWriter* image_writer, Object* copy)
      : image_writer_(image_writer), copy_(copy) {
  }

  // Ignore class roots since we don't have a way to map them to the destination. These are handled
  // with other logic.
  void VisitRootIfNonNull(mirror::CompressedReference<mirror::Object>* root ATTRIBUTE_UNUSED)
      const {}
  void VisitRoot(mirror::CompressedReference<mirror::Object>* root ATTRIBUTE_UNUSED) const {}


  void operator()(ObjPtr<Object> obj, MemberOffset offset, bool is_static ATTRIBUTE_UNUSED) const
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(Locks::heap_bitmap_lock_) {
    ObjPtr<Object> ref = obj->GetFieldObject<Object, kVerifyNone>(offset);
    // Copy the reference and record the fixup if necessary.
    image_writer_->CopyAndFixupReference(
        copy_->GetFieldObjectReferenceAddr<kVerifyNone>(offset), ref);
  }

  // java.lang.ref.Reference visitor.
  void operator()(ObjPtr<mirror::Class> klass ATTRIBUTE_UNUSED,
                  ObjPtr<mirror::Reference> ref) const
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(Locks::heap_bitmap_lock_) {
    operator()(ref, mirror::Reference::ReferentOffset(), /* is_static */ false);
  }

 protected:
  ImageWriter* const image_writer_;
  mirror::Object* const copy_;
};

class ImageWriter::FixupClassVisitor final : public FixupVisitor {
 public:
  FixupClassVisitor(ImageWriter* image_writer, Object* copy)
      : FixupVisitor(image_writer, copy) {}

  void operator()(ObjPtr<Object> obj, MemberOffset offset, bool is_static ATTRIBUTE_UNUSED) const
      REQUIRES(Locks::mutator_lock_, Locks::heap_bitmap_lock_) {
    DCHECK(obj->IsClass());
    FixupVisitor::operator()(obj, offset, /*is_static*/false);
  }

  void operator()(ObjPtr<mirror::Class> klass ATTRIBUTE_UNUSED,
                  ObjPtr<mirror::Reference> ref ATTRIBUTE_UNUSED) const
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(Locks::heap_bitmap_lock_) {
    LOG(FATAL) << "Reference not expected here.";
  }
};

ImageWriter::NativeObjectRelocation ImageWriter::GetNativeRelocation(void* obj) {
  DCHECK(obj != nullptr);
  DCHECK(!IsInBootImage(obj));
  auto it = native_object_relocations_.find(obj);
  CHECK(it != native_object_relocations_.end()) << obj << " spaces "
      << Runtime::Current()->GetHeap()->DumpSpaces();
  return it->second;
}

template <typename T>
std::string PrettyPrint(T* ptr) REQUIRES_SHARED(Locks::mutator_lock_) {
  std::ostringstream oss;
  oss << ptr;
  return oss.str();
}

template <>
std::string PrettyPrint(ArtMethod* method) REQUIRES_SHARED(Locks::mutator_lock_) {
  return ArtMethod::PrettyMethod(method);
}

template <typename T>
T* ImageWriter::NativeLocationInImage(T* obj) {
  if (obj == nullptr || IsInBootImage(obj)) {
    return obj;
  } else {
    NativeObjectRelocation relocation = GetNativeRelocation(obj);
    const ImageInfo& image_info = GetImageInfo(relocation.oat_index);
    return reinterpret_cast<T*>(image_info.image_begin_ + relocation.offset);
  }
}

template <typename T>
T* ImageWriter::NativeCopyLocation(T* obj) {
  const NativeObjectRelocation relocation = GetNativeRelocation(obj);
  const ImageInfo& image_info = GetImageInfo(relocation.oat_index);
  return reinterpret_cast<T*>(image_info.image_.Begin() + relocation.offset);
}

class ImageWriter::NativeLocationVisitor {
 public:
  explicit NativeLocationVisitor(ImageWriter* image_writer)
      : image_writer_(image_writer) {}

  template <typename T>
  T* operator()(T* ptr, void** dest_addr) const REQUIRES_SHARED(Locks::mutator_lock_) {
    if (ptr != nullptr) {
      image_writer_->CopyAndFixupPointer(dest_addr, ptr);
    }
    // TODO: The caller shall overwrite the value stored by CopyAndFixupPointer()
    // with the value we return here. We should try to avoid the duplicate work.
    return image_writer_->NativeLocationInImage(ptr);
  }

 private:
  ImageWriter* const image_writer_;
};

void ImageWriter::FixupClass(mirror::Class* orig, mirror::Class* copy) {
  orig->FixupNativePointers(copy, target_ptr_size_, NativeLocationVisitor(this));
  FixupClassVisitor visitor(this, copy);
  ObjPtr<mirror::Object>(orig)->VisitReferences(visitor, visitor);

  if (kBitstringSubtypeCheckEnabled && compiler_options_.IsAppImage()) {
    // When we call SubtypeCheck::EnsureInitialize, it Assigns new bitstring
    // values to the parent of that class.
    //
    // Every time this happens, the parent class has to mutate to increment
    // the "Next" value.
    //
    // If any of these parents are in the boot image, the changes [in the parents]
    // would be lost when the app image is reloaded.
    //
    // To prevent newly loaded classes (not in the app image) from being reassigned
    // the same bitstring value as an existing app image class, uninitialize
    // all the classes in the app image.
    //
    // On startup, the class linker will then re-initialize all the app
    // image bitstrings. See also ClassLinker::AddImageSpace.
    MutexLock subtype_check_lock(Thread::Current(), *Locks::subtype_check_lock_);
    // Lock every time to prevent a dcheck failure when we suspend with the lock held.
    SubtypeCheck<mirror::Class*>::ForceUninitialize(copy);
  }

  // Remove the clinitThreadId. This is required for image determinism.
  copy->SetClinitThreadId(static_cast<pid_t>(0));
}

void ImageWriter::FixupObject(Object* orig, Object* copy) {
  DCHECK(orig != nullptr);
  DCHECK(copy != nullptr);
  if (kUseBakerReadBarrier) {
    orig->AssertReadBarrierState();
  }
  if (orig->IsIntArray() || orig->IsLongArray()) {
    // Is this a native pointer array?
    auto it = pointer_arrays_.find(down_cast<mirror::PointerArray*>(orig));
    if (it != pointer_arrays_.end()) {
      // Should only need to fixup every pointer array exactly once.
      FixupPointerArray(copy, down_cast<mirror::PointerArray*>(orig), it->second);
      pointer_arrays_.erase(it);
      return;
    }
  }
  if (orig->IsClass()) {
    FixupClass(orig->AsClass<kVerifyNone>(), down_cast<mirror::Class*>(copy));
  } else {
    ObjPtr<mirror::ObjectArray<mirror::Class>> class_roots =
        Runtime::Current()->GetClassLinker()->GetClassRoots();
    ObjPtr<mirror::Class> klass = orig->GetClass();
    if (klass == GetClassRoot<mirror::Method>(class_roots) ||
        klass == GetClassRoot<mirror::Constructor>(class_roots)) {
      // Need to go update the ArtMethod.
      auto* dest = down_cast<mirror::Executable*>(copy);
      auto* src = down_cast<mirror::Executable*>(orig);
      ArtMethod* src_method = src->GetArtMethod();
      CopyAndFixupPointer(dest, mirror::Executable::ArtMethodOffset(), src_method);
    } else if (klass == GetClassRoot<mirror::DexCache>(class_roots)) {
      FixupDexCache(down_cast<mirror::DexCache*>(orig), down_cast<mirror::DexCache*>(copy));
    } else if (klass->IsClassLoaderClass()) {
      mirror::ClassLoader* copy_loader = down_cast<mirror::ClassLoader*>(copy);
      // If src is a ClassLoader, set the class table to null so that it gets recreated by the
      // ClassLoader.
      copy_loader->SetClassTable(nullptr);
      // Also set allocator to null to be safe. The allocator is created when we create the class
      // table. We also never expect to unload things in the image since they are held live as
      // roots.
      copy_loader->SetAllocator(nullptr);
    }
    FixupVisitor visitor(this, copy);
    orig->VisitReferences(visitor, visitor);
  }
}

template <typename T>
void ImageWriter::FixupDexCacheArrayEntry(std::atomic<mirror::DexCachePair<T>>* orig_array,
                                          std::atomic<mirror::DexCachePair<T>>* new_array,
                                          uint32_t array_index) {
  static_assert(sizeof(std::atomic<mirror::DexCachePair<T>>) == sizeof(mirror::DexCachePair<T>),
                "Size check for removing std::atomic<>.");
  mirror::DexCachePair<T>* orig_pair =
      reinterpret_cast<mirror::DexCachePair<T>*>(&orig_array[array_index]);
  mirror::DexCachePair<T>* new_pair =
      reinterpret_cast<mirror::DexCachePair<T>*>(&new_array[array_index]);
  CopyAndFixupReference(
      new_pair->object.AddressWithoutBarrier(), orig_pair->object.Read());
  new_pair->index = orig_pair->index;
}

template <typename T>
void ImageWriter::FixupDexCacheArrayEntry(std::atomic<mirror::NativeDexCachePair<T>>* orig_array,
                                          std::atomic<mirror::NativeDexCachePair<T>>* new_array,
                                          uint32_t array_index) {
  static_assert(
      sizeof(std::atomic<mirror::NativeDexCachePair<T>>) == sizeof(mirror::NativeDexCachePair<T>),
      "Size check for removing std::atomic<>.");
  if (target_ptr_size_ == PointerSize::k64) {
    DexCache::ConversionPair64* orig_pair =
        reinterpret_cast<DexCache::ConversionPair64*>(orig_array) + array_index;
    DexCache::ConversionPair64* new_pair =
        reinterpret_cast<DexCache::ConversionPair64*>(new_array) + array_index;
    *new_pair = *orig_pair;  // Copy original value and index.
    if (orig_pair->first != 0u) {
      CopyAndFixupPointer(
          reinterpret_cast<void**>(&new_pair->first), reinterpret_cast64<void*>(orig_pair->first));
    }
  } else {
    DexCache::ConversionPair32* orig_pair =
        reinterpret_cast<DexCache::ConversionPair32*>(orig_array) + array_index;
    DexCache::ConversionPair32* new_pair =
        reinterpret_cast<DexCache::ConversionPair32*>(new_array) + array_index;
    *new_pair = *orig_pair;  // Copy original value and index.
    if (orig_pair->first != 0u) {
      CopyAndFixupPointer(
          reinterpret_cast<void**>(&new_pair->first), reinterpret_cast32<void*>(orig_pair->first));
    }
  }
}

void ImageWriter::FixupDexCacheArrayEntry(GcRoot<mirror::CallSite>* orig_array,
                                          GcRoot<mirror::CallSite>* new_array,
                                          uint32_t array_index) {
  CopyAndFixupReference(
      new_array[array_index].AddressWithoutBarrier(), orig_array[array_index].Read());
}

template <typename EntryType>
void ImageWriter::FixupDexCacheArray(DexCache* orig_dex_cache,
                                     DexCache* copy_dex_cache,
                                     MemberOffset array_offset,
                                     uint32_t size) {
  EntryType* orig_array = orig_dex_cache->GetFieldPtr64<EntryType*>(array_offset);
  DCHECK_EQ(orig_array != nullptr, size != 0u);
  if (orig_array != nullptr) {
    // Though the DexCache array fields are usually treated as native pointers, we clear
    // the top 32 bits for 32-bit targets.
    CopyAndFixupPointer(copy_dex_cache, array_offset, orig_array, PointerSize::k64);
    EntryType* new_array = NativeCopyLocation(orig_array);
    for (uint32_t i = 0; i != size; ++i) {
      FixupDexCacheArrayEntry(orig_array, new_array, i);
    }
  }
}

void ImageWriter::FixupDexCache(DexCache* orig_dex_cache, DexCache* copy_dex_cache) {
  FixupDexCacheArray<mirror::StringDexCacheType>(orig_dex_cache,
                                                 copy_dex_cache,
                                                 DexCache::StringsOffset(),
                                                 orig_dex_cache->NumStrings());
  FixupDexCacheArray<mirror::TypeDexCacheType>(orig_dex_cache,
                                               copy_dex_cache,
                                               DexCache::ResolvedTypesOffset(),
                                               orig_dex_cache->NumResolvedTypes());
  FixupDexCacheArray<mirror::MethodDexCacheType>(orig_dex_cache,
                                                 copy_dex_cache,
                                                 DexCache::ResolvedMethodsOffset(),
                                                 orig_dex_cache->NumResolvedMethods());
  FixupDexCacheArray<mirror::FieldDexCacheType>(orig_dex_cache,
                                                copy_dex_cache,
                                                DexCache::ResolvedFieldsOffset(),
                                                orig_dex_cache->NumResolvedFields());
  FixupDexCacheArray<mirror::MethodTypeDexCacheType>(orig_dex_cache,
                                                     copy_dex_cache,
                                                     DexCache::ResolvedMethodTypesOffset(),
                                                     orig_dex_cache->NumResolvedMethodTypes());
  FixupDexCacheArray<GcRoot<mirror::CallSite>>(orig_dex_cache,
                                               copy_dex_cache,
                                               DexCache::ResolvedCallSitesOffset(),
                                               orig_dex_cache->NumResolvedCallSites());
  if (orig_dex_cache->GetPreResolvedStrings() != nullptr) {
    CopyAndFixupPointer(copy_dex_cache,
                        DexCache::PreResolvedStringsOffset(),
                        orig_dex_cache->GetPreResolvedStrings(),
                        PointerSize::k64);
  }

  // Remove the DexFile pointers. They will be fixed up when the runtime loads the oat file. Leaving
  // compiler pointers in here will make the output non-deterministic.
  copy_dex_cache->SetDexFile(nullptr);
}

const uint8_t* ImageWriter::GetOatAddress(StubType type) const {
  DCHECK_LE(type, StubType::kLast);
  // If we are compiling an app image, we need to use the stubs of the boot image.
  if (!compiler_options_.IsBootImage()) {
    // Use the current image pointers.
    const std::vector<gc::space::ImageSpace*>& image_spaces =
        Runtime::Current()->GetHeap()->GetBootImageSpaces();
    DCHECK(!image_spaces.empty());
    const OatFile* oat_file = image_spaces[0]->GetOatFile();
    CHECK(oat_file != nullptr);
    const OatHeader& header = oat_file->GetOatHeader();
    switch (type) {
      // TODO: We could maybe clean this up if we stored them in an array in the oat header.
      case StubType::kQuickGenericJNITrampoline:
        return static_cast<const uint8_t*>(header.GetQuickGenericJniTrampoline());
      case StubType::kInterpreterToInterpreterBridge:
        return static_cast<const uint8_t*>(header.GetInterpreterToInterpreterBridge());
      case StubType::kInterpreterToCompiledCodeBridge:
        return static_cast<const uint8_t*>(header.GetInterpreterToCompiledCodeBridge());
      case StubType::kJNIDlsymLookup:
        return static_cast<const uint8_t*>(header.GetJniDlsymLookup());
      case StubType::kQuickIMTConflictTrampoline:
        return static_cast<const uint8_t*>(header.GetQuickImtConflictTrampoline());
      case StubType::kQuickResolutionTrampoline:
        return static_cast<const uint8_t*>(header.GetQuickResolutionTrampoline());
      case StubType::kQuickToInterpreterBridge:
        return static_cast<const uint8_t*>(header.GetQuickToInterpreterBridge());
      default:
        UNREACHABLE();
    }
  }
  const ImageInfo& primary_image_info = GetImageInfo(0);
  return GetOatAddressForOffset(primary_image_info.GetStubOffset(type), primary_image_info);
}

const uint8_t* ImageWriter::GetQuickCode(ArtMethod* method,
                                         const ImageInfo& image_info,
                                         bool* quick_is_interpreted) {
  DCHECK(!method->IsResolutionMethod()) << method->PrettyMethod();
  DCHECK_NE(method, Runtime::Current()->GetImtConflictMethod()) << method->PrettyMethod();
  DCHECK(!method->IsImtUnimplementedMethod()) << method->PrettyMethod();
  DCHECK(method->IsInvokable()) << method->PrettyMethod();
  DCHECK(!IsInBootImage(method)) << method->PrettyMethod();

  // Use original code if it exists. Otherwise, set the code pointer to the resolution
  // trampoline.

  // Quick entrypoint:
  const void* quick_oat_entry_point =
      method->GetEntryPointFromQuickCompiledCodePtrSize(target_ptr_size_);
  const uint8_t* quick_code;

  if (UNLIKELY(IsInBootImage(method->GetDeclaringClass().Ptr()))) {
    DCHECK(method->IsCopied());
    // If the code is not in the oat file corresponding to this image (e.g. default methods)
    quick_code = reinterpret_cast<const uint8_t*>(quick_oat_entry_point);
  } else {
    uint32_t quick_oat_code_offset = PointerToLowMemUInt32(quick_oat_entry_point);
    quick_code = GetOatAddressForOffset(quick_oat_code_offset, image_info);
  }

  *quick_is_interpreted = false;
  if (quick_code != nullptr && (!method->IsStatic() || method->IsConstructor() ||
      method->GetDeclaringClass()->IsInitialized())) {
    // We have code for a non-static or initialized method, just use the code.
  } else if (quick_code == nullptr && method->IsNative() &&
      (!method->IsStatic() || method->GetDeclaringClass()->IsInitialized())) {
    // Non-static or initialized native method missing compiled code, use generic JNI version.
    quick_code = GetOatAddress(StubType::kQuickGenericJNITrampoline);
  } else if (quick_code == nullptr && !method->IsNative()) {
    // We don't have code at all for a non-native method, use the interpreter.
    quick_code = GetOatAddress(StubType::kQuickToInterpreterBridge);
    *quick_is_interpreted = true;
  } else {
    CHECK(!method->GetDeclaringClass()->IsInitialized());
    // We have code for a static method, but need to go through the resolution stub for class
    // initialization.
    quick_code = GetOatAddress(StubType::kQuickResolutionTrampoline);
  }
  if (!IsInBootOatFile(quick_code)) {
    // DCHECK_GE(quick_code, oat_data_begin_);
  }
  return quick_code;
}

void ImageWriter::CopyAndFixupMethod(ArtMethod* orig,
                                     ArtMethod* copy,
                                     size_t oat_index) {
  if (orig->IsAbstract()) {
    // Ignore the single-implementation info for abstract method.
    // Do this on orig instead of copy, otherwise there is a crash due to methods
    // are copied before classes.
    // TODO: handle fixup of single-implementation method for abstract method.
    orig->SetHasSingleImplementation(false);
    orig->SetSingleImplementation(
        nullptr, Runtime::Current()->GetClassLinker()->GetImagePointerSize());
  }

  memcpy(copy, orig, ArtMethod::Size(target_ptr_size_));

  CopyAndFixupReference(
      copy->GetDeclaringClassAddressWithoutBarrier(), orig->GetDeclaringClassUnchecked());

  // OatWriter replaces the code_ with an offset value. Here we re-adjust to a pointer relative to
  // oat_begin_

  // The resolution method has a special trampoline to call.
  Runtime* runtime = Runtime::Current();
  const void* quick_code;
  if (orig->IsRuntimeMethod()) {
    ImtConflictTable* orig_table = orig->GetImtConflictTable(target_ptr_size_);
    if (orig_table != nullptr) {
      // Special IMT conflict method, normal IMT conflict method or unimplemented IMT method.
      quick_code = GetOatAddress(StubType::kQuickIMTConflictTrampoline);
      CopyAndFixupPointer(copy, ArtMethod::DataOffset(target_ptr_size_), orig_table);
    } else if (UNLIKELY(orig == runtime->GetResolutionMethod())) {
      quick_code = GetOatAddress(StubType::kQuickResolutionTrampoline);
    } else {
      bool found_one = false;
      for (size_t i = 0; i < static_cast<size_t>(CalleeSaveType::kLastCalleeSaveType); ++i) {
        auto idx = static_cast<CalleeSaveType>(i);
        if (runtime->HasCalleeSaveMethod(idx) && runtime->GetCalleeSaveMethod(idx) == orig) {
          found_one = true;
          break;
        }
      }
      CHECK(found_one) << "Expected to find callee save method but got " << orig->PrettyMethod();
      CHECK(copy->IsRuntimeMethod());
      CHECK(copy->GetEntryPointFromQuickCompiledCode() == nullptr);
      quick_code = nullptr;
    }
  } else {
    // We assume all methods have code. If they don't currently then we set them to the use the
    // resolution trampoline. Abstract methods never have code and so we need to make sure their
    // use results in an AbstractMethodError. We use the interpreter to achieve this.
    if (UNLIKELY(!orig->IsInvokable())) {
      quick_code = GetOatAddress(StubType::kQuickToInterpreterBridge);
    } else {
      bool quick_is_interpreted;
      const ImageInfo& image_info = image_infos_[oat_index];
      quick_code = GetQuickCode(orig, image_info, &quick_is_interpreted);

      // JNI entrypoint:
      if (orig->IsNative()) {
        // The native method's pointer is set to a stub to lookup via dlsym.
        // Note this is not the code_ pointer, that is handled above.
        copy->SetEntryPointFromJniPtrSize(
            GetOatAddress(StubType::kJNIDlsymLookup), target_ptr_size_);
      } else {
        CHECK(copy->GetDataPtrSize(target_ptr_size_) == nullptr);
      }
    }
  }
  if (quick_code != nullptr) {
    copy->SetEntryPointFromQuickCompiledCodePtrSize(quick_code, target_ptr_size_);
  }
}

size_t ImageWriter::ImageInfo::GetBinSizeSum(Bin up_to) const {
  DCHECK_LE(static_cast<size_t>(up_to), kNumberOfBins);
  return std::accumulate(&bin_slot_sizes_[0],
                         &bin_slot_sizes_[0] + static_cast<size_t>(up_to),
                         /*init*/ static_cast<size_t>(0));
}

ImageWriter::BinSlot::BinSlot(uint32_t lockword) : lockword_(lockword) {
  // These values may need to get updated if more bins are added to the enum Bin
  static_assert(kBinBits == 3, "wrong number of bin bits");
  static_assert(kBinShift == 27, "wrong number of shift");
  static_assert(sizeof(BinSlot) == sizeof(LockWord), "BinSlot/LockWord must have equal sizes");

  DCHECK_LT(GetBin(), Bin::kMirrorCount);
  DCHECK_ALIGNED(GetIndex(), kObjectAlignment);
}

ImageWriter::BinSlot::BinSlot(Bin bin, uint32_t index)
    : BinSlot(index | (static_cast<uint32_t>(bin) << kBinShift)) {
  DCHECK_EQ(index, GetIndex());
}

ImageWriter::Bin ImageWriter::BinSlot::GetBin() const {
  return static_cast<Bin>((lockword_ & kBinMask) >> kBinShift);
}

uint32_t ImageWriter::BinSlot::GetIndex() const {
  return lockword_ & ~kBinMask;
}

ImageWriter::Bin ImageWriter::BinTypeForNativeRelocationType(NativeObjectRelocationType type) {
  switch (type) {
    case NativeObjectRelocationType::kArtField:
    case NativeObjectRelocationType::kArtFieldArray:
      return Bin::kArtField;
    case NativeObjectRelocationType::kArtMethodClean:
    case NativeObjectRelocationType::kArtMethodArrayClean:
      return Bin::kArtMethodClean;
    case NativeObjectRelocationType::kArtMethodDirty:
    case NativeObjectRelocationType::kArtMethodArrayDirty:
      return Bin::kArtMethodDirty;
    case NativeObjectRelocationType::kDexCacheArray:
      return Bin::kDexCacheArray;
    case NativeObjectRelocationType::kRuntimeMethod:
      return Bin::kRuntimeMethod;
    case NativeObjectRelocationType::kIMTable:
      return Bin::kImTable;
    case NativeObjectRelocationType::kIMTConflictTable:
      return Bin::kIMTConflictTable;
    case NativeObjectRelocationType::kGcRootPointer:
      return Bin::kMetadata;
  }
  UNREACHABLE();
}

size_t ImageWriter::GetOatIndex(mirror::Object* obj) const {
  if (!IsMultiImage()) {
    return GetDefaultOatIndex();
  }
  auto it = oat_index_map_.find(obj);
  DCHECK(it != oat_index_map_.end()) << obj;
  return it->second;
}

size_t ImageWriter::GetOatIndexForDexFile(const DexFile* dex_file) const {
  if (!IsMultiImage()) {
    return GetDefaultOatIndex();
  }
  auto it = dex_file_oat_index_map_.find(dex_file);
  DCHECK(it != dex_file_oat_index_map_.end()) << dex_file->GetLocation();
  return it->second;
}

size_t ImageWriter::GetOatIndexForDexCache(ObjPtr<mirror::DexCache> dex_cache) const {
  return (dex_cache == nullptr)
      ? GetDefaultOatIndex()
      : GetOatIndexForDexFile(dex_cache->GetDexFile());
}

void ImageWriter::UpdateOatFileLayout(size_t oat_index,
                                      size_t oat_loaded_size,
                                      size_t oat_data_offset,
                                      size_t oat_data_size) {
  DCHECK_GE(oat_loaded_size, oat_data_offset);
  DCHECK_GE(oat_loaded_size - oat_data_offset, oat_data_size);

  const uint8_t* images_end = image_infos_.back().image_begin_ + image_infos_.back().image_size_;
  DCHECK(images_end != nullptr);  // Image space must be ready.
  for (const ImageInfo& info : image_infos_) {
    DCHECK_LE(info.image_begin_ + info.image_size_, images_end);
  }

  ImageInfo& cur_image_info = GetImageInfo(oat_index);
  cur_image_info.oat_file_begin_ = images_end + cur_image_info.oat_offset_;
  cur_image_info.oat_loaded_size_ = oat_loaded_size;
  cur_image_info.oat_data_begin_ = cur_image_info.oat_file_begin_ + oat_data_offset;
  cur_image_info.oat_size_ = oat_data_size;

  if (compiler_options_.IsAppImage()) {
    CHECK_EQ(oat_filenames_.size(), 1u) << "App image should have no next image.";
    return;
  }

  // Update the oat_offset of the next image info.
  if (oat_index + 1u != oat_filenames_.size()) {
    // There is a following one.
    ImageInfo& next_image_info = GetImageInfo(oat_index + 1u);
    next_image_info.oat_offset_ = cur_image_info.oat_offset_ + oat_loaded_size;
  }
}

void ImageWriter::UpdateOatFileHeader(size_t oat_index, const OatHeader& oat_header) {
  ImageInfo& cur_image_info = GetImageInfo(oat_index);
  cur_image_info.oat_checksum_ = oat_header.GetChecksum();

  if (oat_index == GetDefaultOatIndex()) {
    // Primary oat file, read the trampolines.
    cur_image_info.SetStubOffset(StubType::kInterpreterToInterpreterBridge,
                                 oat_header.GetInterpreterToInterpreterBridgeOffset());
    cur_image_info.SetStubOffset(StubType::kInterpreterToCompiledCodeBridge,
                                 oat_header.GetInterpreterToCompiledCodeBridgeOffset());
    cur_image_info.SetStubOffset(StubType::kJNIDlsymLookup,
                                 oat_header.GetJniDlsymLookupOffset());
    cur_image_info.SetStubOffset(StubType::kQuickGenericJNITrampoline,
                                 oat_header.GetQuickGenericJniTrampolineOffset());
    cur_image_info.SetStubOffset(StubType::kQuickIMTConflictTrampoline,
                                 oat_header.GetQuickImtConflictTrampolineOffset());
    cur_image_info.SetStubOffset(StubType::kQuickResolutionTrampoline,
                                 oat_header.GetQuickResolutionTrampolineOffset());
    cur_image_info.SetStubOffset(StubType::kQuickToInterpreterBridge,
                                 oat_header.GetQuickToInterpreterBridgeOffset());
  }
}

ImageWriter::ImageWriter(
    const CompilerOptions& compiler_options,
    uintptr_t image_begin,
    ImageHeader::StorageMode image_storage_mode,
    const std::vector<std::string>& oat_filenames,
    const std::unordered_map<const DexFile*, size_t>& dex_file_oat_index_map,
    jobject class_loader,
    const HashSet<std::string>* dirty_image_objects)
    : compiler_options_(compiler_options),
      global_image_begin_(reinterpret_cast<uint8_t*>(image_begin)),
      image_objects_offset_begin_(0),
      target_ptr_size_(InstructionSetPointerSize(compiler_options.GetInstructionSet())),
      image_infos_(oat_filenames.size()),
      dirty_methods_(0u),
      clean_methods_(0u),
      app_class_loader_(class_loader),
      boot_image_live_objects_(nullptr),
      image_storage_mode_(image_storage_mode),
      oat_filenames_(oat_filenames),
      dex_file_oat_index_map_(dex_file_oat_index_map),
      dirty_image_objects_(dirty_image_objects) {
  DCHECK(compiler_options.IsBootImage() || compiler_options.IsAppImage());
  CHECK_NE(image_begin, 0U);
  std::fill_n(image_methods_, arraysize(image_methods_), nullptr);
  CHECK_EQ(compiler_options.IsBootImage(),
           Runtime::Current()->GetHeap()->GetBootImageSpaces().empty())
      << "Compiling a boot image should occur iff there are no boot image spaces loaded";
}

ImageWriter::ImageInfo::ImageInfo()
    : intern_table_(new InternTable),
      class_table_(new ClassTable) {}

template <typename DestType>
void ImageWriter::CopyAndFixupReference(DestType* dest, ObjPtr<mirror::Object> src) {
  static_assert(std::is_same<DestType, mirror::CompressedReference<mirror::Object>>::value ||
                    std::is_same<DestType, mirror::HeapReference<mirror::Object>>::value,
                "DestType must be a Compressed-/HeapReference<Object>.");
  dest->Assign(GetImageAddress(src.Ptr()));
}

void ImageWriter::CopyAndFixupPointer(void** target, void* value, PointerSize pointer_size) {
  void* new_value = NativeLocationInImage(value);
  if (pointer_size == PointerSize::k32) {
    *reinterpret_cast<uint32_t*>(target) = reinterpret_cast32<uint32_t>(new_value);
  } else {
    *reinterpret_cast<uint64_t*>(target) = reinterpret_cast64<uint64_t>(new_value);
  }
  DCHECK(value != nullptr);
}

void ImageWriter::CopyAndFixupPointer(void** target, void* value)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  CopyAndFixupPointer(target, value, target_ptr_size_);
}

void ImageWriter::CopyAndFixupPointer(
    void* object, MemberOffset offset, void* value, PointerSize pointer_size) {
  void** target =
      reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(object) + offset.Uint32Value());
  return CopyAndFixupPointer(target, value, pointer_size);
}

void ImageWriter::CopyAndFixupPointer(void* object, MemberOffset offset, void* value) {
  return CopyAndFixupPointer(object, offset, value, target_ptr_size_);
}

}  // namespace linker
}  // namespace art
