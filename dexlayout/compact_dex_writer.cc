/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include "compact_dex_writer.h"

#include "base/logging.h"
#include "base/time_utils.h"
#include "dex/compact_dex_debug_info.h"
#include "dex/compact_dex_file.h"
#include "dexlayout.h"

namespace art {

CompactDexWriter::CompactDexWriter(dex_ir::Header* header,
                                   MemMap* mem_map,
                                   DexLayout* dex_layout,
                                   CompactDexLevel compact_dex_level)
    : DexWriter(header, mem_map, dex_layout, /*compute_offsets*/ true),
      compact_dex_level_(compact_dex_level),
      data_dedupe_(/*bucket_count*/ 32,
                   HashedMemoryRange::HashEqual(mem_map->Begin()),
                   HashedMemoryRange::HashEqual(mem_map->Begin())) {
  CHECK(compact_dex_level_ != CompactDexLevel::kCompactDexLevelNone);
}

uint32_t CompactDexWriter::WriteDebugInfoOffsetTable(uint32_t offset) {
  const uint32_t start_offset = offset;
  const dex_ir::Collections& collections = header_->GetCollections();
  // Debug offsets for method indexes. 0 means no debug info.
  std::vector<uint32_t> debug_info_offsets(collections.MethodIdsSize(), 0u);

  static constexpr InvokeType invoke_types[] = {
    kDirect,
    kVirtual
  };

  for (InvokeType invoke_type : invoke_types) {
    for (const std::unique_ptr<dex_ir::ClassDef>& class_def : collections.ClassDefs()) {
      // Skip classes that are not defined in this dex file.
      dex_ir::ClassData* class_data = class_def->GetClassData();
      if (class_data == nullptr) {
        continue;
      }
      for (auto& method : *(invoke_type == InvokeType::kDirect
                                ? class_data->DirectMethods()
                                : class_data->VirtualMethods())) {
        const dex_ir::MethodId* method_id = method->GetMethodId();
        dex_ir::CodeItem* code_item = method->GetCodeItem();
        if (code_item != nullptr && code_item->DebugInfo() != nullptr) {
          const uint32_t debug_info_offset = code_item->DebugInfo()->GetOffset();
          const uint32_t method_idx = method_id->GetIndex();
          if (debug_info_offsets[method_idx] != 0u) {
            CHECK_EQ(debug_info_offset, debug_info_offsets[method_idx]);
          }
          debug_info_offsets[method_idx] = debug_info_offset;
        }
      }
    }
  }

  std::vector<uint8_t> data;
  debug_info_base_ = 0u;
  debug_info_offsets_table_offset_ = 0u;
  CompactDexDebugInfoOffsetTable::Build(debug_info_offsets,
                                        &data,
                                        &debug_info_base_,
                                        &debug_info_offsets_table_offset_);
  // Align the table and write it out.
  offset = RoundUp(offset, CompactDexDebugInfoOffsetTable::kAlignment);
  debug_info_offsets_pos_ = offset;
  offset += Write(data.data(), data.size(), offset);

  // Verify that the whole table decodes as expected and measure average performance.
  const bool kMeasureAndTestOutput = dex_layout_->GetOptions().verify_output_;
  if (kMeasureAndTestOutput && !debug_info_offsets.empty()) {
    uint64_t start_time = NanoTime();
    CompactDexDebugInfoOffsetTable::Accessor accessor(mem_map_->Begin() + debug_info_offsets_pos_,
                                                      debug_info_base_,
                                                      debug_info_offsets_table_offset_);

    for (size_t i = 0; i < debug_info_offsets.size(); ++i) {
      CHECK_EQ(accessor.GetDebugInfoOffset(i), debug_info_offsets[i]);
    }
    uint64_t end_time = NanoTime();
    VLOG(dex) << "Average lookup time (ns) for debug info offsets: "
              << (end_time - start_time) / debug_info_offsets.size();
  }

  return offset - start_offset;
}

uint32_t CompactDexWriter::WriteCodeItem(dex_ir::CodeItem* code_item,
                                         uint32_t offset,
                                         bool reserve_only) {
  DCHECK(code_item != nullptr);
  DCHECK(!reserve_only) << "Not supported because of deduping.";
  const uint32_t start_offset = offset;

  // Align to minimum requirements, additional alignment requirements are handled below after we
  // know the preheader size.
  offset = RoundUp(offset, CompactDexFile::CodeItem::kAlignment);

  CompactDexFile::CodeItem disk_code_item;

  uint16_t preheader_storage[CompactDexFile::CodeItem::kMaxPreHeaderSize] = {};
  uint16_t* preheader_end = preheader_storage + CompactDexFile::CodeItem::kMaxPreHeaderSize;
  const uint16_t* preheader = disk_code_item.Create(
      code_item->RegistersSize(),
      code_item->InsSize(),
      code_item->OutsSize(),
      code_item->TriesSize(),
      code_item->InsnsSize(),
      preheader_end);
  const size_t preheader_bytes = (preheader_end - preheader) * sizeof(preheader[0]);

  static constexpr size_t kPayloadInstructionRequiredAlignment = 4;
  const uint32_t current_code_item_start = offset + preheader_bytes;
  if (!IsAlignedParam(current_code_item_start, kPayloadInstructionRequiredAlignment)) {
    // If the preheader is going to make the code unaligned, consider adding 2 bytes of padding
    // before if required.
    for (const DexInstructionPcPair& instruction : code_item->Instructions()) {
      const Instruction::Code opcode = instruction->Opcode();
      // Payload instructions possibly require special alignment for their data.
      if (opcode == Instruction::FILL_ARRAY_DATA ||
          opcode == Instruction::PACKED_SWITCH ||
          opcode == Instruction::SPARSE_SWITCH) {
        offset += RoundUp(current_code_item_start, kPayloadInstructionRequiredAlignment) -
            current_code_item_start;
        break;
      }
    }
  }

  const uint32_t data_start = offset;

  // Write preheader first.
  offset += Write(reinterpret_cast<const uint8_t*>(preheader), preheader_bytes, offset);
  // Registered offset is after the preheader.
  ProcessOffset(&offset, code_item);
  // Avoid using sizeof so that we don't write the fake instruction array at the end of the code
  // item.
  offset += Write(&disk_code_item,
                  OFFSETOF_MEMBER(CompactDexFile::CodeItem, insns_),
                  offset);
  // Write the instructions.
  offset += Write(code_item->Insns(), code_item->InsnsSize() * sizeof(uint16_t), offset);
  // Write the post instruction data.
  offset += WriteCodeItemPostInstructionData(code_item, offset, reserve_only);

  if (dex_layout_->GetOptions().dedupe_code_items_ && compute_offsets_) {
    // After having written, try to dedupe the whole code item (excluding padding).
    uint32_t deduped_offset = DedupeData(data_start, offset, code_item->GetOffset());
    if (deduped_offset != kDidNotDedupe) {
      code_item->SetOffset(deduped_offset);
      // Undo the offset for all that we wrote since we deduped.
      offset = start_offset;
    }
  }

  return offset - start_offset;
}

uint32_t CompactDexWriter::DedupeData(uint32_t data_start,
                                      uint32_t data_end,
                                      uint32_t item_offset) {
  HashedMemoryRange range {data_start, data_end - data_start};
  auto existing = data_dedupe_.emplace(range, item_offset);
  if (!existing.second) {
    // Failed to insert, item already existed in the map.
    return existing.first->second;
  }
  return kDidNotDedupe;
}

void CompactDexWriter::SortDebugInfosByMethodIndex() {
  dex_ir::Collections& collections = header_->GetCollections();
  static constexpr InvokeType invoke_types[] = {
    kDirect,
    kVirtual
  };
  std::map<const dex_ir::DebugInfoItem*, uint32_t> method_idx_map;
  for (InvokeType invoke_type : invoke_types) {
    for (std::unique_ptr<dex_ir::ClassDef>& class_def : collections.ClassDefs()) {
      // Skip classes that are not defined in this dex file.
      dex_ir::ClassData* class_data = class_def->GetClassData();
      if (class_data == nullptr) {
        continue;
      }
      for (auto& method : *(invoke_type == InvokeType::kDirect
                                ? class_data->DirectMethods()
                                : class_data->VirtualMethods())) {
        const dex_ir::MethodId* method_id = method->GetMethodId();
        dex_ir::CodeItem* code_item = method->GetCodeItem();
        if (code_item != nullptr && code_item->DebugInfo() != nullptr) {
          const dex_ir::DebugInfoItem* debug_item = code_item->DebugInfo();
          method_idx_map.insert(std::make_pair(debug_item, method_id->GetIndex()));
        }
      }
    }
  }
  std::sort(collections.DebugInfoItems().begin(),
            collections.DebugInfoItems().end(),
            [&](const std::unique_ptr<dex_ir::DebugInfoItem>& a,
                const std::unique_ptr<dex_ir::DebugInfoItem>& b) {
    auto it_a = method_idx_map.find(a.get());
    auto it_b = method_idx_map.find(b.get());
    uint32_t idx_a = it_a != method_idx_map.end() ? it_a->second : 0u;
    uint32_t idx_b = it_b != method_idx_map.end() ? it_b->second : 0u;
    return idx_a < idx_b;
  });
}

void CompactDexWriter::WriteHeader() {
  CompactDexFile::Header header;
  CompactDexFile::WriteMagic(&header.magic_[0]);
  CompactDexFile::WriteCurrentVersion(&header.magic_[0]);
  header.checksum_ = header_->Checksum();
  std::copy_n(header_->Signature(), DexFile::kSha1DigestSize, header.signature_);
  header.file_size_ = header_->FileSize();
  // Since we are not necessarily outputting the same format as the input, avoid using the stored
  // header size.
  header.header_size_ = GetHeaderSize();
  header.endian_tag_ = header_->EndianTag();
  header.link_size_ = header_->LinkSize();
  header.link_off_ = header_->LinkOffset();
  const dex_ir::Collections& collections = header_->GetCollections();
  header.map_off_ = collections.MapListOffset();
  header.string_ids_size_ = collections.StringIdsSize();
  header.string_ids_off_ = collections.StringIdsOffset();
  header.type_ids_size_ = collections.TypeIdsSize();
  header.type_ids_off_ = collections.TypeIdsOffset();
  header.proto_ids_size_ = collections.ProtoIdsSize();
  header.proto_ids_off_ = collections.ProtoIdsOffset();
  header.field_ids_size_ = collections.FieldIdsSize();
  header.field_ids_off_ = collections.FieldIdsOffset();
  header.method_ids_size_ = collections.MethodIdsSize();
  header.method_ids_off_ = collections.MethodIdsOffset();
  header.class_defs_size_ = collections.ClassDefsSize();
  header.class_defs_off_ = collections.ClassDefsOffset();
  header.data_size_ = header_->DataSize();
  header.data_off_ = header_->DataOffset();

  // Compact dex specific flags.
  header.debug_info_offsets_pos_ = debug_info_offsets_pos_;
  header.debug_info_offsets_table_offset_ = debug_info_offsets_table_offset_;
  header.debug_info_base_ = debug_info_base_;
  header.feature_flags_ = 0u;
  // In cases where apps are converted to cdex during install, maintain feature flags so that
  // the verifier correctly verifies apps that aren't targetting default methods.
  if (header_->SupportDefaultMethods()) {
    header.feature_flags_ |= static_cast<uint32_t>(CompactDexFile::FeatureFlags::kDefaultMethods);
  }
  UNUSED(Write(reinterpret_cast<uint8_t*>(&header), sizeof(header), 0u));
}

size_t CompactDexWriter::GetHeaderSize() const {
  return sizeof(CompactDexFile::Header);
}

void CompactDexWriter::WriteMemMap() {
  // Starting offset is right after the header.
  uint32_t offset = GetHeaderSize();

  dex_ir::Collections& collection = header_->GetCollections();

  // Based on: https://source.android.com/devices/tech/dalvik/dex-format
  // Since the offsets may not be calculated already, the writing must be done in the correct order.
  const uint32_t string_ids_offset = offset;
  offset += WriteStringIds(offset, /*reserve_only*/ true);
  offset += WriteTypeIds(offset);
  const uint32_t proto_ids_offset = offset;
  offset += WriteProtoIds(offset, /*reserve_only*/ true);
  offset += WriteFieldIds(offset);
  offset += WriteMethodIds(offset);
  const uint32_t class_defs_offset = offset;
  offset += WriteClassDefs(offset, /*reserve_only*/ true);
  const uint32_t call_site_ids_offset = offset;
  offset += WriteCallSiteIds(offset, /*reserve_only*/ true);
  offset += WriteMethodHandles(offset);

  uint32_t data_offset_ = 0u;
  if (compute_offsets_) {
    // Data section.
    offset = RoundUp(offset, kDataSectionAlignment);
    data_offset_ = offset;
  }

  // Write code item first to minimize the space required for encoded methods.
  // For cdex, the code items don't depend on the debug info.
  offset += WriteCodeItems(offset, /*reserve_only*/ false);

  // Sort the debug infos by method index order, this reduces size by ~0.1% by reducing the size of
  // the debug info offset table.
  SortDebugInfosByMethodIndex();
  offset += WriteDebugInfoItems(offset);

  offset += WriteEncodedArrays(offset);
  offset += WriteAnnotations(offset);
  offset += WriteAnnotationSets(offset);
  offset += WriteAnnotationSetRefs(offset);
  offset += WriteAnnotationsDirectories(offset);
  offset += WriteTypeLists(offset);
  offset += WriteClassDatas(offset);
  offset += WriteStringDatas(offset);

  // Write delayed id sections that depend on data sections.
  WriteStringIds(string_ids_offset, /*reserve_only*/ false);
  WriteProtoIds(proto_ids_offset, /*reserve_only*/ false);
  WriteClassDefs(class_defs_offset, /*reserve_only*/ false);
  WriteCallSiteIds(call_site_ids_offset, /*reserve_only*/ false);

  // Write the map list.
  if (compute_offsets_) {
    offset = RoundUp(offset, SectionAlignment(DexFile::kDexTypeMapList));
    collection.SetMapListOffset(offset);
  } else {
    offset = collection.MapListOffset();
  }
  offset += GenerateAndWriteMapItems(offset);
  offset = RoundUp(offset, kDataSectionAlignment);

  // Map items are included in the data section.
  if (compute_offsets_) {
    header_->SetDataSize(offset - data_offset_);
    if (header_->DataSize() != 0) {
      // Offset must be zero when the size is zero.
      header_->SetDataOffset(data_offset_);
    } else {
      header_->SetDataOffset(0u);
    }
  }

  // Write link data if it exists.
  const std::vector<uint8_t>& link_data = collection.LinkData();
  if (link_data.size() > 0) {
    CHECK_EQ(header_->LinkSize(), static_cast<uint32_t>(link_data.size()));
    if (compute_offsets_) {
      header_->SetLinkOffset(offset);
    }
    offset += Write(&link_data[0], link_data.size(), header_->LinkOffset());
  }

  // Write debug info offset table last to make dex file verifier happy.
  offset += WriteDebugInfoOffsetTable(offset);

  // Write header last.
  if (compute_offsets_) {
    header_->SetFileSize(offset);
  }
  WriteHeader();

  if (dex_layout_->GetOptions().update_checksum_) {
    header_->SetChecksum(DexFile::CalculateChecksum(mem_map_->Begin(), offset));
    // Rewrite the header with the calculated checksum.
    WriteHeader();
  }
}

}  // namespace art
