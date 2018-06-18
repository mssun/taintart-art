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
 * Implementation file of the dexlayout utility.
 *
 * This is a tool to read dex files into an internal representation,
 * reorganize the representation, and emit dex files with a better
 * file layout.
 */

#include "dex_ir.h"

#include "dex/code_item_accessors-inl.h"
#include "dex/dex_file_exception_helpers.h"
#include "dex/dex_instruction-inl.h"
#include "dex_ir_builder.h"

namespace art {
namespace dex_ir {

static uint32_t HeaderOffset(const dex_ir::Header* header ATTRIBUTE_UNUSED) {
  return 0;
}

static uint32_t HeaderSize(const dex_ir::Header* header ATTRIBUTE_UNUSED) {
  // Size is in elements, so there is only one header.
  return 1;
}

// The description of each dex file section type.
struct FileSectionDescriptor {
 public:
  std::string name;
  uint16_t type;
  // A function that when applied to a collection object, gives the size of the section.
  std::function<uint32_t(dex_ir::Header*)> size_fn;
  // A function that when applied to a collection object, gives the offset of the section.
  std::function<uint32_t(dex_ir::Header*)> offset_fn;
};

static const FileSectionDescriptor kFileSectionDescriptors[] = {
  {
    "Header",
    DexFile::kDexTypeHeaderItem,
    &HeaderSize,
    &HeaderOffset,
  }, {
    "StringId",
    DexFile::kDexTypeStringIdItem,
    [](const dex_ir::Header* h) { return h->StringIds().Size(); },
    [](const dex_ir::Header* h) { return h->StringIds().GetOffset(); }
  }, {
    "TypeId",
    DexFile::kDexTypeTypeIdItem,
    [](const dex_ir::Header* h) { return h->TypeIds().Size(); },
    [](const dex_ir::Header* h) { return h->TypeIds().GetOffset(); }
  }, {
    "ProtoId",
    DexFile::kDexTypeProtoIdItem,
    [](const dex_ir::Header* h) { return h->ProtoIds().Size(); },
    [](const dex_ir::Header* h) { return h->ProtoIds().GetOffset(); }
  }, {
    "FieldId",
    DexFile::kDexTypeFieldIdItem,
    [](const dex_ir::Header* h) { return h->FieldIds().Size(); },
    [](const dex_ir::Header* h) { return h->FieldIds().GetOffset(); }
  }, {
    "MethodId",
    DexFile::kDexTypeMethodIdItem,
    [](const dex_ir::Header* h) { return h->MethodIds().Size(); },
    [](const dex_ir::Header* h) { return h->MethodIds().GetOffset(); }
  }, {
    "ClassDef",
    DexFile::kDexTypeClassDefItem,
    [](const dex_ir::Header* h) { return h->ClassDefs().Size(); },
    [](const dex_ir::Header* h) { return h->ClassDefs().GetOffset(); }
  }, {
    "CallSiteId",
    DexFile::kDexTypeCallSiteIdItem,
    [](const dex_ir::Header* h) { return h->CallSiteIds().Size(); },
    [](const dex_ir::Header* h) { return h->CallSiteIds().GetOffset(); }
  }, {
    "MethodHandle",
    DexFile::kDexTypeMethodHandleItem,
    [](const dex_ir::Header* h) { return h->MethodHandleItems().Size(); },
    [](const dex_ir::Header* h) { return h->MethodHandleItems().GetOffset(); }
  }, {
    "StringData",
    DexFile::kDexTypeStringDataItem,
    [](const dex_ir::Header* h) { return h->StringDatas().Size(); },
    [](const dex_ir::Header* h) { return h->StringDatas().GetOffset(); }
  }, {
    "TypeList",
    DexFile::kDexTypeTypeList,
    [](const dex_ir::Header* h) { return h->TypeLists().Size(); },
    [](const dex_ir::Header* h) { return h->TypeLists().GetOffset(); }
  }, {
    "EncArr",
    DexFile::kDexTypeEncodedArrayItem,
    [](const dex_ir::Header* h) { return h->EncodedArrayItems().Size(); },
    [](const dex_ir::Header* h) { return h->EncodedArrayItems().GetOffset(); }
  }, {
    "Annotation",
    DexFile::kDexTypeAnnotationItem,
    [](const dex_ir::Header* h) { return h->AnnotationItems().Size(); },
    [](const dex_ir::Header* h) { return h->AnnotationItems().GetOffset(); }
  }, {
    "AnnoSet",
    DexFile::kDexTypeAnnotationSetItem,
    [](const dex_ir::Header* h) { return h->AnnotationSetItems().Size(); },
    [](const dex_ir::Header* h) { return h->AnnotationSetItems().GetOffset(); }
  }, {
    "AnnoSetRL",
    DexFile::kDexTypeAnnotationSetRefList,
    [](const dex_ir::Header* h) { return h->AnnotationSetRefLists().Size(); },
    [](const dex_ir::Header* h) { return h->AnnotationSetRefLists().GetOffset(); }
  }, {
    "AnnoDir",
    DexFile::kDexTypeAnnotationsDirectoryItem,
    [](const dex_ir::Header* h) { return h->AnnotationsDirectoryItems().Size(); },
    [](const dex_ir::Header* h) { return h->AnnotationsDirectoryItems().GetOffset(); }
  }, {
    "DebugInfo",
    DexFile::kDexTypeDebugInfoItem,
    [](const dex_ir::Header* h) { return h->DebugInfoItems().Size(); },
    [](const dex_ir::Header* h) { return h->DebugInfoItems().GetOffset(); }
  }, {
    "CodeItem",
    DexFile::kDexTypeCodeItem,
    [](const dex_ir::Header* h) { return h->CodeItems().Size(); },
    [](const dex_ir::Header* h) { return h->CodeItems().GetOffset(); }
  }, {
    "ClassData",
    DexFile::kDexTypeClassDataItem,
    [](const dex_ir::Header* h) { return h->ClassDatas().Size(); },
    [](const dex_ir::Header* h) { return h->ClassDatas().GetOffset(); }
  }
};

std::vector<dex_ir::DexFileSection> GetSortedDexFileSections(dex_ir::Header* header,
                                                             dex_ir::SortDirection direction) {
  std::vector<dex_ir::DexFileSection> sorted_sections;
  // Build the table that will map from offset to color
  for (const FileSectionDescriptor& s : kFileSectionDescriptors) {
    sorted_sections.push_back(dex_ir::DexFileSection(s.name,
                                                     s.type,
                                                     s.size_fn(header),
                                                     s.offset_fn(header)));
  }
  // Sort by offset.
  std::sort(sorted_sections.begin(),
            sorted_sections.end(),
            [=](dex_ir::DexFileSection& a, dex_ir::DexFileSection& b) {
              if (direction == SortDirection::kSortDescending) {
                return a.offset > b.offset;
              } else {
                return a.offset < b.offset;
              }
            });
  return sorted_sections;
}

}  // namespace dex_ir
}  // namespace art
