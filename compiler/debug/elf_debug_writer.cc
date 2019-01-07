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
 */

#include "elf_debug_writer.h"

#include <type_traits>
#include <unordered_map>
#include <vector>

#include "base/array_ref.h"
#include "base/stl_util.h"
#include "debug/dwarf/dwarf_constants.h"
#include "debug/elf_compilation_unit.h"
#include "debug/elf_debug_frame_writer.h"
#include "debug/elf_debug_info_writer.h"
#include "debug/elf_debug_line_writer.h"
#include "debug/elf_debug_loc_writer.h"
#include "debug/elf_debug_reader.h"
#include "debug/elf_symtab_writer.h"
#include "debug/method_debug_info.h"
#include "debug/xz_utils.h"
#include "elf.h"
#include "linker/elf_builder.h"
#include "linker/vector_output_stream.h"
#include "oat.h"

namespace art {
namespace debug {

using ElfRuntimeTypes = std::conditional<sizeof(void*) == 4, ElfTypes32, ElfTypes64>::type;

template <typename ElfTypes>
void WriteDebugInfo(linker::ElfBuilder<ElfTypes>* builder,
                    const DebugInfo& debug_info,
                    dwarf::CFIFormat cfi_format,
                    bool write_oat_patches) {
  // Write .strtab and .symtab.
  WriteDebugSymbols(builder, /* mini-debug-info= */ false, debug_info);

  // Write .debug_frame.
  WriteCFISection(builder, debug_info.compiled_methods, cfi_format, write_oat_patches);

  // Group the methods into compilation units based on class.
  std::unordered_map<const dex::ClassDef*, ElfCompilationUnit> class_to_compilation_unit;
  for (const MethodDebugInfo& mi : debug_info.compiled_methods) {
    if (mi.dex_file != nullptr) {
      auto& dex_class_def = mi.dex_file->GetClassDef(mi.class_def_index);
      ElfCompilationUnit& cu = class_to_compilation_unit[&dex_class_def];
      cu.methods.push_back(&mi);
      // All methods must have the same addressing mode otherwise the min/max below does not work.
      DCHECK_EQ(cu.methods.front()->is_code_address_text_relative, mi.is_code_address_text_relative);
      cu.is_code_address_text_relative = mi.is_code_address_text_relative;
      cu.code_address = std::min(cu.code_address, mi.code_address);
      cu.code_end = std::max(cu.code_end, mi.code_address + mi.code_size);
    }
  }

  // Sort compilation units to make the compiler output deterministic.
  std::vector<ElfCompilationUnit> compilation_units;
  compilation_units.reserve(class_to_compilation_unit.size());
  for (auto& it : class_to_compilation_unit) {
    // The .debug_line section requires the methods to be sorted by code address.
    std::stable_sort(it.second.methods.begin(),
                     it.second.methods.end(),
                     [](const MethodDebugInfo* a, const MethodDebugInfo* b) {
                         return a->code_address < b->code_address;
                     });
    compilation_units.push_back(std::move(it.second));
  }
  std::sort(compilation_units.begin(),
            compilation_units.end(),
            [](ElfCompilationUnit& a, ElfCompilationUnit& b) {
                // Sort by index of the first method within the method_infos array.
                // This assumes that the order of method_infos is deterministic.
                // Code address is not good for sorting due to possible duplicates.
                return a.methods.front() < b.methods.front();
            });

  // Write .debug_line section.
  if (!compilation_units.empty()) {
    ElfDebugLineWriter<ElfTypes> line_writer(builder);
    line_writer.Start();
    for (auto& compilation_unit : compilation_units) {
      line_writer.WriteCompilationUnit(compilation_unit);
    }
    line_writer.End(write_oat_patches);
  }

  // Write .debug_info section.
  if (!compilation_units.empty()) {
    ElfDebugInfoWriter<ElfTypes> info_writer(builder);
    info_writer.Start();
    for (const auto& compilation_unit : compilation_units) {
      ElfCompilationUnitWriter<ElfTypes> cu_writer(&info_writer);
      cu_writer.Write(compilation_unit);
    }
    info_writer.End(write_oat_patches);
  }
}

template <typename ElfTypes>
static std::vector<uint8_t> MakeMiniDebugInfoInternal(
    InstructionSet isa,
    const InstructionSetFeatures* features,
    typename ElfTypes::Addr text_section_address,
    size_t text_section_size,
    typename ElfTypes::Addr dex_section_address,
    size_t dex_section_size,
    const DebugInfo& debug_info) {
  std::vector<uint8_t> buffer;
  buffer.reserve(KB);
  linker::VectorOutputStream out("Mini-debug-info ELF file", &buffer);
  std::unique_ptr<linker::ElfBuilder<ElfTypes>> builder(
      new linker::ElfBuilder<ElfTypes>(isa, features, &out));
  builder->Start(/* write_program_headers= */ false);
  // Mirror ELF sections as NOBITS since the added symbols will reference them.
  builder->GetText()->AllocateVirtualMemory(text_section_address, text_section_size);
  if (dex_section_size != 0) {
    builder->GetDex()->AllocateVirtualMemory(dex_section_address, dex_section_size);
  }
  WriteDebugSymbols(builder.get(), /* mini-debug-info= */ true, debug_info);
  WriteCFISection(builder.get(),
                  debug_info.compiled_methods,
                  dwarf::DW_DEBUG_FRAME_FORMAT,
                  /* write_oat_patches= */ false);
  builder->End();
  CHECK(builder->Good());
  std::vector<uint8_t> compressed_buffer;
  compressed_buffer.reserve(buffer.size() / 4);
  XzCompress(ArrayRef<const uint8_t>(buffer), &compressed_buffer);
  return compressed_buffer;
}

std::vector<uint8_t> MakeMiniDebugInfo(
    InstructionSet isa,
    const InstructionSetFeatures* features,
    uint64_t text_section_address,
    size_t text_section_size,
    uint64_t dex_section_address,
    size_t dex_section_size,
    const DebugInfo& debug_info) {
  if (Is64BitInstructionSet(isa)) {
    return MakeMiniDebugInfoInternal<ElfTypes64>(isa,
                                                 features,
                                                 text_section_address,
                                                 text_section_size,
                                                 dex_section_address,
                                                 dex_section_size,
                                                 debug_info);
  } else {
    return MakeMiniDebugInfoInternal<ElfTypes32>(isa,
                                                 features,
                                                 text_section_address,
                                                 text_section_size,
                                                 dex_section_address,
                                                 dex_section_size,
                                                 debug_info);
  }
}

std::vector<uint8_t> MakeElfFileForJIT(
    InstructionSet isa,
    const InstructionSetFeatures* features,
    bool mini_debug_info,
    const MethodDebugInfo& method_info) {
  using ElfTypes = ElfRuntimeTypes;
  CHECK_EQ(sizeof(ElfTypes::Addr), static_cast<size_t>(GetInstructionSetPointerSize(isa)));
  CHECK_EQ(method_info.is_code_address_text_relative, false);
  DebugInfo debug_info{};
  debug_info.compiled_methods = ArrayRef<const MethodDebugInfo>(&method_info, 1);
  std::vector<uint8_t> buffer;
  buffer.reserve(KB);
  linker::VectorOutputStream out("Debug ELF file", &buffer);
  std::unique_ptr<linker::ElfBuilder<ElfTypes>> builder(
      new linker::ElfBuilder<ElfTypes>(isa, features, &out));
  // No program headers since the ELF file is not linked and has no allocated sections.
  builder->Start(/* write_program_headers= */ false);
  builder->GetText()->AllocateVirtualMemory(method_info.code_address, method_info.code_size);
  if (mini_debug_info) {
    // The compression is great help for multiple methods but it is not worth it for a
    // single method due to the overheads so skip the compression here for performance.
    WriteDebugSymbols(builder.get(), /* mini-debug-info= */ true, debug_info);
    WriteCFISection(builder.get(),
                    debug_info.compiled_methods,
                    dwarf::DW_DEBUG_FRAME_FORMAT,
                    /* write_oat_patches= */ false);
  } else {
    WriteDebugInfo(builder.get(),
                   debug_info,
                   dwarf::DW_DEBUG_FRAME_FORMAT,
                   /* write_oat_patches= */ false);
  }
  builder->End();
  CHECK(builder->Good());
  // Verify the ELF file by reading it back using the trivial reader.
  if (kIsDebugBuild) {
    using Elf_Sym = typename ElfTypes::Sym;
    using Elf_Addr = typename ElfTypes::Addr;
    size_t num_syms = 0;
    size_t num_cfis = 0;
    ReadElfSymbols<ElfTypes>(
        buffer.data(),
        [&](Elf_Sym sym, const char*) {
          DCHECK_EQ(sym.st_value, method_info.code_address + CompiledMethod::CodeDelta(isa));
          DCHECK_EQ(sym.st_size, method_info.code_size);
          num_syms++;
        },
        [&](Elf_Addr addr, Elf_Addr size, ArrayRef<const uint8_t> opcodes) {
          DCHECK_EQ(addr, method_info.code_address);
          DCHECK_EQ(size, method_info.code_size);
          DCHECK_GE(opcodes.size(), method_info.cfi.size());
          DCHECK_EQ(memcmp(opcodes.data(), method_info.cfi.data(), method_info.cfi.size()), 0);
          num_cfis++;
        });
    DCHECK_EQ(num_syms, 1u);
    // CFI might be missing. TODO: Ensure we have CFI for all methods.
    DCHECK_LE(num_cfis, 1u);
  }
  return buffer;
}

// Combine several mini-debug-info ELF files into one, while filtering some symbols.
std::vector<uint8_t> PackElfFileForJIT(
    InstructionSet isa,
    const InstructionSetFeatures* features,
    std::vector<const uint8_t*>& added_elf_files,
    std::vector<const void*>& removed_symbols,
    /*out*/ size_t* num_symbols) {
  using ElfTypes = ElfRuntimeTypes;
  using Elf_Addr = typename ElfTypes::Addr;
  using Elf_Sym = typename ElfTypes::Sym;
  CHECK_EQ(sizeof(Elf_Addr), static_cast<size_t>(GetInstructionSetPointerSize(isa)));
  const bool is64bit = Is64BitInstructionSet(isa);
  auto is_removed_symbol = [&removed_symbols](Elf_Addr addr) {
    const void* code_ptr = reinterpret_cast<const void*>(addr);
    return std::binary_search(removed_symbols.begin(), removed_symbols.end(), code_ptr);
  };
  uint64_t min_address = std::numeric_limits<uint64_t>::max();
  uint64_t max_address = 0;

  // Produce the inner ELF file.
  // It will contain the symbols (.symtab) and unwind information (.debug_frame).
  std::vector<uint8_t> inner_elf_file;
  {
    inner_elf_file.reserve(1 * KB);  // Approximate size of ELF file with a single symbol.
    linker::VectorOutputStream out("Mini-debug-info ELF file for JIT", &inner_elf_file);
    std::unique_ptr<linker::ElfBuilder<ElfTypes>> builder(
        new linker::ElfBuilder<ElfTypes>(isa, features, &out));
    builder->Start(/*write_program_headers=*/ false);
    auto* text = builder->GetText();
    auto* strtab = builder->GetStrTab();
    auto* symtab = builder->GetSymTab();
    auto* debug_frame = builder->GetDebugFrame();
    std::deque<Elf_Sym> symbols;
    std::vector<uint8_t> debug_frame_buffer;
    WriteCIE(isa, dwarf::DW_DEBUG_FRAME_FORMAT, &debug_frame_buffer);

    // Write symbols names. All other data is buffered.
    strtab->Start();
    strtab->Write("");  // strtab should start with empty string.
    for (const uint8_t* added_elf_file : added_elf_files) {
      ReadElfSymbols<ElfTypes>(
          added_elf_file,
          [&](Elf_Sym sym, const char* name) {
              if (is_removed_symbol(sym.st_value)) {
                return;
              }
              sym.st_name = strtab->Write(name);
              symbols.push_back(sym);
              min_address = std::min<uint64_t>(min_address, sym.st_value);
              max_address = std::max<uint64_t>(max_address, sym.st_value + sym.st_size);
          },
          [&](Elf_Addr addr, Elf_Addr size, ArrayRef<const uint8_t> opcodes) {
              if (is_removed_symbol(addr)) {
                return;
              }
              WriteFDE(is64bit,
                       /*section_address=*/ 0,
                       /*cie_address=*/ 0,
                       addr,
                       size,
                       opcodes,
                       dwarf::DW_DEBUG_FRAME_FORMAT,
                       debug_frame_buffer.size(),
                       &debug_frame_buffer,
                       /*patch_locations=*/ nullptr);
          });
    }
    strtab->End();

    // Create .text covering the code range. Needed for gdb to find the symbols.
    if (max_address > min_address) {
      text->AllocateVirtualMemory(min_address, max_address - min_address);
    }

    // Add the symbols.
    *num_symbols = symbols.size();
    for (; !symbols.empty(); symbols.pop_front()) {
      symtab->Add(symbols.front(), text);
    }
    symtab->WriteCachedSection();

    // Add the CFI/unwind section.
    debug_frame->Start();
    debug_frame->WriteFully(debug_frame_buffer.data(), debug_frame_buffer.size());
    debug_frame->End();

    builder->End();
    CHECK(builder->Good());
  }

  // Produce the outer ELF file.
  // It contains only the inner ELF file compressed as .gnu_debugdata section.
  // This extra wrapping is not necessary but the compression saves space.
  std::vector<uint8_t> outer_elf_file;
  {
    std::vector<uint8_t> gnu_debugdata;
    gnu_debugdata.reserve(inner_elf_file.size() / 4);
    XzCompress(ArrayRef<const uint8_t>(inner_elf_file), &gnu_debugdata);

    outer_elf_file.reserve(KB + gnu_debugdata.size());
    linker::VectorOutputStream out("Mini-debug-info ELF file for JIT", &outer_elf_file);
    std::unique_ptr<linker::ElfBuilder<ElfTypes>> builder(
        new linker::ElfBuilder<ElfTypes>(isa, features, &out));
    builder->Start(/*write_program_headers=*/ false);
    if (max_address > min_address) {
      builder->GetText()->AllocateVirtualMemory(min_address, max_address - min_address);
    }
    builder->WriteSection(".gnu_debugdata", &gnu_debugdata);
    builder->End();
    CHECK(builder->Good());
  }

  return outer_elf_file;
}

std::vector<uint8_t> WriteDebugElfFileForClasses(
    InstructionSet isa,
    const InstructionSetFeatures* features,
    const ArrayRef<mirror::Class*>& types)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  using ElfTypes = ElfRuntimeTypes;
  CHECK_EQ(sizeof(ElfTypes::Addr), static_cast<size_t>(GetInstructionSetPointerSize(isa)));
  std::vector<uint8_t> buffer;
  buffer.reserve(KB);
  linker::VectorOutputStream out("Debug ELF file", &buffer);
  std::unique_ptr<linker::ElfBuilder<ElfTypes>> builder(
      new linker::ElfBuilder<ElfTypes>(isa, features, &out));
  // No program headers since the ELF file is not linked and has no allocated sections.
  builder->Start(/* write_program_headers= */ false);
  ElfDebugInfoWriter<ElfTypes> info_writer(builder.get());
  info_writer.Start();
  ElfCompilationUnitWriter<ElfTypes> cu_writer(&info_writer);
  cu_writer.Write(types);
  info_writer.End(/* write_oat_patches= */ false);

  builder->End();
  CHECK(builder->Good());
  return buffer;
}

// Explicit instantiations
template void WriteDebugInfo<ElfTypes32>(
    linker::ElfBuilder<ElfTypes32>* builder,
    const DebugInfo& debug_info,
    dwarf::CFIFormat cfi_format,
    bool write_oat_patches);
template void WriteDebugInfo<ElfTypes64>(
    linker::ElfBuilder<ElfTypes64>* builder,
    const DebugInfo& debug_info,
    dwarf::CFIFormat cfi_format,
    bool write_oat_patches);

}  // namespace debug
}  // namespace art
