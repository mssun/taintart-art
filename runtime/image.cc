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

#include "image.h"

#include "base/bit_utils.h"
#include "base/length_prefixed_array.h"
#include "base/utils.h"
#include "mirror/object-inl.h"
#include "mirror/object_array-inl.h"
#include "mirror/object_array.h"

namespace art {

const uint8_t ImageHeader::kImageMagic[] = { 'a', 'r', 't', '\n' };
const uint8_t ImageHeader::kImageVersion[] = { '0', '6', '9', '\0' };  // Remove boot oat extents.

ImageHeader::ImageHeader(uint32_t image_begin,
                         uint32_t image_size,
                         ImageSection* sections,
                         uint32_t image_roots,
                         uint32_t oat_checksum,
                         uint32_t oat_file_begin,
                         uint32_t oat_data_begin,
                         uint32_t oat_data_end,
                         uint32_t oat_file_end,
                         uint32_t boot_image_begin,
                         uint32_t boot_image_size,
                         uint32_t pointer_size,
                         StorageMode storage_mode,
                         size_t data_size)
  : image_begin_(image_begin),
    image_size_(image_size),
    image_checksum_(0u),
    oat_checksum_(oat_checksum),
    oat_file_begin_(oat_file_begin),
    oat_data_begin_(oat_data_begin),
    oat_data_end_(oat_data_end),
    oat_file_end_(oat_file_end),
    boot_image_begin_(boot_image_begin),
    boot_image_size_(boot_image_size),
    image_roots_(image_roots),
    pointer_size_(pointer_size),
    storage_mode_(storage_mode),
    data_size_(data_size) {
  CHECK_EQ(image_begin, RoundUp(image_begin, kPageSize));
  CHECK_EQ(oat_file_begin, RoundUp(oat_file_begin, kPageSize));
  CHECK_EQ(oat_data_begin, RoundUp(oat_data_begin, kPageSize));
  CHECK_LT(image_roots, oat_file_begin);
  CHECK_LE(oat_file_begin, oat_data_begin);
  CHECK_LT(oat_data_begin, oat_data_end);
  CHECK_LE(oat_data_end, oat_file_end);
  CHECK(ValidPointerSize(pointer_size_)) << pointer_size_;
  memcpy(magic_, kImageMagic, sizeof(kImageMagic));
  memcpy(version_, kImageVersion, sizeof(kImageVersion));
  std::copy_n(sections, kSectionCount, sections_);
}

void ImageHeader::RelocateImage(int64_t delta) {
  CHECK_ALIGNED(delta, kPageSize) << " patch delta must be page aligned";
  oat_file_begin_ += delta;
  oat_data_begin_ += delta;
  oat_data_end_ += delta;
  oat_file_end_ += delta;
  RelocateImageObjects(delta);
  RelocateImageMethods(delta);
}

void ImageHeader::RelocateImageObjects(int64_t delta) {
  image_begin_ += delta;
  image_roots_ += delta;
}

void ImageHeader::RelocateImageMethods(int64_t delta) {
  for (size_t i = 0; i < kImageMethodsCount; ++i) {
    image_methods_[i] += delta;
  }
}

bool ImageHeader::IsValid() const {
  if (memcmp(magic_, kImageMagic, sizeof(kImageMagic)) != 0) {
    return false;
  }
  if (memcmp(version_, kImageVersion, sizeof(kImageVersion)) != 0) {
    return false;
  }
  // Unsigned so wraparound is well defined.
  if (image_begin_ >= image_begin_ + image_size_) {
    return false;
  }
  if (oat_file_begin_ > oat_file_end_) {
    return false;
  }
  if (oat_data_begin_ > oat_data_end_) {
    return false;
  }
  if (oat_file_begin_ >= oat_data_begin_) {
    return false;
  }
  return true;
}

const char* ImageHeader::GetMagic() const {
  CHECK(IsValid());
  return reinterpret_cast<const char*>(magic_);
}

ArtMethod* ImageHeader::GetImageMethod(ImageMethod index) const {
  CHECK_LT(static_cast<size_t>(index), kImageMethodsCount);
  return reinterpret_cast<ArtMethod*>(image_methods_[index]);
}

std::ostream& operator<<(std::ostream& os, const ImageSection& section) {
  return os << "size=" << section.Size() << " range=" << section.Offset() << "-" << section.End();
}

void ImageHeader::VisitObjects(ObjectVisitor* visitor,
                               uint8_t* base,
                               PointerSize pointer_size) const {
  DCHECK_EQ(pointer_size, GetPointerSize());
  const ImageSection& objects = GetObjectsSection();
  static const size_t kStartPos = RoundUp(sizeof(ImageHeader), kObjectAlignment);
  for (size_t pos = kStartPos; pos < objects.Size(); ) {
    mirror::Object* object = reinterpret_cast<mirror::Object*>(base + objects.Offset() + pos);
    visitor->Visit(object);
    pos += RoundUp(object->SizeOf(), kObjectAlignment);
  }
}

PointerSize ImageHeader::GetPointerSize() const {
  return ConvertToPointerSize(pointer_size_);
}

}  // namespace art
