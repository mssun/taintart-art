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

#include <sys/mman.h>  // For the PROT_NONE constant.

#include "elf_file.h"

#include "base/file_utils.h"
#include "base/mem_map.h"
#include "base/unix_file/fd_file.h"
#include "base/utils.h"
#include "common_compiler_driver_test.h"
#include "elf/elf_builder.h"
#include "elf_file.h"
#include "elf_file_impl.h"
#include "elf_writer_quick.h"
#include "oat.h"

namespace art {
namespace linker {

class ElfWriterTest : public CommonCompilerDriverTest {
 protected:
  void SetUp() override {
    ReserveImageSpace();
    CommonCompilerTest::SetUp();
  }
};

#define EXPECT_ELF_FILE_ADDRESS(ef, expected_value, symbol_name, build_map) \
  do { \
    void* addr = reinterpret_cast<void*>((ef)->FindSymbolAddress(SHT_DYNSYM, \
                                                                 symbol_name, \
                                                                 build_map)); \
    EXPECT_NE(nullptr, addr); \
    if ((expected_value) == nullptr) { \
      (expected_value) = addr; \
    }                        \
    EXPECT_EQ(expected_value, addr); \
    EXPECT_EQ(expected_value, (ef)->FindDynamicSymbolAddress(symbol_name)); \
  } while (false)

TEST_F(ElfWriterTest, dlsym) {
  std::string elf_location = GetCoreOatLocation();
  std::string elf_filename = GetSystemImageFilename(elf_location.c_str(), kRuntimeISA);
  LOG(INFO) << "elf_filename=" << elf_filename;

  UnreserveImageSpace();
  void* dl_oatdata = nullptr;
  void* dl_oatexec = nullptr;
  void* dl_oatlastword = nullptr;

  std::unique_ptr<File> file(OS::OpenFileForReading(elf_filename.c_str()));
  ASSERT_TRUE(file.get() != nullptr) << elf_filename;
  {
    std::string error_msg;
    std::unique_ptr<ElfFile> ef(ElfFile::Open(file.get(),
                                              /*writable=*/ false,
                                              /*program_header_only=*/ false,
                                              /*low_4gb=*/false,
                                              &error_msg));
    CHECK(ef.get() != nullptr) << error_msg;
    EXPECT_ELF_FILE_ADDRESS(ef, dl_oatdata, "oatdata", false);
    EXPECT_ELF_FILE_ADDRESS(ef, dl_oatexec, "oatexec", false);
    EXPECT_ELF_FILE_ADDRESS(ef, dl_oatlastword, "oatlastword", false);
  }
  {
    std::string error_msg;
    std::unique_ptr<ElfFile> ef(ElfFile::Open(file.get(),
                                              /*writable=*/ false,
                                              /*program_header_only=*/ false,
                                              /*low_4gb=*/ false,
                                              &error_msg));
    CHECK(ef.get() != nullptr) << error_msg;
    EXPECT_ELF_FILE_ADDRESS(ef, dl_oatdata, "oatdata", true);
    EXPECT_ELF_FILE_ADDRESS(ef, dl_oatexec, "oatexec", true);
    EXPECT_ELF_FILE_ADDRESS(ef, dl_oatlastword, "oatlastword", true);
  }
  {
    std::string error_msg;
    std::unique_ptr<ElfFile> ef(ElfFile::Open(file.get(),
                                              /*writable=*/ false,
                                              /*program_header_only=*/ true,
                                              /*low_4gb=*/ false,
                                              &error_msg));
    CHECK(ef.get() != nullptr) << error_msg;
    size_t size;
    bool success = ef->GetLoadedSize(&size, &error_msg);
    CHECK(success) << error_msg;
    MemMap reservation = MemMap::MapAnonymous("ElfWriterTest#dlsym reservation",
                                              RoundUp(size, kPageSize),
                                              PROT_NONE,
                                              /*low_4gb=*/ true,
                                              &error_msg);
    CHECK(reservation.IsValid()) << error_msg;
    uint8_t* base = reservation.Begin();
    success =
        ef->Load(file.get(), /*executable=*/ false, /*low_4gb=*/ false, &reservation, &error_msg);
    CHECK(success) << error_msg;
    CHECK(!reservation.IsValid());
    EXPECT_EQ(reinterpret_cast<uintptr_t>(dl_oatdata) + reinterpret_cast<uintptr_t>(base),
        reinterpret_cast<uintptr_t>(ef->FindDynamicSymbolAddress("oatdata")));
    EXPECT_EQ(reinterpret_cast<uintptr_t>(dl_oatexec) + reinterpret_cast<uintptr_t>(base),
        reinterpret_cast<uintptr_t>(ef->FindDynamicSymbolAddress("oatexec")));
    EXPECT_EQ(reinterpret_cast<uintptr_t>(dl_oatlastword) + reinterpret_cast<uintptr_t>(base),
        reinterpret_cast<uintptr_t>(ef->FindDynamicSymbolAddress("oatlastword")));
  }
}

TEST_F(ElfWriterTest, CheckBuildIdPresent) {
  std::string elf_location = GetCoreOatLocation();
  std::string elf_filename = GetSystemImageFilename(elf_location.c_str(), kRuntimeISA);
  LOG(INFO) << "elf_filename=" << elf_filename;

  std::unique_ptr<File> file(OS::OpenFileForReading(elf_filename.c_str()));
  ASSERT_TRUE(file.get() != nullptr);
  {
    std::string error_msg;
    std::unique_ptr<ElfFile> ef(ElfFile::Open(file.get(),
                                              /*writable=*/ false,
                                              /*program_header_only=*/ false,
                                              /*low_4gb=*/ false,
                                              &error_msg));
    CHECK(ef.get() != nullptr) << error_msg;
    EXPECT_TRUE(ef->HasSection(".note.gnu.build-id"));
  }
}

}  // namespace linker
}  // namespace art
