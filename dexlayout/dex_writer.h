/*
 * Copyright (C) 2016 The Android Open Source Project
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
 *
 * Header file of an in-memory representation of DEX files.
 */

#ifndef ART_DEXLAYOUT_DEX_WRITER_H_
#define ART_DEXLAYOUT_DEX_WRITER_H_

#include <functional>
#include <memory>  // For unique_ptr

#include "base/unix_file/fd_file.h"
#include "dex/compact_dex_level.h"
#include "dex_container.h"
#include "dex/dex_file.h"
#include "dex_ir.h"
#include "mem_map.h"
#include "os.h"

#include <queue>

namespace art {

class DexLayout;
class DexLayoutHotnessInfo;

struct MapItem {
  // Not using DexFile::MapItemType since compact dex and standard dex file may have different
  // sections.
  MapItem() = default;
  MapItem(uint32_t type, uint32_t size, size_t offset)
      : type_(type), size_(size), offset_(offset) { }

  // Sort by decreasing order since the priority_queue puts largest elements first.
  bool operator>(const MapItem& other) const {
    return offset_ > other.offset_;
  }

  uint32_t type_ = 0u;
  uint32_t size_ = 0u;
  uint32_t offset_ = 0u;
};

class MapItemQueue : public
    std::priority_queue<MapItem, std::vector<MapItem>, std::greater<MapItem>> {
 public:
  void AddIfNotEmpty(const MapItem& item);
};

class DexWriter {
 public:
  static constexpr uint32_t kDataSectionAlignment = sizeof(uint32_t) * 2;
  static constexpr uint32_t kDexSectionWordAlignment = 4;

  // Stream that writes into a dex container section. Do not have two streams pointing to the same
  // backing storage as there may be invalidation of backing storage to resize the section.
  // Random access stream (consider refactoring).
  class Stream {
   public:
    explicit Stream(DexContainer::Section* section) : section_(section) {
      SyncWithSection();
    }

    const uint8_t* Begin() const {
      return data_;
    }

    // Functions are not virtual (yet) for speed.
    size_t Tell() const {
      return position_;
    }

    void Seek(size_t position) {
      position_ = position;
    }

    // Does not allow overwriting for bug prevention purposes.
    ALWAYS_INLINE size_t Write(const void* buffer, size_t length) {
      EnsureStorage(length);
      for (size_t i = 0; i < length; ++i) {
        DCHECK_EQ(data_[position_ + i], 0u);
      }
      memcpy(&data_[position_], buffer, length);
      position_ += length;
      return length;
    }

    ALWAYS_INLINE size_t Overwrite(const void* buffer, size_t length) {
      EnsureStorage(length);
      memcpy(&data_[position_], buffer, length);
      position_ += length;
      return length;
    }

    ALWAYS_INLINE size_t Clear(size_t position, size_t length) {
      EnsureStorage(length);
      memset(&data_[position], 0, length);
      return length;
    }

    ALWAYS_INLINE size_t WriteSleb128(int32_t value) {
      EnsureStorage(8);
      uint8_t* ptr = &data_[position_];
      const size_t len = EncodeSignedLeb128(ptr, value) - ptr;
      position_ += len;
      return len;
    }

    ALWAYS_INLINE size_t WriteUleb128(uint32_t value) {
      EnsureStorage(8);
      uint8_t* ptr = &data_[position_];
      const size_t len = EncodeUnsignedLeb128(ptr, value) - ptr;
      position_ += len;
      return len;
    }

    ALWAYS_INLINE void AlignTo(const size_t alignment) {
      position_ = RoundUp(position_, alignment);
    }

    ALWAYS_INLINE void Skip(const size_t count) {
      position_ += count;
    }

    class ScopedSeek {
     public:
      ScopedSeek(Stream* stream, uint32_t offset) : stream_(stream), offset_(stream->Tell()) {
        stream->Seek(offset);
      }

      ~ScopedSeek() {
        stream_->Seek(offset_);
      }

     private:
      Stream* const stream_;
      const uint32_t offset_;
    };

   private:
    ALWAYS_INLINE void EnsureStorage(size_t length) {
      size_t end = position_ + length;
      while (UNLIKELY(end > data_size_)) {
        section_->Resize(data_size_ * 3 / 2 + 1);
        SyncWithSection();
      }
    }

    void SyncWithSection() {
      data_ = section_->Begin();
      data_size_ = section_->Size();
    }

