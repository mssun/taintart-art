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

#ifndef ART_DEXLAYOUT_DEX_IR_H_
#define ART_DEXLAYOUT_DEX_IR_H_

#include <stdint.h>

#include <map>
#include <vector>

#include "base/iteration_range.h"
#include "base/leb128.h"
#include "base/stl_util.h"
#include "dex/dex_file-inl.h"
#include "dex/dex_file_types.h"
#include "dex/utf.h"

namespace art {
namespace dex_ir {

// Forward declarations for classes used in containers or pointed to.
class AnnotationItem;
class AnnotationsDirectoryItem;
class AnnotationSetItem;
class AnnotationSetRefList;
class CallSiteId;
class ClassData;
class ClassDef;
class CodeItem;
class DebugInfoItem;
class EncodedAnnotation;
class EncodedArrayItem;
class EncodedValue;
class FieldId;
class FieldItem;
class Header;
class MapList;
class MapItem;
class MethodHandleItem;
class MethodId;
class MethodItem;
class ParameterAnnotation;
class ProtoId;
class StringData;
class StringId;
class TryItem;
class TypeId;
class TypeList;

// Item size constants.
static constexpr size_t kHeaderItemSize = 112;
static constexpr size_t kStringIdItemSize = 4;
static constexpr size_t kTypeIdItemSize = 4;
static constexpr size_t kProtoIdItemSize = 12;
static constexpr size_t kFieldIdItemSize = 8;
static constexpr size_t kMethodIdItemSize = 8;
static constexpr size_t kClassDefItemSize = 32;
static constexpr size_t kCallSiteIdItemSize = 4;
static constexpr size_t kMethodHandleItemSize = 8;

// Visitor support
class AbstractDispatcher {
 public:
  AbstractDispatcher() = default;
  virtual ~AbstractDispatcher() { }

  virtual void Dispatch(Header* header) = 0;
  virtual void Dispatch(const StringData* string_data) = 0;
  virtual void Dispatch(const StringId* string_id) = 0;
  virtual void Dispatch(const TypeId* type_id) = 0;
  virtual void Dispatch(const ProtoId* proto_id) = 0;
  virtual void Dispatch(const FieldId* field_id) = 0;
  virtual void Dispatch(const MethodId* method_id) = 0;
  virtual void Dispatch(const CallSiteId* call_site_id) = 0;
  virtual void Dispatch(const MethodHandleItem* method_handle_item) = 0;
  virtual void Dispatch(ClassData* class_data) = 0;
  virtual void Dispatch(ClassDef* class_def) = 0;
  virtual void Dispatch(FieldItem* field_item) = 0;
  virtual void Dispatch(MethodItem* method_item) = 0;
  virtual void Dispatch(EncodedArrayItem* array_item) = 0;
  virtual void Dispatch(CodeItem* code_item) = 0;
  virtual void Dispatch(TryItem* try_item) = 0;
  virtual void Dispatch(DebugInfoItem* debug_info_item) = 0;
  virtual void Dispatch(AnnotationItem* annotation_item) = 0;
  virtual void Dispatch(AnnotationSetItem* annotation_set_item) = 0;
  virtual void Dispatch(AnnotationSetRefList* annotation_set_ref_list) = 0;
  virtual void Dispatch(AnnotationsDirectoryItem* annotations_directory_item) = 0;
  virtual void Dispatch(MapList* map_list) = 0;
  virtual void Dispatch(MapItem* map_item) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AbstractDispatcher);
};

template<class T> class Iterator : public std::iterator<std::random_access_iterator_tag, T> {
 public:
  using value_type = typename std::iterator<std::random_access_iterator_tag, T>::value_type;
  using difference_type =
      typename std::iterator<std::random_access_iterator_tag, value_type>::difference_type;
  using pointer = typename std::iterator<std::random_access_iterator_tag, value_type>::pointer;
  using reference = typename std::iterator<std::random_access_iterator_tag, value_type>::reference;

  Iterator(const Iterator&) = default;
  Iterator(Iterator&&) = default;
  Iterator& operator=(const Iterator&) = default;
  Iterator& operator=(Iterator&&) = default;

  Iterator(const std::vector<T>& vector,
           uint32_t position,
           uint32_t iterator_end)
      : vector_(&vector),
        position_(position),
        iterator_end_(iterator_end) { }
  Iterator() : vector_(nullptr), position_(0U), iterator_end_(0U) { }

  bool IsValid() const { return position_ < iterator_end_; }

  bool operator==(const Iterator& rhs) const { return position_ == rhs.position_; }
  bool operator!=(const Iterator& rhs) const { return !(*this == rhs); }
  bool operator<(const Iterator& rhs) const { return position_ < rhs.position_; }
  bool operator>(const Iterator& rhs) const { return rhs < *this; }
  bool operator<=(const Iterator& rhs) const { return !(rhs < *this); }
  bool operator>=(const Iterator& rhs) const { return !(*this < rhs); }

  Iterator& operator++() {  // Value after modification.
    ++position_;
    return *this;
  }

  Iterator operator++(int) {
    Iterator temp = *this;
    ++position_;
    return temp;
  }

  Iterator& operator+=(difference_type delta) {
    position_ += delta;
    return *this;
  }

  Iterator operator+(difference_type delta) const {
    Iterator temp = *this;
    temp += delta;
    return temp;
  }

  Iterator& operator--() {  // Value after modification.
    --position_;
    return *this;
  }

  Iterator operator--(int) {
    Iterator temp = *this;
    --position_;
    return temp;
  }

  Iterator& operator-=(difference_type delta) {
    position_ -= delta;
    return *this;
  }

  Iterator operator-(difference_type delta) const {
    Iterator temp = *this;
    temp -= delta;
    return temp;
  }

  difference_type operator-(const Iterator& rhs) {
    return position_ - rhs.position_;
  }

  reference operator*() const {
    return const_cast<reference>((*vector_)[position_]);
  }

  pointer operator->() const {
    return const_cast<pointer>(&((*vector_)[position_]));
  }

  reference operator[](difference_type n) const {
    return (*vector_)[position_ + n];
  }

 private:
  const std::vector<T>* vector_;
  uint32_t position_;
  uint32_t iterator_end_;

  template <typename U>
  friend bool operator<(const Iterator<U>& lhs, const Iterator<U>& rhs);
};

// Collections become owners of the objects added by moving them into unique pointers.
class CollectionBase {
 public:
  CollectionBase() = default;
  virtual ~CollectionBase() { }

