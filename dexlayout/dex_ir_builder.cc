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

#include <stdint.h>
#include <vector>

#include "dex_ir_builder.h"

#include "dex/code_item_accessors-inl.h"
#include "dex/dex_file_exception_helpers.h"
#include "dexlayout.h"

namespace art {
namespace dex_ir {

static uint64_t ReadVarWidth(const uint8_t** data, uint8_t length, bool sign_extend) {
  uint64_t value = 0;
  for (uint32_t i = 0; i <= length; i++) {
    value |= static_cast<uint64_t>(*(*data)++) << (i * 8);
  }
  if (sign_extend) {
    int shift = (7 - length) * 8;
    return (static_cast<int64_t>(value) << shift) >> shift;
  }
  return value;
}

static uint32_t GetDebugInfoStreamSize(const uint8_t* debug_info_stream) {
  const uint8_t* stream = debug_info_stream;
  DecodeUnsignedLeb128(&stream);  // line_start
  uint32_t parameters_size = DecodeUnsignedLeb128(&stream);
  for (uint32_t i = 0; i < parameters_size; ++i) {
    DecodeUnsignedLeb128P1(&stream);  // Parameter name.
  }

  for (;;)  {
    uint8_t opcode = *stream++;
    switch (opcode) {
      case DexFile::DBG_END_SEQUENCE:
        return stream - debug_info_stream;  // end of stream.
      case DexFile::DBG_ADVANCE_PC:
        DecodeUnsignedLeb128(&stream);  // addr_diff
        break;
      case DexFile::DBG_ADVANCE_LINE:
        DecodeSignedLeb128(&stream);  // line_diff
        break;
      case DexFile::DBG_START_LOCAL:
        DecodeUnsignedLeb128(&stream);  // register_num
        DecodeUnsignedLeb128P1(&stream);  // name_idx
        DecodeUnsignedLeb128P1(&stream);  // type_idx
        break;
      case DexFile::DBG_START_LOCAL_EXTENDED:
        DecodeUnsignedLeb128(&stream);  // register_num
        DecodeUnsignedLeb128P1(&stream);  // name_idx
        DecodeUnsignedLeb128P1(&stream);  // type_idx
        DecodeUnsignedLeb128P1(&stream);  // sig_idx
        break;
      case DexFile::DBG_END_LOCAL:
      case DexFile::DBG_RESTART_LOCAL:
        DecodeUnsignedLeb128(&stream);  // register_num
        break;
      case DexFile::DBG_SET_PROLOGUE_END:
      case DexFile::DBG_SET_EPILOGUE_BEGIN:
        break;
      case DexFile::DBG_SET_FILE: {
        DecodeUnsignedLeb128P1(&stream);  // name_idx
        break;
      }
      default: {
        break;
      }
    }
  }
}

template<class T> class CollectionMap : public CollectionBase {
 public:
  CollectionMap() = default;
  virtual ~CollectionMap() OVERRIDE { }

  template <class... Args>
  T* CreateAndAddItem(CollectionVector<T>& vector,
                      bool eagerly_assign_offsets,
                      uint32_t offset,
                      Args&&... args) {
    T* item = vector.CreateAndAddItem(std::forward<Args>(args)...);
    DCHECK(!GetExistingObject(offset));
    DCHECK(!item->OffsetAssigned());
    if (eagerly_assign_offsets) {
      item->SetOffset(offset);
    }
    AddItem(item, offset);
    return item;
  }

  // Returns the existing item if it is already inserted, null otherwise.
  T* GetExistingObject(uint32_t offset) {
    auto it = collection_.find(offset);
    return it != collection_.end() ? it->second : nullptr;
  }

  // Lower case for template interop with std::map.
  uint32_t size() const { return collection_.size(); }
  std::map<uint32_t, T*>& Collection() { return collection_; }

 private:
  std::map<uint32_t, T*> collection_;

  // CollectionMaps do not own the objects they contain, therefore AddItem is supported
  // rather than CreateAndAddItem.
  void AddItem(T* object, uint32_t offset) {
    auto it = collection_.emplace(offset, object);
    CHECK(it.second) << "CollectionMap already has an object with offset " << offset << " "
                     << " and address " << it.first->second;
  }

  DISALLOW_COPY_AND_ASSIGN(CollectionMap);
};

class BuilderMaps {
 public:
  BuilderMaps(Header* header, bool eagerly_assign_offsets)
      : header_(header), eagerly_assign_offsets_(eagerly_assign_offsets) { }

  void CreateStringId(const DexFile& dex_file, uint32_t i);
  void CreateTypeId(const DexFile& dex_file, uint32_t i);
  void CreateProtoId(const DexFile& dex_file, uint32_t i);
  void CreateFieldId(const DexFile& dex_file, uint32_t i);
  void CreateMethodId(const DexFile& dex_file, uint32_t i);
  void CreateClassDef(const DexFile& dex_file, uint32_t i);
  void CreateCallSiteId(const DexFile& dex_file, uint32_t i);
  void CreateMethodHandleItem(const DexFile& dex_file, uint32_t i);

  void CreateCallSitesAndMethodHandles(const DexFile& dex_file);

  TypeList* CreateTypeList(const DexFile::TypeList* type_list, uint32_t offset);
  EncodedArrayItem* CreateEncodedArrayItem(const DexFile& dex_file,
                                           const uint8_t* static_data,
                                           uint32_t offset);
  AnnotationItem* CreateAnnotationItem(const DexFile& dex_file,
                                       const DexFile::AnnotationItem* annotation);
  AnnotationSetItem* CreateAnnotationSetItem(const DexFile& dex_file,
      const DexFile::AnnotationSetItem* disk_annotations_item, uint32_t offset);
  AnnotationsDirectoryItem* CreateAnnotationsDirectoryItem(const DexFile& dex_file,
      const DexFile::AnnotationsDirectoryItem* disk_annotations_item, uint32_t offset);
  CodeItem* DedupeOrCreateCodeItem(const DexFile& dex_file,
                                   const DexFile::CodeItem* disk_code_item,
                                   uint32_t offset,
                                   uint32_t dex_method_index);
  ClassData* CreateClassData(const DexFile& dex_file, const uint8_t* encoded_data, uint32_t offset);

  void AddAnnotationsFromMapListSection(const DexFile& dex_file,
                                        uint32_t start_offset,
                                        uint32_t count);

  void CheckAndSetRemainingOffsets(const DexFile& dex_file, const Options& options);

  // Sort the vectors buy map order (same order that was used in the input file).
  void SortVectorsByMapOrder();

 private:
  bool GetIdsFromByteCode(const CodeItem* code,
                          std::vector<TypeId*>* type_ids,
                          std::vector<StringId*>* string_ids,
                          std::vector<MethodId*>* method_ids,
                          std::vector<FieldId*>* field_ids);

