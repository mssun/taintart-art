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
#include "debug/elf_compilation_unit.h"
#include "debug/elf_debug_frame_writer.h"
#include "debug/elf_debug_info_writer.h"
#include "debug/elf_debug_line_writer.h"
#include "debug/elf_debug_loc_writer.h"
#include "debug/elf_symtab_writer.h"
#include "debug/method_debug_info.h"
#include "dwarf/dwarf_constants.h"
#include "elf/elf_builder.h"
#include "elf/elf_debug_reader.h"
#include "elf/elf_utils.h"
#include "elf/xz_utils.h"
#include "oat.h"
#include "stream/vector_output_stream.h"

namespace art {
namespace debug {

using ElfRuntimeTypes = std::conditional<sizeof(void*) == 4, ElfTypes32, ElfTypes64>::type;

template <typename ElfTypes>
void WriteDebugInfo(ElfBuilder<ElfTypes>* builder,
                    const DebugInfo& debug_info) {
  // Write .strtab and .symtab.
  WriteDebugSymbols(builder, /* mini-debug-info= */ false, debug_info);

  // Write .debug_frame.
  WriteCFISection(builder, debug_info.compiled_methods);

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
    line_writer.End();
  }

  // Write .debug_info section.
  if (!compilation_units.empty()) {
    ElfDebugInfoWriter<ElfTypes> info_writer(builder);
    info_writer.Start();
    for (const auto& compilation_unit : compilation_units) {
      ElfCompilationUnitWriter<ElfTypes> cu_writer(&info_writer);
      cu_writer.Write(compilation_unit);
    }
    info_writer.End();
  }
}

template <typename ElfTypes>
static std::vector<uint8_t> MakeMiniDebugInfoInternal(
    InstructionSet isa,
    const InstructionSetFeatures* features ATTRIBUTE_UNUSED,
    typename ElfTypes::Addr text_section_address,
    size_t text_section_size,
    typename ElfTypes::Addr dex_section_address,
    size_t dex_section_size,
    const DebugInfo& debug_info) {
  std::vector<uint8_t> buffer;
  buffer.reserve(KB);
  VectorOutputStream out("Mini-debug-info ELF file", &buffer);
  std::unique_ptr<ElfBuilder<ElfTypes>> builder(new ElfBuilder<ElfTypes>(isa, &out));
  builder->Start(/* write_program_headers= */ false);
  // Mirror ELF sections as NOBITS since the added symbols will reference them.
  if (text_section_size != 0) {
    builder->GetText()->AllocateVirtualMemory(text_section_address, text_section_size);
  }
  if (dex_section_size != 0) {
    builder->GetDex()->AllocateVirtualMemory(dex_section_address, dex_section_size);
  }
  if (!debug_info.Empty()) {
    WriteDebugSymbols(builder.get(), /* mini-debug-info= */ true, debug_info);
  }
  if (!debug_info.compiled_methods.empty()) {
    WriteCFISection(builder.get(), debug_info.compiled_methods);
  }
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
    const InstructionSetFeatures* features ATTRIBUTE_UNUSED,
    bool mini_debug_info,
    const MethodDebugInfo& method_info) {
  using ElfTypes = ElfRuntimeTypes;
  CHECK_EQ(sizeof(ElfTypes::Addr), static_cast<size_t>(GetInstructionSetPointerSize(isa)));
  CHECK_EQ(method_info.is_code_address_text_relative, false);
  DebugInfo debug_info{};
  debug_info.compiled_methods = ArrayRef<const MethodDebugInfo>(&method_info, 1);
  std::vector<uint8_t> buffer;
  buffer.reserve(KB);
  VectorOutputStream out("Debug ELF file", &buffer);
  std::unique_ptr<ElfBuilder<ElfTypes>> builder(new ElfBuilder<ElfTypes>(isa, &out));
  // No program headers since the ELF file is not linked and has no allocated sections.
  builder->Start(/* write_program_headers= */ false);
  builder->GetText()->AllocateVirtualMemory(method_info.code_address, method_info.code_size);
  if (mini_debug_info) {
    // The compression is great help for multiple methods but it is not worth it for a
    // single method due to the overheads so skip the compression here for performance.
    WriteDebugSymbols(builder.get(), /* mini-debug-info= */ true, debug_info);
    WriteCFISection(builder.get(), debug_info.compiled_methods);
  } else {
    WriteDebugInfo(builder.get(), debug_info);
  }
  builder->End();
  CHECK(builder->Good());
  // Verify the ELF file by reading it back using the trivial reader.
  if (kIsDebugBuild) {
    using Elf_Sym = typename ElfTypes::Sym;
    size_t num_syms = 0;
    size_t num_cies = 0;
    size_t num_fdes = 0;
    using Reader = ElfDebugReader<ElfTypes>;
    Reader reader(buffer);
    reader.VisitFunctionSymbols([&](Elf_Sym sym, const char*) {
      DCHECK_EQ(sym.st_value, method_info.code_address + CompiledMethod::CodeDelta(isa));
      DCHECK_EQ(sym.st_size, method_info.code_size);
      num_syms++;
    });
    reader.VisitDebugFrame([&](const Reader::CIE* cie ATTRIBUTE_UNUSED) {
      num_cies++;
    }, [&](const Reader::FDE* fde, const Reader::CIE* cie ATTRIBUTE_UNUSED) {
      DCHECK_EQ(fde->sym_addr, method_info.code_address);
      DCHECK_EQ(fde->sym_size, method_info.code_size);
      num_fdes++;
    });
    DCHECK_EQ(num_syms, 1u);
    DCHECK_LE(num_cies, 1u);
    DCHECK_LE(num_fdes, 1u);
  }
  return buffer;
}

// Combine several mini-debug-info ELF files into one, while filtering some symbols.
std::vector<uint8_t> PackElfFileForJIT(
    InstructionSet isa,
    const InstructionSetFeatures* features ATTRIBUTE_UNUSED,
    std::vector<ArrayRef<const uint8_t>>& added_elf_files,
    std::vector<const void*>& removed_symbols,
    /*out*/ size_t* num_symbols) {
  using ElfTypes = ElfRuntimeTypes;
  using Elf_Addr = typename ElfTypes::Addr;
  using Elf_Sym = typename ElfTypes::Sym;
  CHECK_EQ(sizeof(Elf_Addr), static_cast<size_t>(GetInstructionSetPointerSize(isa)));
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
    VectorOutputStream out("Mini-debug-info ELF file for JIT", &inner_elf_file);
    std::unique_ptr<ElfBuilder<ElfTypes>> builder(new ElfBuilder<ElfTypes>(isa, &out));
    builder->Start(/*write_program_headers=*/ false);
    auto* text = builder->GetText();
    auto* strtab = builder->GetStrTab();
    auto* symtab = builder->GetSymTab();
    auto* debug_frame = builder->GetDebugFrame();
    std::deque<Elf_Sym> symbols;

    using Reader = ElfDebugReader<ElfTypes>;
    std::deque<Reader> readers;
    for (ArrayRef<const uint8_t> added_elf_file : added_elf_files) {
      readers.emplace_back(added_elf_file);
    }

    // Write symbols names. All other data is buffered.
    strtab->Start();
    strtab->Write("");  // strtab should start with empty string.
    for (Reader& reader : readers) {
      reader.VisitFunctionSymbols([&](Elf_Sym sym, const char* name) {
          if (is_removed_symbol(sym.st_value)) {
            return;
          }
          sym.st_name = strtab->Write(name);
          symbols.push_back(sym);
          min_address = std::min<uint64_t>(min_address, sym.st_value);
          max_address = std::max<uint64_t>(max_address, sym.st_value + sym.st_size);
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
    // ART always produces the same CIE, so we copy the first one and ignore the rest.
    bool copied_cie = false;
    for (Reader& reader : readers) {
      reader.VisitDebugFrame([&](const Reader::CIE* cie) {
        if (!copied_cie) {
          debug_frame->WriteFully(cie->data(), cie->size());
          copied_cie = true;
        }
      }, [&](const Reader::FDE* fde, const Reader::CIE* cie ATTRIBUTE_UNUSED) {
        DCHECK(copied_cie);
        DCHECK_EQ(fde->cie_pointer, 0);
        if (!is_removed_symbol(fde->sym_addr)) {
          debug_frame->WriteFully(fde->data(), fde->size());
        }
      });
    }
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
    VectorOutputStream out("Mini-debug-info ELF file for JIT", &outer_elf_file);
    std::unique_ptr<ElfBuilder<ElfTypes>> builder(new ElfBuilder<ElfTypes>(isa, &out));
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
    const InstructionSetFeatures* features ATTRIBUTE_UNUSED,
    const ArrayRef<mirror::Class*>& types)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  using ElfTypes = ElfRuntimeTypes;
  CHECK_EQ(sizeof(ElfTypes::Addr), static_cast<size_t>(GetInstructionSetPointerSize(isa)));
  std::vector<uint8_t> buffer;
  buffer.reserve(KB);
  VectorOutputStream out("Debug ELF file", &buffer);
  std::unique_ptr<ElfBuilder<ElfTypes>> builder(new ElfBuilder<ElfTypes>(isa, &out));
  // No program headers since the ELF file is not linked and has no allocated sections.
  builder->Start(/* write_program_headers= */ false);
  ElfDebugInfoWriter<ElfTypes> info_writer(builder.get());
  info_writer.Start();
  ElfCompilationUnitWriter<ElfTypes> cu_writer(&info_writer);
  cu_writer.Write(types);
  info_writer.End();

  builder->End();
  CHECK(builder->Good());
  return buffer;
}

// Explicit instantiations
template void WriteDebugInfo<ElfTypes32>(
    ElfBuilder<ElfTypes32>* builder,
    const DebugInfo& debug_info);
template void WriteDebugInfo<ElfTypes64>(
    ElfBuilder<ElfTypes64>* builder,
    const DebugInfo& debug_info);

}  // namespace debug
}  // namespace art
