/* Copyright (C) 2017 The Android Open Source Project
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This file implements interfaces from the file jvmti.h. This implementation
 * is licensed under the same terms as the file jvmti.h.  The
 * copyright and license information for the file jvmti.h follows.
 *
 * Copyright (c) 2003, 2011, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

#include "fixed_up_dex_file.h"
#include "dex/art_dex_file_loader.h"
#include "dex/dex_file-inl.h"
#include "dex/dex_file_loader.h"

// Runtime includes.
#include "dex/compact_dex_level.h"
#include "dex_to_dex_decompiler.h"
#include "dexlayout.h"
#include "oat_file.h"
#include "vdex_file.h"

namespace openjdkjvmti {

static void RecomputeDexChecksum(art::DexFile* dex_file)
    REQUIRES_SHARED(art::Locks::mutator_lock_) {
  reinterpret_cast<art::DexFile::Header*>(const_cast<uint8_t*>(dex_file->Begin()))->checksum_ =
      dex_file->CalculateChecksum();
}

static void DoDexUnquicken(const art::DexFile& new_dex_file, const art::DexFile& original_dex_file)
    REQUIRES_SHARED(art::Locks::mutator_lock_) {
  const art::OatDexFile* oat_dex = original_dex_file.GetOatDexFile();
  if (oat_dex == nullptr) {
    return;
  }
  const art::OatFile* oat_file = oat_dex->GetOatFile();
  if (oat_file == nullptr) {
    return;
  }
  const art::VdexFile* vdex = oat_file->GetVdexFile();
  if (vdex == nullptr) {
    return;
  }
  vdex->UnquickenDexFile(new_dex_file, original_dex_file, /* decompile_return_instruction */true);
}

std::unique_ptr<FixedUpDexFile> FixedUpDexFile::Create(const art::DexFile& original) {
  // Copy the data into mutable memory.
  std::vector<unsigned char> data;
  data.resize(original.Size());
  memcpy(data.data(), original.Begin(), original.Size());
  std::string error;
  const art::ArtDexFileLoader dex_file_loader;
  std::unique_ptr<const art::DexFile> new_dex_file(dex_file_loader.Open(
      data.data(),
      data.size(),
      /*location*/"Unquickening_dexfile.dex",
      /*location_checksum*/0,
      /*oat_dex_file*/nullptr,
      /*verify*/false,
      /*verify_checksum*/false,
      &error));
  if (new_dex_file.get() == nullptr) {
    LOG(ERROR) << "Unable to open dex file from memory for unquickening! error: " << error;
    return nullptr;
  }

  DoDexUnquicken(*new_dex_file, original);

  if (original.IsCompactDexFile()) {
    // Since we are supposed to return a standard dex, convert back using dexlayout.
    art::Options options;
    options.output_to_memmap_ = true;
    options.compact_dex_level_ = art::CompactDexLevel::kCompactDexLevelNone;
    options.update_checksum_ = true;
    art::DexLayout dex_layout(options, nullptr, nullptr);
    dex_layout.ProcessDexFile(new_dex_file->GetLocation().c_str(), new_dex_file.get(), 0);
    std::unique_ptr<art::MemMap> mem_map(dex_layout.GetAndReleaseMemMap());

    const uint32_t dex_file_size =
        reinterpret_cast<const art::DexFile::Header*>(mem_map->Begin())->file_size_;
    // Overwrite the dex file stored in data with the new result.
    data.clear();
    data.insert(data.end(), mem_map->Begin(), mem_map->Begin() + dex_file_size);
    new_dex_file = dex_file_loader.Open(
        data.data(),
        data.size(),
        /*location*/"Unquickening_dexfile.dex",
        /*location_checksum*/0,
        /*oat_dex_file*/nullptr,
        /*verify*/false,
        /*verify_checksum*/false,
        &error);
  }

  RecomputeDexChecksum(const_cast<art::DexFile*>(new_dex_file.get()));
  std::unique_ptr<FixedUpDexFile> ret(new FixedUpDexFile(std::move(new_dex_file), std::move(data)));
  return ret;
}

}  // namespace openjdkjvmti