  uint32_t GetOffset() const { return offset_; }
  void SetOffset(uint32_t new_offset) { offset_ = new_offset; }
  virtual uint32_t Size() const { return 0U; }

 private:
  // Start out unassigned.
  uint32_t offset_ = 0u;

  DISALLOW_COPY_AND_ASSIGN(CollectionBase);
};

template<class T> class CollectionVector : public CollectionBase {
 public:
  using ElementType = std::unique_ptr<T>;

  CollectionVector() { }
  explicit CollectionVector(size_t size) {
    // Preallocate so that assignment does not invalidate pointers into the vector.
    collection_.reserve(size);
  }
  virtual ~CollectionVector() OVERRIDE { }

  template<class... Args>
  T* CreateAndAddItem(Args&&... args) {
    T* object = new T(std::forward<Args>(args)...);
    collection_.push_back(std::unique_ptr<T>(object));
    return object;
  }

  virtual uint32_t Size() const OVERRIDE { return collection_.size(); }

  Iterator<ElementType> begin() const { return Iterator<ElementType>(collection_, 0U, Size()); }
  Iterator<ElementType> end() const { return Iterator<ElementType>(collection_, Size(), Size()); }

  const ElementType& operator[](size_t index) const {
    DCHECK_LT(index, Size());
    return collection_[index];
  }
  ElementType& operator[](size_t index) {
    DCHECK_LT(index, Size());
    return collection_[index];
  }

  // Sort the vector by copying pointers over.
  template <typename MapType>
  void SortByMapOrder(const MapType& map) {
    auto it = map.begin();
    CHECK_EQ(map.size(), Size());
    for (size_t i = 0; i < Size(); ++i) {
      // There are times when the array will temporarily contain the same pointer twice, doing the
      // release here sure there is no double free errors.
      collection_[i].release();
      collection_[i].reset(it->second);
      ++it;
    }
  }

 protected:
  std::vector<ElementType> collection_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CollectionVector);
};

