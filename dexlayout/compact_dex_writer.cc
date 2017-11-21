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

#include "cdex/compact_dex_file.h"

namespace art {

void CompactDexWriter::WriteHeader() {
  CompactDexFile::Header header;
  CompactDexFile::WriteMagic(&header.magic_[0]);
  CompactDexFile::WriteCurrentVersion(&header.magic_[0]);
  header.checksum_ = header_->Checksum();
  std::copy_n(header_->Signature(), DexFile::kSha1DigestSize, header.signature_);
  header.file_size_ = header_->FileSize();
  header.header_size_ = header_->GetSize();
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
  UNUSED(Write(reinterpret_cast<uint8_t*>(&header), sizeof(header), 0u));
}

}  // namespace art
