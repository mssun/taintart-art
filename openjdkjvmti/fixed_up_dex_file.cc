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

#include "base/leb128.h"
#include "fixed_up_dex_file.h"
#include "dex/class_accessor-inl.h"
#include "dex/dex_file-inl.h"
#include "dex/dex_file_loader.h"
#include "dex/dex_file_verifier.h"

// Runtime includes.
#include "dex_container.h"
#include "dex/compact_dex_level.h"
#include "dex_to_dex_decompiler.h"
#include "dexlayout.h"
#include "oat_file.h"
#include "vdex_file.h"

namespace openjdkjvmti {

static void RecomputeDexChecksum(art::DexFile* dex_file) {
  reinterpret_cast<art::DexFile::Header*>(const_cast<uint8_t*>(dex_file->Begin()))->checksum_ =
      dex_file->CalculateChecksum();
}

static const art::VdexFile* GetVdex(const art::DexFile& original_dex_file) {
  const art::OatDexFile* oat_dex = original_dex_file.GetOatDexFile();
  if (oat_dex == nullptr) {
    return nullptr;
  }
  const art::OatFile* oat_file = oat_dex->GetOatFile();
  if (oat_file == nullptr) {
    return nullptr;
  }
  return oat_file->GetVdexFile();
}

static void DoDexUnquicken(const art::DexFile& new_dex_file,
                           const art::DexFile& original_dex_file) {
  const art::VdexFile* vdex = GetVdex(original_dex_file);
  if (vdex != nullptr) {
    vdex->UnquickenDexFile(new_dex_file,
                           original_dex_file,
                           /* decompile_return_instruction= */ true);
  }
}

static void DCheckVerifyDexFile(const art::DexFile& dex) {
  if (art::kIsDebugBuild) {
    std::string error;
    if (!art::DexFileVerifier::Verify(&dex,
                                      dex.Begin(),
                                      dex.Size(),
                                      "FixedUpDexFile_Verification.dex",
                                      /*verify_checksum=*/ true,
                                      &error)) {
      LOG(FATAL) << "Failed to verify de-quickened dex file: " << error;
    }
  }
}

std::unique_ptr<FixedUpDexFile> FixedUpDexFile::Create(const art::DexFile& original,
                                                       const char* descriptor) {
  // Copy the data into mutable memory.
  std::vector<unsigned char> data;
  std::unique_ptr<const art::DexFile> new_dex_file;
  std::string error;

  // Do not use ArtDexFileLoader here. This code runs in a signal handler and
  // its stack is too small to invoke the required LocationIsOnSystemFramework
  // (b/76429651). Instead, we use DexFileLoader and copy the IsPlatformDexFile
  // property from `original` to `new_dex_file`.
  const art::DexFileLoader dex_file_loader;

  if (original.IsCompactDexFile() || original.HasHiddenapiClassData()) {
    // Since we are supposed to return a standard dex, convert back using dexlayout. It's OK to do
    // this before unquickening.
    // We also do dex layout for dex files that have hidden API data, as we want to remove that
    // data.
    art::Options options;
    options.compact_dex_level_ = art::CompactDexLevel::kCompactDexLevelNone;
    // Add a filter to only include the class that has the matching descriptor.
    static constexpr bool kFilterByDescriptor = true;
    if (kFilterByDescriptor) {
      options.class_filter_.insert(descriptor);
    }
    art::DexLayout dex_layout(options,
                              /*info=*/ nullptr,
                              /*out_file=*/ nullptr,
                              /*header=*/ nullptr);
    std::unique_ptr<art::DexContainer> dex_container;
    bool result = dex_layout.ProcessDexFile(
        original.GetLocation().c_str(),
        &original,
        0,
        &dex_container,
        &error);
    CHECK(result) << "Failed to generate dex file " << error;
    art::DexContainer::Section* main_section = dex_container->GetMainSection();
    CHECK_EQ(dex_container->GetDataSection()->Size(), 0u);
    data.insert(data.end(), main_section->Begin(), main_section->End());
  } else {
    data.resize(original.Size());
    memcpy(data.data(), original.Begin(), original.Size());
  }

  // Open the dex file in the buffer.
  new_dex_file = dex_file_loader.Open(
      data.data(),
      data.size(),
      /*location=*/"Unquickening_dexfile.dex",
      /*location_checksum=*/0,
      /*oat_dex_file=*/nullptr,
      /*verify=*/false,
      /*verify_checksum=*/false,
      &error);

  if (new_dex_file == nullptr) {
    LOG(ERROR) << "Unable to open dex file from memory for unquickening! error: " << error;
    return nullptr;
  }

  new_dex_file->SetHiddenapiDomain(original.GetHiddenapiDomain());

  DoDexUnquicken(*new_dex_file, original);

  RecomputeDexChecksum(const_cast<art::DexFile*>(new_dex_file.get()));
  DCheckVerifyDexFile(*new_dex_file);
  std::unique_ptr<FixedUpDexFile> ret(new FixedUpDexFile(std::move(new_dex_file), std::move(data)));
  return ret;
}

}  // namespace openjdkjvmti