template<class T> class IndexedCollectionVector : public CollectionVector<T> {
 public:
  using Vector = std::vector<std::unique_ptr<T>>;
  IndexedCollectionVector() = default;
  explicit IndexedCollectionVector(size_t size) : CollectionVector<T>(size) { }

  template <class... Args>
  T* CreateAndAddIndexedItem(uint32_t index, Args&&... args) {
    T* object = CollectionVector<T>::CreateAndAddItem(std::forward<Args>(args)...);
    object->SetIndex(index);
    return object;
  }

  T* operator[](size_t index) const {
    DCHECK_NE(CollectionVector<T>::collection_[index].get(), static_cast<T*>(nullptr));
    return CollectionVector<T>::collection_[index].get();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(IndexedCollectionVector);
};

class Item {
 public:
  Item() { }
  virtual ~Item() { }

  Item(Item&&) = default;

  // Return the assigned offset.
  uint32_t GetOffset() const WARN_UNUSED {
    CHECK(OffsetAssigned());
    return offset_;
  }
  uint32_t GetSize() const WARN_UNUSED { return size_; }
  void SetOffset(uint32_t offset) { offset_ = offset; }
  void SetSize(uint32_t size) { size_ = size; }
  bool OffsetAssigned() const {
    return offset_ != kOffsetUnassigned;
  }

 protected:
  Item(uint32_t offset, uint32_t size) : offset_(offset), size_(size) { }

  // 0 is the dex file header and shouldn't be a valid offset for any part of the dex file.
  static constexpr uint32_t kOffsetUnassigned = 0u;

  // Start out unassigned.
  uint32_t offset_ = kOffsetUnassigned;
  uint32_t size_ = 0;
};

class IndexedItem : public Item {
 public:
  IndexedItem() { }
  virtual ~IndexedItem() { }

  uint32_t GetIndex() const { return index_; }
  void SetIndex(uint32_t index) { index_ = index; }

 protected:
  IndexedItem(uint32_t offset, uint32_t size, uint32_t index)
      : Item(offset, size), index_(index) { }

  uint32_t index_ = 0;
};

class Header : public Item {
 public:
  Header(const uint8_t* magic,
         uint32_t checksum,
         const uint8_t* signature,
         uint32_t endian_tag,
         uint32_t file_size,
         uint32_t header_size,
         uint32_t link_size,
         uint32_t link_offset,
         uint32_t data_size,
         uint32_t data_offset,
         bool support_default_methods)
      : Item(0, kHeaderItemSize), support_default_methods_(support_default_methods) {
    ConstructorHelper(magic,
                      checksum,
                      signature,
                      endian_tag,
                      file_size,
                      header_size,
                      link_size,
                      link_offset,
                      data_size,
                      data_offset);
  }

  Header(const uint8_t* magic,
         uint32_t checksum,
         const uint8_t* signature,
         uint32_t endian_tag,
         uint32_t file_size,
         uint32_t header_size,
         uint32_t link_size,
         uint32_t link_offset,
         uint32_t data_size,
         uint32_t data_offset,
         bool support_default_methods,
         uint32_t num_string_ids,
         uint32_t num_type_ids,
         uint32_t num_proto_ids,
         uint32_t num_field_ids,
         uint32_t num_method_ids,
         uint32_t num_class_defs)
      : Item(0, kHeaderItemSize),
        support_default_methods_(support_default_methods),
        string_ids_(num_string_ids),
        type_ids_(num_type_ids),
        proto_ids_(num_proto_ids),
        field_ids_(num_field_ids),
        method_ids_(num_method_ids),
        class_defs_(num_class_defs) {
    ConstructorHelper(magic,
                      checksum,
                      signature,
                      endian_tag,
                      file_size,
                      header_size,
                      link_size,
                      link_offset,
                      data_size,
                      data_offset);
  }
  ~Header() OVERRIDE { }

  static size_t ItemSize() { return kHeaderItemSize; }

  const uint8_t* Magic() const { return magic_; }
  uint32_t Checksum() const { return checksum_; }
  const uint8_t* Signature() const { return signature_; }
  uint32_t EndianTag() const { return endian_tag_; }
  uint32_t FileSize() const { return file_size_; }
  uint32_t HeaderSize() const { return header_size_; }
  uint32_t LinkSize() const { return link_size_; }
  uint32_t LinkOffset() const { return link_offset_; }
  uint32_t DataSize() const { return data_size_; }
  uint32_t DataOffset() const { return data_offset_; }

  void SetChecksum(uint32_t new_checksum) { checksum_ = new_checksum; }
  void SetSignature(const uint8_t* new_signature) {
    memcpy(signature_, new_signature, sizeof(signature_));
  }
  void SetFileSize(uint32_t new_file_size) { file_size_ = new_file_size; }
  void SetHeaderSize(uint32_t new_header_size) { header_size_ = new_header_size; }
  void SetLinkSize(uint32_t new_link_size) { link_size_ = new_link_size; }
  void SetLinkOffset(uint32_t new_link_offset) { link_offset_ = new_link_offset; }
  void SetDataSize(uint32_t new_data_size) { data_size_ = new_data_size; }
  void SetDataOffset(uint32_t new_data_offset) { data_offset_ = new_data_offset; }

  IndexedCollectionVector<StringId>& StringIds() { return string_ids_; }
  const IndexedCollectionVector<StringId>& StringIds() const { return string_ids_; }
  IndexedCollectionVector<TypeId>& TypeIds() { return type_ids_; }
  const IndexedCollectionVector<TypeId>& TypeIds() const { return type_ids_; }
  IndexedCollectionVector<ProtoId>& ProtoIds() { return proto_ids_; }
  const IndexedCollectionVector<ProtoId>& ProtoIds() const { return proto_ids_; }
  IndexedCollectionVector<FieldId>& FieldIds() { return field_ids_; }
  const IndexedCollectionVector<FieldId>& FieldIds() const { return field_ids_; }
  IndexedCollectionVector<MethodId>& MethodIds() { return method_ids_; }
  const IndexedCollectionVector<MethodId>& MethodIds() const { return method_ids_; }
  IndexedCollectionVector<ClassDef>& ClassDefs() { return class_defs_; }
  const IndexedCollectionVector<ClassDef>& ClassDefs() const { return class_defs_; }
  IndexedCollectionVector<CallSiteId>& CallSiteIds() { return call_site_ids_; }
  const IndexedCollectionVector<CallSiteId>& CallSiteIds() const { return call_site_ids_; }
  IndexedCollectionVector<MethodHandleItem>& MethodHandleItems() { return method_handle_items_; }
  const IndexedCollectionVector<MethodHandleItem>& MethodHandleItems() const {
    return method_handle_items_;
  }
  CollectionVector<StringData>& StringDatas() { return string_datas_; }
  const CollectionVector<StringData>& StringDatas() const { return string_datas_; }
  CollectionVector<TypeList>& TypeLists() { return type_lists_; }
  const CollectionVector<TypeList>& TypeLists() const { return type_lists_; }
  CollectionVector<EncodedArrayItem>& EncodedArrayItems() { return encoded_array_items_; }
  const CollectionVector<EncodedArrayItem>& EncodedArrayItems() const {
    return encoded_array_items_;
  }
  CollectionVector<AnnotationItem>& AnnotationItems() { return annotation_items_; }
  const CollectionVector<AnnotationItem>& AnnotationItems() const { return annotation_items_; }
  CollectionVector<AnnotationSetItem>& AnnotationSetItems() { return annotation_set_items_; }
  const CollectionVector<AnnotationSetItem>& AnnotationSetItems() const {
    return annotation_set_items_;
  }
  CollectionVector<AnnotationSetRefList>& AnnotationSetRefLists() {
    return annotation_set_ref_lists_;
  }
  const CollectionVector<AnnotationSetRefList>& AnnotationSetRefLists() const {
    return annotation_set_ref_lists_;
  }
  CollectionVector<AnnotationsDirectoryItem>& AnnotationsDirectoryItems() {
    return annotations_directory_items_;
  }
  const CollectionVector<AnnotationsDirectoryItem>& AnnotationsDirectoryItems() const {
    return annotations_directory_items_;
  }
  CollectionVector<DebugInfoItem>& DebugInfoItems() { return debug_info_items_; }
  const CollectionVector<DebugInfoItem>& DebugInfoItems() const { return debug_info_items_; }
  CollectionVector<CodeItem>& CodeItems() { return code_items_; }
  const CollectionVector<CodeItem>& CodeItems() const { return code_items_; }
  CollectionVector<ClassData>& ClassDatas() { return class_datas_; }
  const CollectionVector<ClassData>& ClassDatas() const { return class_datas_; }

  StringId* GetStringIdOrNullPtr(uint32_t index) {
    return index == dex::kDexNoIndex ? nullptr : StringIds()[index];
  }
  TypeId* GetTypeIdOrNullPtr(uint16_t index) {
    return index == DexFile::kDexNoIndex16 ? nullptr : TypeIds()[index];
  }

  uint32_t MapListOffset() const { return map_list_offset_; }
  void SetMapListOffset(uint32_t new_offset) { map_list_offset_ = new_offset; }

  const std::vector<uint8_t>& LinkData() const { return link_data_; }
  void SetLinkData(std::vector<uint8_t>&& link_data) { link_data_ = std::move(link_data); }

  void Accept(AbstractDispatcher* dispatch) { dispatch->Dispatch(this); }

  bool SupportDefaultMethods() const {
    return support_default_methods_;
  }

 private:
  uint8_t magic_[8];
  uint32_t checksum_;
  uint8_t signature_[DexFile::kSha1DigestSize];
  uint32_t endian_tag_;
  uint32_t file_size_;
  uint32_t header_size_;
  uint32_t link_size_;
  uint32_t link_offset_;
  uint32_t data_size_;
  uint32_t data_offset_;
  const bool support_default_methods_;

  void ConstructorHelper(const uint8_t* magic,
                         uint32_t checksum,
                         const uint8_t* signature,
                         uint32_t endian_tag,
                         uint32_t file_size,
                         uint32_t header_size,
                         uint32_t link_size,
                         uint32_t link_offset,
                         uint32_t data_size,
                         uint32_t data_offset) {
    checksum_ = checksum;
    endian_tag_ = endian_tag;
    file_size_ = file_size;
    header_size_ = header_size;
    link_size_ = link_size;
    link_offset_ = link_offset;
    data_size_ = data_size;
    data_offset_ = data_offset;
    memcpy(magic_, magic, sizeof(magic_));
    memcpy(signature_, signature, sizeof(signature_));
  }

  // Collection vectors own the IR data.
  IndexedCollectionVector<StringId> string_ids_;
  IndexedCollectionVector<TypeId> type_ids_;
  IndexedCollectionVector<ProtoId> proto_ids_;
  IndexedCollectionVector<FieldId> field_ids_;
  IndexedCollectionVector<MethodId> method_ids_;
  IndexedCollectionVector<ClassDef> class_defs_;
  IndexedCollectionVector<CallSiteId> call_site_ids_;
  IndexedCollectionVector<MethodHandleItem> method_handle_items_;
  IndexedCollectionVector<StringData> string_datas_;
  IndexedCollectionVector<TypeList> type_lists_;
  IndexedCollectionVector<EncodedArrayItem> encoded_array_items_;
  IndexedCollectionVector<AnnotationItem> annotation_items_;
  IndexedCollectionVector<AnnotationSetItem> annotation_set_items_;
  IndexedCollectionVector<AnnotationSetRefList> annotation_set_ref_lists_;
  IndexedCollectionVector<AnnotationsDirectoryItem> annotations_directory_items_;
  // The order of the vectors controls the layout of the output file by index order, to change the
  // layout just sort the vector. Note that you may only change the order of the non indexed vectors
  // below. Indexed vectors are accessed by indices in other places, changing the sorting order will
  // invalidate the existing indices and is not currently supported.
  CollectionVector<DebugInfoItem> debug_info_items_;
  CollectionVector<CodeItem> code_items_;
  CollectionVector<ClassData> class_datas_;

  uint32_t map_list_offset_ = 0;

  // Link data.
  std::vector<uint8_t> link_data_;

  DISALLOW_COPY_AND_ASSIGN(Header);
};

class StringData : public Item {
 public:
  explicit StringData(const char* data) : data_(strdup(data)) {
    size_ = UnsignedLeb128Size(CountModifiedUtf8Chars(data)) + strlen(data);
  }

  const char* Data() const { return data_.get(); }

  void Accept(AbstractDispatcher* dispatch) const { dispatch->Dispatch(this); }

 private:
  UniqueCPtr<const char> data_;

  DISALLOW_COPY_AND_ASSIGN(StringData);
};

class StringId : public IndexedItem {
 public:
  explicit StringId(StringData* string_data) : string_data_(string_data) {
    size_ = kStringIdItemSize;
  }
  ~StringId() OVERRIDE { }

  static size_t ItemSize() { return kStringIdItemSize; }

  const char* Data() const { return string_data_->Data(); }
  StringData* DataItem() const { return string_data_; }

  void Accept(AbstractDispatcher* dispatch) const { dispatch->Dispatch(this); }

 private:
  StringData* string_data_;

  DISALLOW_COPY_AND_ASSIGN(StringId);
};

class TypeId : public IndexedItem {
 public:
  explicit TypeId(StringId* string_id) : string_id_(string_id) { size_ = kTypeIdItemSize; }
  ~TypeId() OVERRIDE { }

  static size_t ItemSize() { return kTypeIdItemSize; }

  StringId* GetStringId() const { return string_id_; }

  void Accept(AbstractDispatcher* dispatch) const { dispatch->Dispatch(this); }

 private:
  StringId* string_id_;

  DISALLOW_COPY_AND_ASSIGN(TypeId);
};

using TypeIdVector = std::vector<const TypeId*>;

class TypeList : public Item {
 public:
  explicit TypeList(TypeIdVector* type_list) : type_list_(type_list) {
    size_ = sizeof(uint32_t) + (type_list->size() * sizeof(uint16_t));
  }
  ~TypeList() OVERRIDE { }

  const TypeIdVector* GetTypeList() const { return type_list_.get(); }

 private:
  std::unique_ptr<TypeIdVector> type_list_;

  DISALLOW_COPY_AND_ASSIGN(TypeList);
};

class ProtoId : public IndexedItem {
 public:
  ProtoId(const StringId* shorty, const TypeId* return_type, TypeList* parameters)
      : shorty_(shorty), return_type_(return_type), parameters_(parameters)
      { size_ = kProtoIdItemSize; }
  ~ProtoId() OVERRIDE { }

  static size_t ItemSize() { return kProtoIdItemSize; }

  const StringId* Shorty() const { return shorty_; }
  const TypeId* ReturnType() const { return return_type_; }
  const TypeList* Parameters() const { return parameters_; }

  void Accept(AbstractDispatcher* dispatch) const { dispatch->Dispatch(this); }

 private:
  const StringId* shorty_;
  const TypeId* return_type_;
  TypeList* parameters_;  // This can be nullptr.

  DISALLOW_COPY_AND_ASSIGN(ProtoId);
};

class FieldId : public IndexedItem {
 public:
  FieldId(const TypeId* klass, const TypeId* type, const StringId* name)
      : class_(klass), type_(type), name_(name) { size_ = kFieldIdItemSize; }
  ~FieldId() OVERRIDE { }

  static size_t ItemSize() { return kFieldIdItemSize; }

  const TypeId* Class() const { return class_; }
  const TypeId* Type() const { return type_; }
  const StringId* Name() const { return name_; }

  void Accept(AbstractDispatcher* dispatch) const { dispatch->Dispatch(this); }

 private:
  const TypeId* class_;
  const TypeId* type_;
  const StringId* name_;

  DISALLOW_COPY_AND_ASSIGN(FieldId);
};

class MethodId : public IndexedItem {
 public:
  MethodId(const TypeId* klass, const ProtoId* proto, const StringId* name)
      : class_(klass), proto_(proto), name_(name) { size_ = kMethodIdItemSize; }
  ~MethodId() OVERRIDE { }

  static size_t ItemSize() { return kMethodIdItemSize; }

  const TypeId* Class() const { return class_; }
  const ProtoId* Proto() const { return proto_; }
  const StringId* Name() const { return name_; }

  void Accept(AbstractDispatcher* dispatch) const { dispatch->Dispatch(this); }

 private:
  const TypeId* class_;
  const ProtoId* proto_;
  const StringId* name_;

  DISALLOW_COPY_AND_ASSIGN(MethodId);
};

class FieldItem : public Item {
 public:
  FieldItem(uint32_t access_flags, const FieldId* field_id)
      : access_flags_(access_flags), field_id_(field_id) { }
  ~FieldItem() OVERRIDE { }

  FieldItem(FieldItem&&) = default;

  uint32_t GetAccessFlags() const { return access_flags_; }
  const FieldId* GetFieldId() const { return field_id_; }

  void Accept(AbstractDispatcher* dispatch) { dispatch->Dispatch(this); }

 private:
  uint32_t access_flags_;
  const FieldId* field_id_;

  DISALLOW_COPY_AND_ASSIGN(FieldItem);
};

using FieldItemVector = std::vector<FieldItem>;

class MethodItem : public Item {
 public:
  MethodItem(uint32_t access_flags, const MethodId* method_id, CodeItem* code)
      : access_flags_(access_flags), method_id_(method_id), code_(code) { }
  ~MethodItem() OVERRIDE { }

  MethodItem(MethodItem&&) = default;

  uint32_t GetAccessFlags() const { return access_flags_; }
  const MethodId* GetMethodId() const { return method_id_; }
  CodeItem* GetCodeItem() { return code_; }

  void Accept(AbstractDispatcher* dispatch) { dispatch->Dispatch(this); }

 private:
  uint32_t access_flags_;
  const MethodId* method_id_;
  CodeItem* code_;  // This can be nullptr.

  DISALLOW_COPY_AND_ASSIGN(MethodItem);
};

using MethodItemVector = std::vector<MethodItem>;

class EncodedValue {
 public:
  explicit EncodedValue(uint8_t type) : type_(type) { }

  int8_t Type() const { return type_; }

  void SetBoolean(bool z) { u_.bool_val_ = z; }
  void SetByte(int8_t b) { u_.byte_val_ = b; }
  void SetShort(int16_t s) { u_.short_val_ = s; }
  void SetChar(uint16_t c) { u_.char_val_ = c; }
  void SetInt(int32_t i) { u_.int_val_ = i; }
  void SetLong(int64_t l) { u_.long_val_ = l; }
  void SetFloat(float f) { u_.float_val_ = f; }
  void SetDouble(double d) { u_.double_val_ = d; }
  void SetStringId(StringId* string_id) { u_.string_val_ = string_id; }
  void SetTypeId(TypeId* type_id) { u_.type_val_ = type_id; }
  void SetProtoId(ProtoId* proto_id) { u_.proto_val_ = proto_id; }
  void SetFieldId(FieldId* field_id) { u_.field_val_ = field_id; }
  void SetMethodId(MethodId* method_id) { u_.method_val_ = method_id; }
  void SetMethodHandle(MethodHandleItem* method_handle) { u_.method_handle_val_ = method_handle; }
  void SetEncodedArray(EncodedArrayItem* encoded_array) { encoded_array_.reset(encoded_array); }
  void SetEncodedAnnotation(EncodedAnnotation* encoded_annotation)
      { encoded_annotation_.reset(encoded_annotation); }

  bool GetBoolean() const { return u_.bool_val_; }
  int8_t GetByte() const { return u_.byte_val_; }
  int16_t GetShort() const { return u_.short_val_; }
  uint16_t GetChar() const { return u_.char_val_; }
  int32_t GetInt() const { return u_.int_val_; }
  int64_t GetLong() const { return u_.long_val_; }
  float GetFloat() const { return u_.float_val_; }
  double GetDouble() const { return u_.double_val_; }
  StringId* GetStringId() const { return u_.string_val_; }
  TypeId* GetTypeId() const { return u_.type_val_; }
  ProtoId* GetProtoId() const { return u_.proto_val_; }
  FieldId* GetFieldId() const { return u_.field_val_; }
  MethodId* GetMethodId() const { return u_.method_val_; }
  MethodHandleItem* GetMethodHandle() const { return u_.method_handle_val_; }
  EncodedArrayItem* GetEncodedArray() const { return encoded_array_.get(); }
  EncodedAnnotation* GetEncodedAnnotation() const { return encoded_annotation_.get(); }

  EncodedAnnotation* ReleaseEncodedAnnotation() { return encoded_annotation_.release(); }

 private:
  uint8_t type_;
  union {
    bool bool_val_;
    int8_t byte_val_;
    int16_t short_val_;
    uint16_t char_val_;
    int32_t int_val_;
    int64_t long_val_;
    float float_val_;
    double double_val_;
    StringId* string_val_;
    TypeId* type_val_;
    ProtoId* proto_val_;
    FieldId* field_val_;
    MethodId* method_val_;
    MethodHandleItem* method_handle_val_;
  } u_;
  std::unique_ptr<EncodedArrayItem> encoded_array_;
  std::unique_ptr<EncodedAnnotation> encoded_annotation_;

  DISALLOW_COPY_AND_ASSIGN(EncodedValue);
};

using EncodedValueVector = std::vector<std::unique_ptr<EncodedValue>>;

class AnnotationElement {
 public:
  AnnotationElement(StringId* name, EncodedValue* value) : name_(name), value_(value) { }

  StringId* GetName() const { return name_; }
  EncodedValue* GetValue() const { return value_.get(); }

 private:
  StringId* name_;
  std::unique_ptr<EncodedValue> value_;

  DISALLOW_COPY_AND_ASSIGN(AnnotationElement);
};

using AnnotationElementVector = std::vector<std::unique_ptr<AnnotationElement>>;

class EncodedAnnotation {
 public:
  EncodedAnnotation(TypeId* type, AnnotationElementVector* elements)
      : type_(type), elements_(elements) { }

  TypeId* GetType() const { return type_; }
  AnnotationElementVector* GetAnnotationElements() const { return elements_.get(); }

 private:
  TypeId* type_;
  std::unique_ptr<AnnotationElementVector> elements_;

  DISALLOW_COPY_AND_ASSIGN(EncodedAnnotation);
};

class EncodedArrayItem : public Item {
 public:
  explicit EncodedArrayItem(EncodedValueVector* encoded_values)
      : encoded_values_(encoded_values) { }

  EncodedValueVector* GetEncodedValues() const { return encoded_values_.get(); }

 private:
  std::unique_ptr<EncodedValueVector> encoded_values_;

  DISALLOW_COPY_AND_ASSIGN(EncodedArrayItem);
};

class ClassData : public Item {
 public:
  ClassData(FieldItemVector* static_fields,
            FieldItemVector* instance_fields,
            MethodItemVector* direct_methods,
            MethodItemVector* virtual_methods)
      : static_fields_(static_fields),
        instance_fields_(instance_fields),
        direct_methods_(direct_methods),
        virtual_methods_(virtual_methods) { }

  ~ClassData() OVERRIDE = default;
  FieldItemVector* StaticFields() { return static_fields_.get(); }
  FieldItemVector* InstanceFields() { return instance_fields_.get(); }
  MethodItemVector* DirectMethods() { return direct_methods_.get(); }
  MethodItemVector* VirtualMethods() { return virtual_methods_.get(); }

  void Accept(AbstractDispatcher* dispatch) { dispatch->Dispatch(this); }

 private:
  std::unique_ptr<FieldItemVector> static_fields_;
  std::unique_ptr<FieldItemVector> instance_fields_;
  std::unique_ptr<MethodItemVector> direct_methods_;
  std::unique_ptr<MethodItemVector> virtual_methods_;

  DISALLOW_COPY_AND_ASSIGN(ClassData);
};

class ClassDef : public IndexedItem {
 public:
  ClassDef(const TypeId* class_type,
           uint32_t access_flags,
           const TypeId* superclass,
           TypeList* interfaces,
           const StringId* source_file,
           AnnotationsDirectoryItem* annotations,
           EncodedArrayItem* static_values,
           ClassData* class_data)
      : class_type_(class_type),
        access_flags_(access_flags),
        superclass_(superclass),
        interfaces_(interfaces),
        source_file_(source_file),
        annotations_(annotations),
        class_data_(class_data),
        static_values_(static_values) { size_ = kClassDefItemSize; }

  ~ClassDef() OVERRIDE { }

  static size_t ItemSize() { return kClassDefItemSize; }

  const TypeId* ClassType() const { return class_type_; }
  uint32_t GetAccessFlags() const { return access_flags_; }
  const TypeId* Superclass() const { return superclass_; }
  const TypeList* Interfaces() { return interfaces_; }
  uint32_t InterfacesOffset() { return interfaces_ == nullptr ? 0 : interfaces_->GetOffset(); }
  const StringId* SourceFile() const { return source_file_; }
  AnnotationsDirectoryItem* Annotations() const { return annotations_; }
  ClassData* GetClassData() { return class_data_; }
  EncodedArrayItem* StaticValues() { return static_values_; }

  MethodItem* GenerateMethodItem(Header& header, ClassDataItemIterator& cdii);

  void Accept(AbstractDispatcher* dispatch) { dispatch->Dispatch(this); }

 private:
  const TypeId* class_type_;
  uint32_t access_flags_;
  const TypeId* superclass_;  // This can be nullptr.
  TypeList* interfaces_;  // This can be nullptr.
  const StringId* source_file_;  // This can be nullptr.
  AnnotationsDirectoryItem* annotations_;  // This can be nullptr.
  ClassData* class_data_;  // This can be nullptr.
  EncodedArrayItem* static_values_;  // This can be nullptr.

  DISALLOW_COPY_AND_ASSIGN(ClassDef);
};

class TypeAddrPair {
 public:
  TypeAddrPair(const TypeId* type_id, uint32_t address) : type_id_(type_id), address_(address) { }

  const TypeId* GetTypeId() const { return type_id_; }
  uint32_t GetAddress() const { return address_; }

 private:
  const TypeId* type_id_;  // This can be nullptr.
  uint32_t address_;

  DISALLOW_COPY_AND_ASSIGN(TypeAddrPair);
};

using TypeAddrPairVector = std::vector<std::unique_ptr<const TypeAddrPair>>;

class CatchHandler {
 public:
  explicit CatchHandler(bool catch_all, uint16_t list_offset, TypeAddrPairVector* handlers)
      : catch_all_(catch_all), list_offset_(list_offset), handlers_(handlers) { }

  bool HasCatchAll() const { return catch_all_; }
  uint16_t GetListOffset() const { return list_offset_; }
  TypeAddrPairVector* GetHandlers() const { return handlers_.get(); }

 private:
  bool catch_all_;
  uint16_t list_offset_;
  std::unique_ptr<TypeAddrPairVector> handlers_;

  DISALLOW_COPY_AND_ASSIGN(CatchHandler);
};

using CatchHandlerVector = std::vector<std::unique_ptr<const CatchHandler>>;

class TryItem : public Item {
 public:
  TryItem(uint32_t start_addr, uint16_t insn_count, const CatchHandler* handlers)
      : start_addr_(start_addr), insn_count_(insn_count), handlers_(handlers) { }
  ~TryItem() OVERRIDE { }

  uint32_t StartAddr() const { return start_addr_; }
  uint16_t InsnCount() const { return insn_count_; }
  const CatchHandler* GetHandlers() const { return handlers_; }

  void Accept(AbstractDispatcher* dispatch) { dispatch->Dispatch(this); }

 private:
  uint32_t start_addr_;
  uint16_t insn_count_;
  const CatchHandler* handlers_;

  DISALLOW_COPY_AND_ASSIGN(TryItem);
};

using TryItemVector = std::vector<std::unique_ptr<const TryItem>>;

class CodeFixups {
 public:
  CodeFixups(std::vector<TypeId*> type_ids,
             std::vector<StringId*> string_ids,
             std::vector<MethodId*> method_ids,
             std::vector<FieldId*> field_ids)
      : type_ids_(std::move(type_ids)),
        string_ids_(std::move(string_ids)),
        method_ids_(std::move(method_ids)),
        field_ids_(std::move(field_ids)) { }

  const std::vector<TypeId*>& TypeIds() const { return type_ids_; }
  const std::vector<StringId*>& StringIds() const { return string_ids_; }
  const std::vector<MethodId*>& MethodIds() const { return method_ids_; }
  const std::vector<FieldId*>& FieldIds() const { return field_ids_; }

 private:
  std::vector<TypeId*> type_ids_;
  std::vector<StringId*> string_ids_;
  std::vector<MethodId*> method_ids_;
  std::vector<FieldId*> field_ids_;

  DISALLOW_COPY_AND_ASSIGN(CodeFixups);
};

class CodeItem : public Item {
 public:
  CodeItem(uint16_t registers_size,
           uint16_t ins_size,
           uint16_t outs_size,
           DebugInfoItem* debug_info,
           uint32_t insns_size,
           uint16_t* insns,
           TryItemVector* tries,
           CatchHandlerVector* handlers)
      : registers_size_(registers_size),
        ins_size_(ins_size),
        outs_size_(outs_size),
        debug_info_(debug_info),
        insns_size_(insns_size),
        insns_(insns),
        tries_(tries),
        handlers_(handlers) { }

  ~CodeItem() OVERRIDE { }

  uint16_t RegistersSize() const { return registers_size_; }
  uint16_t InsSize() const { return ins_size_; }
  uint16_t OutsSize() const { return outs_size_; }
  uint16_t TriesSize() const { return tries_ == nullptr ? 0 : tries_->size(); }
  DebugInfoItem* DebugInfo() const { return debug_info_; }
  uint32_t InsnsSize() const { return insns_size_; }
  uint16_t* Insns() const { return insns_.get(); }
  TryItemVector* Tries() const { return tries_.get(); }
  CatchHandlerVector* Handlers() const { return handlers_.get(); }

  void SetCodeFixups(CodeFixups* fixups) { fixups_.reset(fixups); }
  CodeFixups* GetCodeFixups() const { return fixups_.get(); }

  void Accept(AbstractDispatcher* dispatch) { dispatch->Dispatch(this); }

  IterationRange<DexInstructionIterator> Instructions() const {
    return MakeIterationRange(DexInstructionIterator(Insns(), 0u),
                              DexInstructionIterator(Insns(), InsnsSize()));
  }

 private:
  uint16_t registers_size_;
  uint16_t ins_size_;
  uint16_t outs_size_;
  DebugInfoItem* debug_info_;  // This can be nullptr.
  uint32_t insns_size_;
  std::unique_ptr<uint16_t[]> insns_;
  std::unique_ptr<TryItemVector> tries_;  // This can be nullptr.
  std::unique_ptr<CatchHandlerVector> handlers_;  // This can be nullptr.
  std::unique_ptr<CodeFixups> fixups_;  // This can be nullptr.

  DISALLOW_COPY_AND_ASSIGN(CodeItem);
};

class DebugInfoItem : public Item {
 public:
  DebugInfoItem(uint32_t debug_info_size, uint8_t* debug_info)
     : debug_info_size_(debug_info_size), debug_info_(debug_info) { }

  uint32_t GetDebugInfoSize() const { return debug_info_size_; }
  uint8_t* GetDebugInfo() const { return debug_info_.get(); }

 private:
  uint32_t debug_info_size_;
  std::unique_ptr<uint8_t[]> debug_info_;

  DISALLOW_COPY_AND_ASSIGN(DebugInfoItem);
};

class AnnotationItem : public Item {
 public:
  AnnotationItem(uint8_t visibility, EncodedAnnotation* annotation)
      : visibility_(visibility), annotation_(annotation) { }

  uint8_t GetVisibility() const { return visibility_; }
  EncodedAnnotation* GetAnnotation() const { return annotation_.get(); }

  void Accept(AbstractDispatcher* dispatch) { dispatch->Dispatch(this); }

 private:
  uint8_t visibility_;
  std::unique_ptr<EncodedAnnotation> annotation_;

  DISALLOW_COPY_AND_ASSIGN(AnnotationItem);
};

class AnnotationSetItem : public Item {
 public:
  explicit AnnotationSetItem(std::vector<AnnotationItem*>* items) : items_(items) {
    size_ = sizeof(uint32_t) + items->size() * sizeof(uint32_t);
  }
  ~AnnotationSetItem() OVERRIDE { }

  std::vector<AnnotationItem*>* GetItems() { return items_.get(); }

  void Accept(AbstractDispatcher* dispatch) { dispatch->Dispatch(this); }

 private:
  std::unique_ptr<std::vector<AnnotationItem*>> items_;

  DISALLOW_COPY_AND_ASSIGN(AnnotationSetItem);
};

class AnnotationSetRefList : public Item {
 public:
  explicit AnnotationSetRefList(std::vector<AnnotationSetItem*>* items) : items_(items) {
    size_ = sizeof(uint32_t) + items->size() * sizeof(uint32_t);
  }
  ~AnnotationSetRefList() OVERRIDE { }

  std::vector<AnnotationSetItem*>* GetItems() { return items_.get(); }

  void Accept(AbstractDispatcher* dispatch) { dispatch->Dispatch(this); }

 private:
  std::unique_ptr<std::vector<AnnotationSetItem*>> items_;  // Elements of vector can be nullptr.

  DISALLOW_COPY_AND_ASSIGN(AnnotationSetRefList);
};

class FieldAnnotation {
 public:
  FieldAnnotation(FieldId* field_id, AnnotationSetItem* annotation_set_item)
      : field_id_(field_id), annotation_set_item_(annotation_set_item) { }

  FieldId* GetFieldId() const { return field_id_; }
  AnnotationSetItem* GetAnnotationSetItem() const { return annotation_set_item_; }

 private:
  FieldId* field_id_;
  AnnotationSetItem* annotation_set_item_;

  DISALLOW_COPY_AND_ASSIGN(FieldAnnotation);
};

using FieldAnnotationVector = std::vector<std::unique_ptr<FieldAnnotation>>;

class MethodAnnotation {
 public:
  MethodAnnotation(MethodId* method_id, AnnotationSetItem* annotation_set_item)
      : method_id_(method_id), annotation_set_item_(annotation_set_item) { }

  MethodId* GetMethodId() const { return method_id_; }
  AnnotationSetItem* GetAnnotationSetItem() const { return annotation_set_item_; }

 private:
  MethodId* method_id_;
  AnnotationSetItem* annotation_set_item_;

  DISALLOW_COPY_AND_ASSIGN(MethodAnnotation);
};

using MethodAnnotationVector = std::vector<std::unique_ptr<MethodAnnotation>>;

class ParameterAnnotation {
 public:
  ParameterAnnotation(MethodId* method_id, AnnotationSetRefList* annotations)
      : method_id_(method_id), annotations_(annotations) { }

  MethodId* GetMethodId() const { return method_id_; }
  AnnotationSetRefList* GetAnnotations() { return annotations_; }

 private:
  MethodId* method_id_;
  AnnotationSetRefList* annotations_;

  DISALLOW_COPY_AND_ASSIGN(ParameterAnnotation);
};

using ParameterAnnotationVector = std::vector<std::unique_ptr<ParameterAnnotation>>;

class AnnotationsDirectoryItem : public Item {
 public:
  AnnotationsDirectoryItem(AnnotationSetItem* class_annotation,
                           FieldAnnotationVector* field_annotations,
                           MethodAnnotationVector* method_annotations,
                           ParameterAnnotationVector* parameter_annotations)
      : class_annotation_(class_annotation),
        field_annotations_(field_annotations),
        method_annotations_(method_annotations),
        parameter_annotations_(parameter_annotations) { }

  AnnotationSetItem* GetClassAnnotation() const { return class_annotation_; }
  FieldAnnotationVector* GetFieldAnnotations() { return field_annotations_.get(); }
  MethodAnnotationVector* GetMethodAnnotations() { return method_annotations_.get(); }
  ParameterAnnotationVector* GetParameterAnnotations() { return parameter_annotations_.get(); }

  void Accept(AbstractDispatcher* dispatch) { dispatch->Dispatch(this); }

 private:
  AnnotationSetItem* class_annotation_;  // This can be nullptr.
  std::unique_ptr<FieldAnnotationVector> field_annotations_;  // This can be nullptr.
  std::unique_ptr<MethodAnnotationVector> method_annotations_;  // This can be nullptr.
  std::unique_ptr<ParameterAnnotationVector> parameter_annotations_;  // This can be nullptr.

  DISALLOW_COPY_AND_ASSIGN(AnnotationsDirectoryItem);
};

class CallSiteId : public IndexedItem {
 public:
  explicit CallSiteId(EncodedArrayItem* call_site_item) : call_site_item_(call_site_item) {
    size_ = kCallSiteIdItemSize;
  }
  ~CallSiteId() OVERRIDE { }

  static size_t ItemSize() { return kCallSiteIdItemSize; }

  EncodedArrayItem* CallSiteItem() const { return call_site_item_; }

  void Accept(AbstractDispatcher* dispatch) const { dispatch->Dispatch(this); }

 private:
  EncodedArrayItem* call_site_item_;

  DISALLOW_COPY_AND_ASSIGN(CallSiteId);
};

class MethodHandleItem : public IndexedItem {
 public:
  MethodHandleItem(DexFile::MethodHandleType method_handle_type, IndexedItem* field_or_method_id)
      : method_handle_type_(method_handle_type),
        field_or_method_id_(field_or_method_id) {
    size_ = kMethodHandleItemSize;
  }
  ~MethodHandleItem() OVERRIDE { }

  static size_t ItemSize() { return kMethodHandleItemSize; }

  DexFile::MethodHandleType GetMethodHandleType() const { return method_handle_type_; }
  IndexedItem* GetFieldOrMethodId() const { return field_or_method_id_; }

  void Accept(AbstractDispatcher* dispatch) const { dispatch->Dispatch(this); }

 private:
  DexFile::MethodHandleType method_handle_type_;
  IndexedItem* field_or_method_id_;

  DISALLOW_COPY_AND_ASSIGN(MethodHandleItem);
};

// TODO(sehr): implement MapList.
class MapList : public Item {
 public:
  void Accept(AbstractDispatcher* dispatch) { dispatch->Dispatch(this); }

 private:
  DISALLOW_COPY_AND_ASSIGN(MapList);
};

class MapItem : public Item {
 public:
  void Accept(AbstractDispatcher* dispatch) { dispatch->Dispatch(this); }

 private:
  DISALLOW_COPY_AND_ASSIGN(MapItem);
};

// Interface for building a vector of file sections for use by other clients.
struct DexFileSection {
 public:
  DexFileSection(const std::string& name, uint16_t type, uint32_t size, uint32_t offset)
      : name(name), type(type), size(size), offset(offset) { }
  std::string name;
  // The type (DexFile::MapItemType).
  uint16_t type;
  // The size (in elements, not bytes).
  uint32_t size;
  // The byte offset from the start of the file.
  uint32_t offset;
};

enum class SortDirection {
  kSortAscending,
  kSortDescending
};

std::vector<DexFileSection> GetSortedDexFileSections(dex_ir::Header* header,
                                                     SortDirection direction);

}  // namespace dex_ir
}  // namespace art

#endif  // ART_DEXLAYOUT_DEX_IR_H_