    // Current position of the stream.
    size_t position_ = 0u;
    DexContainer::Section* const section_ = nullptr;
    // Cached Begin() from the container to provide faster accesses.
    uint8_t* data_ = nullptr;
    // Cached Size from the container to provide faster accesses.
    size_t data_size_ = 0u;
  };

  static inline constexpr uint32_t SectionAlignment(DexFile::MapItemType type) {
    switch (type) {
      case DexFile::kDexTypeClassDataItem:
      case DexFile::kDexTypeStringDataItem:
      case DexFile::kDexTypeDebugInfoItem:
      case DexFile::kDexTypeAnnotationItem:
      case DexFile::kDexTypeEncodedArrayItem:
        return alignof(uint8_t);

      default:
        // All other sections are kDexAlignedSection.
        return DexWriter::kDexSectionWordAlignment;
    }
  }

  class Container : public DexContainer {
   public:
    Section* GetMainSection() OVERRIDE {
      return &main_section_;
    }

    Section* GetDataSection() OVERRIDE {
      return &data_section_;
    }

    bool IsCompactDexContainer() const OVERRIDE {
      return false;
    }

   private:
    VectorSection main_section_;
    VectorSection data_section_;

    friend class CompactDexWriter;
  };

  DexWriter(DexLayout* dex_layout, bool compute_offsets);

  static void Output(DexLayout* dex_layout,
                     std::unique_ptr<DexContainer>* container,
                     bool compute_offsets);

  virtual ~DexWriter() {}

 protected:
  virtual void Write(DexContainer* output);
  virtual std::unique_ptr<DexContainer> CreateDexContainer() const;

  size_t WriteEncodedValue(Stream* stream, dex_ir::EncodedValue* encoded_value);
  size_t WriteEncodedValueHeader(Stream* stream, int8_t value_type, size_t value_arg);
  size_t WriteEncodedArray(Stream* stream, dex_ir::EncodedValueVector* values);
  size_t WriteEncodedAnnotation(Stream* stream, dex_ir::EncodedAnnotation* annotation);
  size_t WriteEncodedFields(Stream* stream, dex_ir::FieldItemVector* fields);
  size_t WriteEncodedMethods(Stream* stream, dex_ir::MethodItemVector* methods);

  // Header and id section
  virtual void WriteHeader(Stream* stream);
  virtual size_t GetHeaderSize() const;
  // reserve_only means don't write, only reserve space. This is required since the string data
  // offsets must be assigned.
  uint32_t WriteStringIds(Stream* stream, bool reserve_only);
  uint32_t WriteTypeIds(Stream* stream);
  uint32_t WriteProtoIds(Stream* stream, bool reserve_only);
  uint32_t WriteFieldIds(Stream* stream);
  uint32_t WriteMethodIds(Stream* stream);
  uint32_t WriteClassDefs(Stream* stream, bool reserve_only);
  uint32_t WriteCallSiteIds(Stream* stream, bool reserve_only);

  uint32_t WriteEncodedArrays(Stream* stream);
  uint32_t WriteAnnotations(Stream* stream);
  uint32_t WriteAnnotationSets(Stream* stream);
  uint32_t WriteAnnotationSetRefs(Stream* stream);
  uint32_t WriteAnnotationsDirectories(Stream* stream);

  // Data section.
  uint32_t WriteDebugInfoItems(Stream* stream);
  uint32_t WriteCodeItems(Stream* stream, bool reserve_only);
  uint32_t WriteTypeLists(Stream* stream);
  uint32_t WriteStringDatas(Stream* stream);
  uint32_t WriteClassDatas(Stream* stream);
  uint32_t WriteMethodHandles(Stream* stream);
  uint32_t WriteMapItems(Stream* stream, MapItemQueue* queue);
  uint32_t GenerateAndWriteMapItems(Stream* stream);

  virtual uint32_t WriteCodeItemPostInstructionData(Stream* stream,
                                                    dex_ir::CodeItem* item,
                                                    bool reserve_only);
  virtual void WriteCodeItem(Stream* stream, dex_ir::CodeItem* item, bool reserve_only);
  virtual void WriteDebugInfoItem(Stream* stream, dex_ir::DebugInfoItem* debug_info);
  virtual void WriteStringData(Stream* stream, dex_ir::StringData* string_data);

  // Process an offset, if compute_offset is set, write into the dex ir item, otherwise read the
  // existing offset and use that for writing.
  void ProcessOffset(Stream* stream, dex_ir::Item* item);

  dex_ir::Header* const header_;
  DexLayout* const dex_layout_;
  bool compute_offsets_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DexWriter);
};

}  // namespace art

#endif  // ART_DEXLAYOUT_DEX_WRITER_H_