  bool GetIdFromInstruction(const Instruction* dec_insn,
                            std::vector<TypeId*>* type_ids,
                            std::vector<StringId*>* string_ids,
                            std::vector<MethodId*>* method_ids,
                            std::vector<FieldId*>* field_ids);

  EncodedValue* ReadEncodedValue(const DexFile& dex_file, const uint8_t** data);
  EncodedValue* ReadEncodedValue(const DexFile& dex_file,
                                 const uint8_t** data,
                                 uint8_t type,
                                 uint8_t length);
  void ReadEncodedValue(const DexFile& dex_file,
                        const uint8_t** data,
                        uint8_t type,
                        uint8_t length,
                        EncodedValue* item);

  MethodItem GenerateMethodItem(const DexFile& dex_file, ClassDataItemIterator& cdii);

  ParameterAnnotation* GenerateParameterAnnotation(
      const DexFile& dex_file,
      MethodId* method_id,
      const DexFile::AnnotationSetRefList* annotation_set_ref_list,
      uint32_t offset);

  template <typename Type, class... Args>
  Type* CreateAndAddIndexedItem(IndexedCollectionVector<Type>& vector,
                                uint32_t offset,
                                uint32_t index,
                                Args&&... args) {
    Type* item = vector.CreateAndAddIndexedItem(index, std::forward<Args>(args)...);
    DCHECK(!item->OffsetAssigned());
    if (eagerly_assign_offsets_) {
      item->SetOffset(offset);
    }
    return item;
  }

  Header* header_;
  // If we eagerly assign offsets during IR building or later after layout. Must be false if
  // changing the layout is enabled.
  bool eagerly_assign_offsets_;

  // Note: maps do not have ownership.
  CollectionMap<StringData> string_datas_map_;
  CollectionMap<TypeList> type_lists_map_;
  CollectionMap<EncodedArrayItem> encoded_array_items_map_;
  CollectionMap<AnnotationItem> annotation_items_map_;
  CollectionMap<AnnotationSetItem> annotation_set_items_map_;
  CollectionMap<AnnotationSetRefList> annotation_set_ref_lists_map_;
  CollectionMap<AnnotationsDirectoryItem> annotations_directory_items_map_;
  CollectionMap<DebugInfoItem> debug_info_items_map_;
  // Code item maps need to check both the debug info offset and debug info offset, do not use
  // CollectionMap.
  // First offset is the code item offset, second is the debug info offset.
  std::map<std::pair<uint32_t, uint32_t>, CodeItem*> code_items_map_;
  CollectionMap<ClassData> class_datas_map_;

  DISALLOW_COPY_AND_ASSIGN(BuilderMaps);
};

Header* DexIrBuilder(const DexFile& dex_file,
                     bool eagerly_assign_offsets,
                     const Options& options) {
  const DexFile::Header& disk_header = dex_file.GetHeader();
  Header* header = new Header(disk_header.magic_,
                              disk_header.checksum_,
                              disk_header.signature_,
                              disk_header.endian_tag_,
                              disk_header.file_size_,
                              disk_header.header_size_,
                              disk_header.link_size_,
                              disk_header.link_off_,
                              disk_header.data_size_,
                              disk_header.data_off_,
                              dex_file.SupportsDefaultMethods(),
                              dex_file.NumStringIds(),
                              dex_file.NumTypeIds(),
                              dex_file.NumProtoIds(),
                              dex_file.NumFieldIds(),
                              dex_file.NumMethodIds(),
                              dex_file.NumClassDefs());
  BuilderMaps builder_maps(header, eagerly_assign_offsets);
  // Walk the rest of the header fields.
  // StringId table.
  header->StringIds().SetOffset(disk_header.string_ids_off_);
  for (uint32_t i = 0; i < dex_file.NumStringIds(); ++i) {
    builder_maps.CreateStringId(dex_file, i);
  }
  // TypeId table.
  header->TypeIds().SetOffset(disk_header.type_ids_off_);
  for (uint32_t i = 0; i < dex_file.NumTypeIds(); ++i) {
    builder_maps.CreateTypeId(dex_file, i);
  }
  // ProtoId table.
  header->ProtoIds().SetOffset(disk_header.proto_ids_off_);
  for (uint32_t i = 0; i < dex_file.NumProtoIds(); ++i) {
    builder_maps.CreateProtoId(dex_file, i);
  }
  // FieldId table.
  header->FieldIds().SetOffset(disk_header.field_ids_off_);
  for (uint32_t i = 0; i < dex_file.NumFieldIds(); ++i) {
    builder_maps.CreateFieldId(dex_file, i);
  }
  // MethodId table.
  header->MethodIds().SetOffset(disk_header.method_ids_off_);
  for (uint32_t i = 0; i < dex_file.NumMethodIds(); ++i) {
    builder_maps.CreateMethodId(dex_file, i);
  }
  // ClassDef table.
  header->ClassDefs().SetOffset(disk_header.class_defs_off_);
  for (uint32_t i = 0; i < dex_file.NumClassDefs(); ++i) {
    if (!options.class_filter_.empty()) {
      // If the filter is enabled (not empty), filter out classes that don't have a matching
      // descriptor.
      const DexFile::ClassDef& class_def = dex_file.GetClassDef(i);
      const char* descriptor = dex_file.GetClassDescriptor(class_def);
      if (options.class_filter_.find(descriptor) == options.class_filter_.end()) {
        continue;
      }
    }
    builder_maps.CreateClassDef(dex_file, i);
  }
  // MapItem.
  header->SetMapListOffset(disk_header.map_off_);
  // CallSiteIds and MethodHandleItems.
  builder_maps.CreateCallSitesAndMethodHandles(dex_file);
  builder_maps.CheckAndSetRemainingOffsets(dex_file, options);

  // Sort the vectors by the map order (same order as the file).
  builder_maps.SortVectorsByMapOrder();

  // Load the link data if it exists.
  header->SetLinkData(std::vector<uint8_t>(
      dex_file.DataBegin() + dex_file.GetHeader().link_off_,
      dex_file.DataBegin() + dex_file.GetHeader().link_off_ + dex_file.GetHeader().link_size_));

  return header;
}

/*
 * Get all the types, strings, methods, and fields referred to from bytecode.
 */
void BuilderMaps::CheckAndSetRemainingOffsets(const DexFile& dex_file, const Options& options) {
  const DexFile::Header& disk_header = dex_file.GetHeader();
  // Read MapItems and validate/set remaining offsets.
  const DexFile::MapList* map = dex_file.GetMapList();
  const uint32_t count = map->size_;
  for (uint32_t i = 0; i < count; ++i) {
    const DexFile::MapItem* item = map->list_ + i;
    switch (item->type_) {
      case DexFile::kDexTypeHeaderItem:
        CHECK_EQ(item->size_, 1u);
        CHECK_EQ(item->offset_, 0u);
        break;
      case DexFile::kDexTypeStringIdItem:
        CHECK_EQ(item->size_, header_->StringIds().Size());
        CHECK_EQ(item->offset_, header_->StringIds().GetOffset());
        break;
      case DexFile::kDexTypeTypeIdItem:
        CHECK_EQ(item->size_, header_->TypeIds().Size());
        CHECK_EQ(item->offset_, header_->TypeIds().GetOffset());
        break;
      case DexFile::kDexTypeProtoIdItem:
        CHECK_EQ(item->size_, header_->ProtoIds().Size());
        CHECK_EQ(item->offset_, header_->ProtoIds().GetOffset());
        break;
      case DexFile::kDexTypeFieldIdItem:
        CHECK_EQ(item->size_, header_->FieldIds().Size());
        CHECK_EQ(item->offset_, header_->FieldIds().GetOffset());
        break;
      case DexFile::kDexTypeMethodIdItem:
        CHECK_EQ(item->size_, header_->MethodIds().Size());
        CHECK_EQ(item->offset_, header_->MethodIds().GetOffset());
        break;
      case DexFile::kDexTypeClassDefItem:
        if (options.class_filter_.empty()) {
          // The filter may have removed some classes, this will get fixed up during writing.
          CHECK_EQ(item->size_, header_->ClassDefs().Size());
        }
        CHECK_EQ(item->offset_, header_->ClassDefs().GetOffset());
        break;
      case DexFile::kDexTypeCallSiteIdItem:
        CHECK_EQ(item->size_, header_->CallSiteIds().Size());
        CHECK_EQ(item->offset_, header_->CallSiteIds().GetOffset());
        break;
      case DexFile::kDexTypeMethodHandleItem:
        CHECK_EQ(item->size_, header_->MethodHandleItems().Size());
        CHECK_EQ(item->offset_, header_->MethodHandleItems().GetOffset());
        break;
      case DexFile::kDexTypeMapList:
        CHECK_EQ(item->size_, 1u);
        CHECK_EQ(item->offset_, disk_header.map_off_);
        break;
      case DexFile::kDexTypeTypeList:
        header_->TypeLists().SetOffset(item->offset_);
        break;
      case DexFile::kDexTypeAnnotationSetRefList:
        header_->AnnotationSetRefLists().SetOffset(item->offset_);
        break;
      case DexFile::kDexTypeAnnotationSetItem:
        header_->AnnotationSetItems().SetOffset(item->offset_);
        break;
      case DexFile::kDexTypeClassDataItem:
        header_->ClassDatas().SetOffset(item->offset_);
        break;
      case DexFile::kDexTypeCodeItem:
        header_->CodeItems().SetOffset(item->offset_);
        break;
      case DexFile::kDexTypeStringDataItem:
        header_->StringDatas().SetOffset(item->offset_);
        break;
      case DexFile::kDexTypeDebugInfoItem:
        header_->DebugInfoItems().SetOffset(item->offset_);
        break;
      case DexFile::kDexTypeAnnotationItem:
        header_->AnnotationItems().SetOffset(item->offset_);
        AddAnnotationsFromMapListSection(dex_file, item->offset_, item->size_);
        break;
      case DexFile::kDexTypeEncodedArrayItem:
        header_->EncodedArrayItems().SetOffset(item->offset_);
        break;
      case DexFile::kDexTypeAnnotationsDirectoryItem:
        header_->AnnotationsDirectoryItems().SetOffset(item->offset_);
        break;
      default:
        LOG(ERROR) << "Unknown map list item type.";
    }
  }
}

void BuilderMaps::CreateStringId(const DexFile& dex_file, uint32_t i) {
  const DexFile::StringId& disk_string_id = dex_file.GetStringId(dex::StringIndex(i));
  StringData* string_data =
      string_datas_map_.CreateAndAddItem(header_->StringDatas(),
                                         eagerly_assign_offsets_,
                                         disk_string_id.string_data_off_,
                                         dex_file.GetStringData(disk_string_id));
  CreateAndAddIndexedItem(header_->StringIds(),
                          header_->StringIds().GetOffset() + i * StringId::ItemSize(),
                          i,
                          string_data);
}

void BuilderMaps::CreateTypeId(const DexFile& dex_file, uint32_t i) {
  const DexFile::TypeId& disk_type_id = dex_file.GetTypeId(dex::TypeIndex(i));
  CreateAndAddIndexedItem(header_->TypeIds(),
                          header_->TypeIds().GetOffset() + i * TypeId::ItemSize(),
                          i,
                          header_->StringIds()[disk_type_id.descriptor_idx_.index_]);
}

void BuilderMaps::CreateProtoId(const DexFile& dex_file, uint32_t i) {
  const DexFile::ProtoId& disk_proto_id = dex_file.GetProtoId(dex::ProtoIndex(i));
  const DexFile::TypeList* type_list = dex_file.GetProtoParameters(disk_proto_id);
  TypeList* parameter_type_list = CreateTypeList(type_list, disk_proto_id.parameters_off_);

  CreateAndAddIndexedItem(header_->ProtoIds(),
                          header_->ProtoIds().GetOffset() + i * ProtoId::ItemSize(),
                          i,
                          header_->StringIds()[disk_proto_id.shorty_idx_.index_],
                          header_->TypeIds()[disk_proto_id.return_type_idx_.index_],
                          parameter_type_list);
}

void BuilderMaps::CreateFieldId(const DexFile& dex_file, uint32_t i) {
  const DexFile::FieldId& disk_field_id = dex_file.GetFieldId(i);
  CreateAndAddIndexedItem(header_->FieldIds(),
                          header_->FieldIds().GetOffset() + i * FieldId::ItemSize(),
                          i,
                          header_->TypeIds()[disk_field_id.class_idx_.index_],
                          header_->TypeIds()[disk_field_id.type_idx_.index_],
                          header_->StringIds()[disk_field_id.name_idx_.index_]);
}

void BuilderMaps::CreateMethodId(const DexFile& dex_file, uint32_t i) {
  const DexFile::MethodId& disk_method_id = dex_file.GetMethodId(i);
  CreateAndAddIndexedItem(header_->MethodIds(),
                          header_->MethodIds().GetOffset() + i * MethodId::ItemSize(),
                          i,
                          header_->TypeIds()[disk_method_id.class_idx_.index_],
                          header_->ProtoIds()[disk_method_id.proto_idx_.index_],
                          header_->StringIds()[disk_method_id.name_idx_.index_]);
}

void BuilderMaps::CreateClassDef(const DexFile& dex_file, uint32_t i) {
  const DexFile::ClassDef& disk_class_def = dex_file.GetClassDef(i);
  const TypeId* class_type = header_->TypeIds()[disk_class_def.class_idx_.index_];
  uint32_t access_flags = disk_class_def.access_flags_;
  const TypeId* superclass = header_->GetTypeIdOrNullPtr(disk_class_def.superclass_idx_.index_);

  const DexFile::TypeList* type_list = dex_file.GetInterfacesList(disk_class_def);
  TypeList* interfaces_type_list = CreateTypeList(type_list, disk_class_def.interfaces_off_);

  const StringId* source_file =
      header_->GetStringIdOrNullPtr(disk_class_def.source_file_idx_.index_);
  // Annotations.
  AnnotationsDirectoryItem* annotations = nullptr;
  const DexFile::AnnotationsDirectoryItem* disk_annotations_directory_item =
      dex_file.GetAnnotationsDirectory(disk_class_def);
  if (disk_annotations_directory_item != nullptr) {
    annotations = CreateAnnotationsDirectoryItem(
        dex_file, disk_annotations_directory_item, disk_class_def.annotations_off_);
  }
  // Static field initializers.
  const uint8_t* static_data = dex_file.GetEncodedStaticFieldValuesArray(disk_class_def);
  EncodedArrayItem* static_values =
      CreateEncodedArrayItem(dex_file, static_data, disk_class_def.static_values_off_);
  ClassData* class_data = CreateClassData(
      dex_file, dex_file.GetClassData(disk_class_def), disk_class_def.class_data_off_);
  CreateAndAddIndexedItem(header_->ClassDefs(),
                          header_->ClassDefs().GetOffset() + i * ClassDef::ItemSize(),
                          i,
                          class_type,
                          access_flags,
                          superclass,
                          interfaces_type_list,
                          source_file,
                          annotations,
                          static_values,
                          class_data);
}

void BuilderMaps::CreateCallSiteId(const DexFile& dex_file, uint32_t i) {
  const DexFile::CallSiteIdItem& disk_call_site_id = dex_file.GetCallSiteId(i);
  const uint8_t* disk_call_item_ptr = dex_file.DataBegin() + disk_call_site_id.data_off_;
  EncodedArrayItem* call_site_item =
      CreateEncodedArrayItem(dex_file, disk_call_item_ptr, disk_call_site_id.data_off_);

  CreateAndAddIndexedItem(header_->CallSiteIds(),
                          header_->CallSiteIds().GetOffset() + i * CallSiteId::ItemSize(),
                          i,
                          call_site_item);
}

void BuilderMaps::CreateMethodHandleItem(const DexFile& dex_file, uint32_t i) {
  const DexFile::MethodHandleItem& disk_method_handle = dex_file.GetMethodHandle(i);
  uint16_t index = disk_method_handle.field_or_method_idx_;
  DexFile::MethodHandleType type =
      static_cast<DexFile::MethodHandleType>(disk_method_handle.method_handle_type_);
  bool is_invoke = type == DexFile::MethodHandleType::kInvokeStatic ||
                   type == DexFile::MethodHandleType::kInvokeInstance ||
                   type == DexFile::MethodHandleType::kInvokeConstructor ||
                   type == DexFile::MethodHandleType::kInvokeDirect ||
                   type == DexFile::MethodHandleType::kInvokeInterface;
  static_assert(DexFile::MethodHandleType::kLast == DexFile::MethodHandleType::kInvokeInterface,
                "Unexpected method handle types.");
  IndexedItem* field_or_method_id;
  if (is_invoke) {
    field_or_method_id = header_->MethodIds()[index];
  } else {
    field_or_method_id = header_->FieldIds()[index];
  }
  CreateAndAddIndexedItem(header_->MethodHandleItems(),
                          header_->MethodHandleItems().GetOffset() +
                              i * MethodHandleItem::ItemSize(),
                          i,
                          type,
                          field_or_method_id);
}

void BuilderMaps::CreateCallSitesAndMethodHandles(const DexFile& dex_file) {
  // Iterate through the map list and set the offset of the CallSiteIds and MethodHandleItems.
  const DexFile::MapList* map = dex_file.GetMapList();
  for (uint32_t i = 0; i < map->size_; ++i) {
    const DexFile::MapItem* item = map->list_ + i;
    switch (item->type_) {
      case DexFile::kDexTypeCallSiteIdItem:
        header_->CallSiteIds().SetOffset(item->offset_);
        break;
      case DexFile::kDexTypeMethodHandleItem:
        header_->MethodHandleItems().SetOffset(item->offset_);
        break;
      default:
        break;
    }
  }
  // Populate MethodHandleItems first (CallSiteIds may depend on them).
  for (uint32_t i = 0; i < dex_file.NumMethodHandles(); i++) {
    CreateMethodHandleItem(dex_file, i);
  }
  // Populate CallSiteIds.
  for (uint32_t i = 0; i < dex_file.NumCallSiteIds(); i++) {
    CreateCallSiteId(dex_file, i);
  }
}

TypeList* BuilderMaps::CreateTypeList(const DexFile::TypeList* dex_type_list, uint32_t offset) {
  if (dex_type_list == nullptr) {
    return nullptr;
  }
  TypeList* type_list = type_lists_map_.GetExistingObject(offset);
  if (type_list == nullptr) {
    TypeIdVector* type_vector = new TypeIdVector();
    uint32_t size = dex_type_list->Size();
    for (uint32_t index = 0; index < size; ++index) {
      type_vector->push_back(header_->TypeIds()[
                             dex_type_list->GetTypeItem(index).type_idx_.index_]);
    }
    type_list = type_lists_map_.CreateAndAddItem(header_->TypeLists(),
                                                 eagerly_assign_offsets_,
                                                 offset,
                                                 type_vector);
  }
  return type_list;
}

EncodedArrayItem* BuilderMaps::CreateEncodedArrayItem(const DexFile& dex_file,
                                                      const uint8_t* static_data,
                                                      uint32_t offset) {
  if (static_data == nullptr) {
    return nullptr;
  }
  EncodedArrayItem* encoded_array_item = encoded_array_items_map_.GetExistingObject(offset);
  if (encoded_array_item == nullptr) {
    uint32_t size = DecodeUnsignedLeb128(&static_data);
    EncodedValueVector* values = new EncodedValueVector();
    for (uint32_t i = 0; i < size; ++i) {
      values->push_back(std::unique_ptr<EncodedValue>(ReadEncodedValue(dex_file, &static_data)));
    }
    // TODO: Calculate the size of the encoded array.
    encoded_array_item = encoded_array_items_map_.CreateAndAddItem(header_->EncodedArrayItems(),
                                                                   eagerly_assign_offsets_,
                                                                   offset,
                                                                   values);
  }
  return encoded_array_item;
}

void BuilderMaps::AddAnnotationsFromMapListSection(const DexFile& dex_file,
                                                   uint32_t start_offset,
                                                   uint32_t count) {
  uint32_t current_offset = start_offset;
  for (size_t i = 0; i < count; ++i) {
    // Annotation that we didn't process already, add it to the set.
    const DexFile::AnnotationItem* annotation = dex_file.GetAnnotationItemAtOffset(current_offset);
    AnnotationItem* annotation_item = CreateAnnotationItem(dex_file, annotation);
    DCHECK(annotation_item != nullptr);
    current_offset += annotation_item->GetSize();
  }
}

AnnotationItem* BuilderMaps::CreateAnnotationItem(const DexFile& dex_file,
                                                  const DexFile::AnnotationItem* annotation) {
  const uint8_t* const start_data = reinterpret_cast<const uint8_t*>(annotation);
  const uint32_t offset = start_data - dex_file.DataBegin();
  AnnotationItem* annotation_item = annotation_items_map_.GetExistingObject(offset);
  if (annotation_item == nullptr) {
    uint8_t visibility = annotation->visibility_;
    const uint8_t* annotation_data = annotation->annotation_;
    std::unique_ptr<EncodedValue> encoded_value(
        ReadEncodedValue(dex_file, &annotation_data, DexFile::kDexAnnotationAnnotation, 0));
    annotation_item =
        annotation_items_map_.CreateAndAddItem(header_->AnnotationItems(),
                                               eagerly_assign_offsets_,
                                               offset,
                                               visibility,
                                               encoded_value->ReleaseEncodedAnnotation());
    annotation_item->SetSize(annotation_data - start_data);
  }
  return annotation_item;
}


AnnotationSetItem* BuilderMaps::CreateAnnotationSetItem(const DexFile& dex_file,
    const DexFile::AnnotationSetItem* disk_annotations_item, uint32_t offset) {
  if (disk_annotations_item == nullptr || (disk_annotations_item->size_ == 0 && offset == 0)) {
    return nullptr;
  }
  AnnotationSetItem* annotation_set_item = annotation_set_items_map_.GetExistingObject(offset);
  if (annotation_set_item == nullptr) {
    std::vector<AnnotationItem*>* items = new std::vector<AnnotationItem*>();
    for (uint32_t i = 0; i < disk_annotations_item->size_; ++i) {
      const DexFile::AnnotationItem* annotation =
          dex_file.GetAnnotationItem(disk_annotations_item, i);
      if (annotation == nullptr) {
        continue;
      }
      AnnotationItem* annotation_item = CreateAnnotationItem(dex_file, annotation);
      items->push_back(annotation_item);
    }
    annotation_set_item =
        annotation_set_items_map_.CreateAndAddItem(header_->AnnotationSetItems(),
                                                   eagerly_assign_offsets_,
                                                   offset,
                                                   items);
  }
  return annotation_set_item;
}

AnnotationsDirectoryItem* BuilderMaps::CreateAnnotationsDirectoryItem(const DexFile& dex_file,
    const DexFile::AnnotationsDirectoryItem* disk_annotations_item, uint32_t offset) {
  AnnotationsDirectoryItem* annotations_directory_item =
      annotations_directory_items_map_.GetExistingObject(offset);
  if (annotations_directory_item != nullptr) {
    return annotations_directory_item;
  }
  const DexFile::AnnotationSetItem* class_set_item =
      dex_file.GetClassAnnotationSet(disk_annotations_item);
  AnnotationSetItem* class_annotation = nullptr;
  if (class_set_item != nullptr) {
    uint32_t item_offset = disk_annotations_item->class_annotations_off_;
    class_annotation = CreateAnnotationSetItem(dex_file, class_set_item, item_offset);
  }
  const DexFile::FieldAnnotationsItem* fields =
      dex_file.GetFieldAnnotations(disk_annotations_item);
  FieldAnnotationVector* field_annotations = nullptr;
  if (fields != nullptr) {
    field_annotations = new FieldAnnotationVector();
    for (uint32_t i = 0; i < disk_annotations_item->fields_size_; ++i) {
      FieldId* field_id = header_->FieldIds()[fields[i].field_idx_];
      const DexFile::AnnotationSetItem* field_set_item =
          dex_file.GetFieldAnnotationSetItem(fields[i]);
      uint32_t annotation_set_offset = fields[i].annotations_off_;
      AnnotationSetItem* annotation_set_item =
          CreateAnnotationSetItem(dex_file, field_set_item, annotation_set_offset);
      field_annotations->push_back(std::unique_ptr<FieldAnnotation>(
          new FieldAnnotation(field_id, annotation_set_item)));
    }
  }
  const DexFile::MethodAnnotationsItem* methods =
      dex_file.GetMethodAnnotations(disk_annotations_item);
  MethodAnnotationVector* method_annotations = nullptr;
  if (methods != nullptr) {
    method_annotations = new MethodAnnotationVector();
    for (uint32_t i = 0; i < disk_annotations_item->methods_size_; ++i) {
      MethodId* method_id = header_->MethodIds()[methods[i].method_idx_];
      const DexFile::AnnotationSetItem* method_set_item =
          dex_file.GetMethodAnnotationSetItem(methods[i]);
      uint32_t annotation_set_offset = methods[i].annotations_off_;
      AnnotationSetItem* annotation_set_item =
          CreateAnnotationSetItem(dex_file, method_set_item, annotation_set_offset);
      method_annotations->push_back(std::unique_ptr<MethodAnnotation>(
          new MethodAnnotation(method_id, annotation_set_item)));
    }
  }
  const DexFile::ParameterAnnotationsItem* parameters =
      dex_file.GetParameterAnnotations(disk_annotations_item);
  ParameterAnnotationVector* parameter_annotations = nullptr;
  if (parameters != nullptr) {
    parameter_annotations = new ParameterAnnotationVector();
    for (uint32_t i = 0; i < disk_annotations_item->parameters_size_; ++i) {
      MethodId* method_id = header_->MethodIds()[parameters[i].method_idx_];
      const DexFile::AnnotationSetRefList* list =
          dex_file.GetParameterAnnotationSetRefList(&parameters[i]);
      parameter_annotations->push_back(std::unique_ptr<ParameterAnnotation>(
          GenerateParameterAnnotation(dex_file, method_id, list, parameters[i].annotations_off_)));
    }
  }
  // TODO: Calculate the size of the annotations directory.
  return annotations_directory_items_map_.CreateAndAddItem(header_->AnnotationsDirectoryItems(),
                                                           eagerly_assign_offsets_,
                                                           offset,
                                                           class_annotation,
                                                           field_annotations,
                                                           method_annotations,
                                                           parameter_annotations);
}

CodeItem* BuilderMaps::DedupeOrCreateCodeItem(const DexFile& dex_file,
                                              const DexFile::CodeItem* disk_code_item,
                                              uint32_t offset,
                                              uint32_t dex_method_index) {
  if (disk_code_item == nullptr) {
    return nullptr;
  }
  CodeItemDebugInfoAccessor accessor(dex_file, disk_code_item, dex_method_index);
  const uint32_t debug_info_offset = accessor.DebugInfoOffset();

  // Create the offsets pair and dedupe based on it.
  std::pair<uint32_t, uint32_t> offsets_pair(offset, debug_info_offset);
  auto existing = code_items_map_.find(offsets_pair);
  if (existing != code_items_map_.end()) {
    return existing->second;
  }

  const uint8_t* debug_info_stream = dex_file.GetDebugInfoStream(debug_info_offset);
  DebugInfoItem* debug_info = nullptr;
  if (debug_info_stream != nullptr) {
    debug_info = debug_info_items_map_.GetExistingObject(debug_info_offset);
    if (debug_info == nullptr) {
      uint32_t debug_info_size = GetDebugInfoStreamSize(debug_info_stream);
      uint8_t* debug_info_buffer = new uint8_t[debug_info_size];
      memcpy(debug_info_buffer, debug_info_stream, debug_info_size);
      debug_info = debug_info_items_map_.CreateAndAddItem(header_->DebugInfoItems(),
                                                          eagerly_assign_offsets_,
                                                          debug_info_offset,
                                                          debug_info_size,
                                                          debug_info_buffer);
    }
  }

  uint32_t insns_size = accessor.InsnsSizeInCodeUnits();
  uint16_t* insns = new uint16_t[insns_size];
  memcpy(insns, accessor.Insns(), insns_size * sizeof(uint16_t));

  TryItemVector* tries = nullptr;
  CatchHandlerVector* handler_list = nullptr;
  if (accessor.TriesSize() > 0) {
    tries = new TryItemVector();
    handler_list = new CatchHandlerVector();
    for (const DexFile::TryItem& disk_try_item : accessor.TryItems()) {
      uint32_t start_addr = disk_try_item.start_addr_;
      uint16_t insn_count = disk_try_item.insn_count_;
      uint16_t handler_off = disk_try_item.handler_off_;
      const CatchHandler* handlers = nullptr;
      for (std::unique_ptr<const CatchHandler>& existing_handlers : *handler_list) {
        if (handler_off == existing_handlers->GetListOffset()) {
          handlers = existing_handlers.get();
          break;
        }
      }
      if (handlers == nullptr) {
        bool catch_all = false;
        TypeAddrPairVector* addr_pairs = new TypeAddrPairVector();
        for (CatchHandlerIterator it(accessor, disk_try_item); it.HasNext(); it.Next()) {
          const dex::TypeIndex type_index = it.GetHandlerTypeIndex();
          const TypeId* type_id = header_->GetTypeIdOrNullPtr(type_index.index_);
          catch_all |= type_id == nullptr;
          addr_pairs->push_back(std::unique_ptr<const TypeAddrPair>(
              new TypeAddrPair(type_id, it.GetHandlerAddress())));
        }
        handlers = new CatchHandler(catch_all, handler_off, addr_pairs);
        handler_list->push_back(std::unique_ptr<const CatchHandler>(handlers));
      }
      TryItem* try_item = new TryItem(start_addr, insn_count, handlers);
      tries->push_back(std::unique_ptr<const TryItem>(try_item));
    }
    // Manually walk catch handlers list and add any missing handlers unreferenced by try items.
    const uint8_t* handlers_base = accessor.GetCatchHandlerData();
    const uint8_t* handlers_data = handlers_base;
    uint32_t handlers_size = DecodeUnsignedLeb128(&handlers_data);
    while (handlers_size > handler_list->size()) {
      bool already_added = false;
      uint16_t handler_off = handlers_data - handlers_base;
      for (std::unique_ptr<const CatchHandler>& existing_handlers : *handler_list) {
        if (handler_off == existing_handlers->GetListOffset()) {
          already_added = true;
          break;
        }
      }
      int32_t size = DecodeSignedLeb128(&handlers_data);
      bool has_catch_all = size <= 0;
      if (has_catch_all) {
        size = -size;
      }
      if (already_added) {
        for (int32_t i = 0; i < size; i++) {
          DecodeUnsignedLeb128(&handlers_data);
          DecodeUnsignedLeb128(&handlers_data);
        }
        if (has_catch_all) {
          DecodeUnsignedLeb128(&handlers_data);
        }
        continue;
      }
      TypeAddrPairVector* addr_pairs = new TypeAddrPairVector();
      for (int32_t i = 0; i < size; i++) {
        const TypeId* type_id =
            header_->GetTypeIdOrNullPtr(DecodeUnsignedLeb128(&handlers_data));
        uint32_t addr = DecodeUnsignedLeb128(&handlers_data);
        addr_pairs->push_back(
            std::unique_ptr<const TypeAddrPair>(new TypeAddrPair(type_id, addr)));
      }
      if (has_catch_all) {
        uint32_t addr = DecodeUnsignedLeb128(&handlers_data);
        addr_pairs->push_back(
            std::unique_ptr<const TypeAddrPair>(new TypeAddrPair(nullptr, addr)));
      }
      const CatchHandler* handler = new CatchHandler(has_catch_all, handler_off, addr_pairs);
      handler_list->push_back(std::unique_ptr<const CatchHandler>(handler));
    }
  }

  uint32_t size = dex_file.GetCodeItemSize(*disk_code_item);
  CodeItem* code_item = header_->CodeItems().CreateAndAddItem(accessor.RegistersSize(),
                                                                  accessor.InsSize(),
                                                                  accessor.OutsSize(),
                                                                  debug_info,
                                                                  insns_size,
                                                                  insns,
                                                                  tries,
                                                                  handler_list);
  code_item->SetSize(size);

  // Add the code item to the map.
  DCHECK(!code_item->OffsetAssigned());
  if (eagerly_assign_offsets_) {
    code_item->SetOffset(offset);
  }
  code_items_map_.emplace(offsets_pair, code_item);

  // Add "fixup" references to types, strings, methods, and fields.
  // This is temporary, as we will probably want more detailed parsing of the
  // instructions here.
  std::vector<TypeId*> type_ids;
  std::vector<StringId*> string_ids;
  std::vector<MethodId*> method_ids;
  std::vector<FieldId*> field_ids;
  if (GetIdsFromByteCode(code_item,
                         /*out*/ &type_ids,
                         /*out*/ &string_ids,
                         /*out*/ &method_ids,
                         /*out*/ &field_ids)) {
    CodeFixups* fixups = new CodeFixups(std::move(type_ids),
                                        std::move(string_ids),
                                        std::move(method_ids),
                                        std::move(field_ids));
    code_item->SetCodeFixups(fixups);
  }

  return code_item;
}

ClassData* BuilderMaps::CreateClassData(
    const DexFile& dex_file, const uint8_t* encoded_data, uint32_t offset) {
  // Read the fields and methods defined by the class, resolving the circular reference from those
  // to classes by setting class at the same time.
  ClassData* class_data = class_datas_map_.GetExistingObject(offset);
  if (class_data == nullptr && encoded_data != nullptr) {
    ClassDataItemIterator cdii(dex_file, encoded_data);
    // Static fields.
    FieldItemVector* static_fields = new FieldItemVector();
    for (; cdii.HasNextStaticField(); cdii.Next()) {
      FieldId* field_item = header_->FieldIds()[cdii.GetMemberIndex()];
      uint32_t access_flags = cdii.GetRawMemberAccessFlags();
      static_fields->emplace_back(access_flags, field_item);
    }
    // Instance fields.
    FieldItemVector* instance_fields = new FieldItemVector();
    for (; cdii.HasNextInstanceField(); cdii.Next()) {
      FieldId* field_item = header_->FieldIds()[cdii.GetMemberIndex()];
      uint32_t access_flags = cdii.GetRawMemberAccessFlags();
      instance_fields->emplace_back(access_flags, field_item);
    }
    // Direct methods.
    MethodItemVector* direct_methods = new MethodItemVector();
    for (; cdii.HasNextDirectMethod(); cdii.Next()) {
      direct_methods->push_back(GenerateMethodItem(dex_file, cdii));
    }
    // Virtual methods.
    MethodItemVector* virtual_methods = new MethodItemVector();
    for (; cdii.HasNextVirtualMethod(); cdii.Next()) {
      virtual_methods->push_back(GenerateMethodItem(dex_file, cdii));
    }
    class_data = class_datas_map_.CreateAndAddItem(header_->ClassDatas(),
                                                   eagerly_assign_offsets_,
                                                   offset,
                                                   static_fields,
                                                   instance_fields,
                                                   direct_methods,
                                                   virtual_methods);
    class_data->SetSize(cdii.EndDataPointer() - encoded_data);
  }
  return class_data;
}

void BuilderMaps::SortVectorsByMapOrder() {
  header_->StringDatas().SortByMapOrder(string_datas_map_.Collection());
  header_->TypeLists().SortByMapOrder(type_lists_map_.Collection());
  header_->EncodedArrayItems().SortByMapOrder(encoded_array_items_map_.Collection());
  header_->AnnotationItems().SortByMapOrder(annotation_items_map_.Collection());
  header_->AnnotationSetItems().SortByMapOrder(annotation_set_items_map_.Collection());
  header_->AnnotationSetRefLists().SortByMapOrder(annotation_set_ref_lists_map_.Collection());
  header_->AnnotationsDirectoryItems().SortByMapOrder(
      annotations_directory_items_map_.Collection());
  header_->DebugInfoItems().SortByMapOrder(debug_info_items_map_.Collection());
  header_->CodeItems().SortByMapOrder(code_items_map_);
  header_->ClassDatas().SortByMapOrder(class_datas_map_.Collection());
}

bool BuilderMaps::GetIdsFromByteCode(const CodeItem* code,
                                     std::vector<TypeId*>* type_ids,
                                     std::vector<StringId*>* string_ids,
                                     std::vector<MethodId*>* method_ids,
                                     std::vector<FieldId*>* field_ids) {
  bool has_id = false;
  IterationRange<DexInstructionIterator> instructions = code->Instructions();
  SafeDexInstructionIterator it(instructions.begin(), instructions.end());
  for (; !it.IsErrorState() && it < instructions.end(); ++it) {
    // In case the instruction goes past the end of the code item, make sure to not process it.
    SafeDexInstructionIterator next = it;
    ++next;
    if (next.IsErrorState()) {
      break;
    }
    has_id |= GetIdFromInstruction(&it.Inst(), type_ids, string_ids, method_ids, field_ids);
  }  // for
  return has_id;
}

bool BuilderMaps::GetIdFromInstruction(const Instruction* dec_insn,
                                       std::vector<TypeId*>* type_ids,
                                       std::vector<StringId*>* string_ids,
                                       std::vector<MethodId*>* method_ids,
                                       std::vector<FieldId*>* field_ids) {
  // Determine index and width of the string.
  uint32_t index = 0;
  switch (Instruction::FormatOf(dec_insn->Opcode())) {
    // SOME NOT SUPPORTED:
    // case Instruction::k20bc:
    case Instruction::k21c:
    case Instruction::k35c:
    // case Instruction::k35ms:
    case Instruction::k3rc:
    // case Instruction::k3rms:
    // case Instruction::k35mi:
    // case Instruction::k3rmi:
    case Instruction::k45cc:
    case Instruction::k4rcc:
      index = dec_insn->VRegB();
      break;
    case Instruction::k31c:
      index = dec_insn->VRegB();
      break;
    case Instruction::k22c:
    // case Instruction::k22cs:
      index = dec_insn->VRegC();
      break;
    default:
      break;
  }  // switch

  // Determine index type, and add reference to the appropriate collection.
  switch (Instruction::IndexTypeOf(dec_insn->Opcode())) {
    case Instruction::kIndexTypeRef:
      if (index < header_->TypeIds().Size()) {
        type_ids->push_back(header_->TypeIds()[index]);
        return true;
      }
      break;
    case Instruction::kIndexStringRef:
      if (index < header_->StringIds().Size()) {
        string_ids->push_back(header_->StringIds()[index]);
        return true;
      }
      break;
    case Instruction::kIndexMethodRef:
    case Instruction::kIndexMethodAndProtoRef:
      if (index < header_->MethodIds().Size()) {
        method_ids->push_back(header_->MethodIds()[index]);
        return true;
      }
      break;
    case Instruction::kIndexFieldRef:
      if (index < header_->FieldIds().Size()) {
        field_ids->push_back(header_->FieldIds()[index]);
        return true;
      }
      break;
    case Instruction::kIndexUnknown:
    case Instruction::kIndexNone:
    case Instruction::kIndexVtableOffset:
    case Instruction::kIndexFieldOffset:
    default:
      break;
  }  // switch
  return false;
}

EncodedValue* BuilderMaps::ReadEncodedValue(const DexFile& dex_file, const uint8_t** data) {
  const uint8_t encoded_value = *(*data)++;
  const uint8_t type = encoded_value & 0x1f;
  EncodedValue* item = new EncodedValue(type);
  ReadEncodedValue(dex_file, data, type, encoded_value >> 5, item);
  return item;
}

EncodedValue* BuilderMaps::ReadEncodedValue(const DexFile& dex_file,
                                            const uint8_t** data,
                                            uint8_t type,
                                            uint8_t length) {
  EncodedValue* item = new EncodedValue(type);
  ReadEncodedValue(dex_file, data, type, length, item);
  return item;
}

void BuilderMaps::ReadEncodedValue(const DexFile& dex_file,
                                   const uint8_t** data,
                                   uint8_t type,
                                   uint8_t length,
                                   EncodedValue* item) {
  switch (type) {
    case DexFile::kDexAnnotationByte:
      item->SetByte(static_cast<int8_t>(ReadVarWidth(data, length, false)));
      break;
    case DexFile::kDexAnnotationShort:
      item->SetShort(static_cast<int16_t>(ReadVarWidth(data, length, true)));
      break;
    case DexFile::kDexAnnotationChar:
      item->SetChar(static_cast<uint16_t>(ReadVarWidth(data, length, false)));
      break;
    case DexFile::kDexAnnotationInt:
      item->SetInt(static_cast<int32_t>(ReadVarWidth(data, length, true)));
      break;
    case DexFile::kDexAnnotationLong:
      item->SetLong(static_cast<int64_t>(ReadVarWidth(data, length, true)));
      break;
    case DexFile::kDexAnnotationFloat: {
      // Fill on right.
      union {
        float f;
        uint32_t data;
      } conv;
      conv.data = static_cast<uint32_t>(ReadVarWidth(data, length, false)) << (3 - length) * 8;
      item->SetFloat(conv.f);
      break;
    }
    case DexFile::kDexAnnotationDouble: {
      // Fill on right.
      union {
        double d;
        uint64_t data;
      } conv;
      conv.data = ReadVarWidth(data, length, false) << (7 - length) * 8;
      item->SetDouble(conv.d);
      break;
    }
    case DexFile::kDexAnnotationMethodType: {
      const uint32_t proto_index = static_cast<uint32_t>(ReadVarWidth(data, length, false));
      item->SetProtoId(header_->ProtoIds()[proto_index]);
      break;
    }
    case DexFile::kDexAnnotationMethodHandle: {
      const uint32_t method_handle_index = static_cast<uint32_t>(ReadVarWidth(data, length, false));
      item->SetMethodHandle(header_->MethodHandleItems()[method_handle_index]);
      break;
    }
    case DexFile::kDexAnnotationString: {
      const uint32_t string_index = static_cast<uint32_t>(ReadVarWidth(data, length, false));
      item->SetStringId(header_->StringIds()[string_index]);
      break;
    }
    case DexFile::kDexAnnotationType: {
      const uint32_t string_index = static_cast<uint32_t>(ReadVarWidth(data, length, false));
      item->SetTypeId(header_->TypeIds()[string_index]);
      break;
    }
    case DexFile::kDexAnnotationField:
    case DexFile::kDexAnnotationEnum: {
      const uint32_t field_index = static_cast<uint32_t>(ReadVarWidth(data, length, false));
      item->SetFieldId(header_->FieldIds()[field_index]);
      break;
    }
    case DexFile::kDexAnnotationMethod: {
      const uint32_t method_index = static_cast<uint32_t>(ReadVarWidth(data, length, false));
      item->SetMethodId(header_->MethodIds()[method_index]);
      break;
    }
    case DexFile::kDexAnnotationArray: {
      EncodedValueVector* values = new EncodedValueVector();
      const uint32_t offset = *data - dex_file.DataBegin();
      const uint32_t size = DecodeUnsignedLeb128(data);
      // Decode all elements.
      for (uint32_t i = 0; i < size; i++) {
        values->push_back(std::unique_ptr<EncodedValue>(ReadEncodedValue(dex_file, data)));
      }
      EncodedArrayItem* array_item = new EncodedArrayItem(values);
      if (eagerly_assign_offsets_) {
        array_item->SetOffset(offset);
      }
      item->SetEncodedArray(array_item);
      break;
    }
    case DexFile::kDexAnnotationAnnotation: {
      AnnotationElementVector* elements = new AnnotationElementVector();
      const uint32_t type_idx = DecodeUnsignedLeb128(data);
      const uint32_t size = DecodeUnsignedLeb128(data);
      // Decode all name=value pairs.
      for (uint32_t i = 0; i < size; i++) {
        const uint32_t name_index = DecodeUnsignedLeb128(data);
        elements->push_back(std::unique_ptr<AnnotationElement>(
            new AnnotationElement(header_->StringIds()[name_index],
                                  ReadEncodedValue(dex_file, data))));
      }
      item->SetEncodedAnnotation(new EncodedAnnotation(header_->TypeIds()[type_idx], elements));
      break;
    }
    case DexFile::kDexAnnotationNull:
      break;
    case DexFile::kDexAnnotationBoolean:
      item->SetBoolean(length != 0);
      break;
    default:
      break;
  }
}

MethodItem BuilderMaps::GenerateMethodItem(const DexFile& dex_file, ClassDataItemIterator& cdii) {
  MethodId* method_id = header_->MethodIds()[cdii.GetMemberIndex()];
  uint32_t access_flags = cdii.GetRawMemberAccessFlags();
  const DexFile::CodeItem* disk_code_item = cdii.GetMethodCodeItem();
  // Temporary hack to prevent incorrectly deduping code items if they have the same offset since
  // they may have different debug info streams.
  CodeItem* code_item = DedupeOrCreateCodeItem(dex_file,
                                               disk_code_item,
                                               cdii.GetMethodCodeItemOffset(),
                                               cdii.GetMemberIndex());
  return MethodItem(access_flags, method_id, code_item);
}

ParameterAnnotation* BuilderMaps::GenerateParameterAnnotation(
    const DexFile& dex_file,
    MethodId* method_id,
    const DexFile::AnnotationSetRefList* annotation_set_ref_list,
    uint32_t offset) {
  AnnotationSetRefList* set_ref_list = annotation_set_ref_lists_map_.GetExistingObject(offset);
  if (set_ref_list == nullptr) {
    std::vector<AnnotationSetItem*>* annotations = new std::vector<AnnotationSetItem*>();
    for (uint32_t i = 0; i < annotation_set_ref_list->size_; ++i) {
      const DexFile::AnnotationSetItem* annotation_set_item =
          dex_file.GetSetRefItemItem(&annotation_set_ref_list->list_[i]);
      uint32_t set_offset = annotation_set_ref_list->list_[i].annotations_off_;
      annotations->push_back(CreateAnnotationSetItem(dex_file, annotation_set_item, set_offset));
    }
    set_ref_list =
        annotation_set_ref_lists_map_.CreateAndAddItem(header_->AnnotationSetRefLists(),
                                                       eagerly_assign_offsets_,
                                                       offset,
                                                       annotations);
  }
  return new ParameterAnnotation(method_id, set_ref_list);
}

}  // namespace dex_ir
}  // namespace art
