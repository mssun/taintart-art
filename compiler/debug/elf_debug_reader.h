/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef ART_COMPILER_DEBUG_ELF_DEBUG_READER_H_
#define ART_COMPILER_DEBUG_ELF_DEBUG_READER_H_

#include "base/array_ref.h"
#include "debug/dwarf/headers.h"
#include "elf.h"
#include "xz_utils.h"

namespace art {
namespace debug {

// Trivial ELF file reader.
//
// It is the bare minimum needed to read mini-debug-info symbols for unwinding.
// We use it to merge JIT mini-debug-infos together or to prune them after GC.
// The consumed ELF file comes from ART JIT.
template <typename ElfTypes, typename VisitSym, typename VisitFde>
static void ReadElfSymbols(const uint8_t* elf, VisitSym visit_sym, VisitFde visit_fde) {
  // Note that the input buffer might be misaligned.
  typedef typename ElfTypes::Ehdr ALIGNED(1) Elf_Ehdr;
  typedef typename ElfTypes::Shdr ALIGNED(1) Elf_Shdr;
  typedef typename ElfTypes::Sym ALIGNED(1) Elf_Sym;
  typedef typename ElfTypes::Addr ALIGNED(1) Elf_Addr;

  // Read and check the elf header.
  const Elf_Ehdr* header = reinterpret_cast<const Elf_Ehdr*>(elf);
  CHECK(header->checkMagic());

  // Find sections that we are interested in.
  const Elf_Shdr* sections = reinterpret_cast<const Elf_Shdr*>(elf + header->e_shoff);
  const Elf_Shdr* strtab = nullptr;
  const Elf_Shdr* symtab = nullptr;
  const Elf_Shdr* debug_frame = nullptr;
  const Elf_Shdr* gnu_debugdata = nullptr;
  for (size_t i = 1 /* skip null section */; i < header->e_shnum; i++) {
    const Elf_Shdr* section = sections + i;
    const char* name = reinterpret_cast<const char*>(
        elf + sections[header->e_shstrndx].sh_offset + section->sh_name);
    if (strcmp(name, ".strtab") == 0) {
      strtab = section;
    } else if (strcmp(name, ".symtab") == 0) {
      symtab = section;
    } else if (strcmp(name, ".debug_frame") == 0) {
      debug_frame = section;
    } else if (strcmp(name, ".gnu_debugdata") == 0) {
      gnu_debugdata = section;
    }
  }

  // Visit symbols.
  if (symtab != nullptr && strtab != nullptr) {
    const Elf_Sym* symbols = reinterpret_cast<const Elf_Sym*>(elf + symtab->sh_offset);
    DCHECK_EQ(symtab->sh_entsize, sizeof(Elf_Sym));
    size_t count = symtab->sh_size / sizeof(Elf_Sym);
    for (size_t i = 1 /* skip null symbol */; i < count; i++) {
      Elf_Sym symbol = symbols[i];
      if (symbol.getBinding() != STB_LOCAL) {  // Ignore local symbols (e.g. "$t").
        const uint8_t* name = elf + strtab->sh_offset + symbol.st_name;
        visit_sym(symbol, reinterpret_cast<const char*>(name));
      }
    }
  }

  // Visit CFI (unwind) data.
  if (debug_frame != nullptr) {
    const uint8_t* data = elf + debug_frame->sh_offset;
    const uint8_t* end = data + debug_frame->sh_size;
    while (data < end) {
      Elf_Addr addr, size;
      ArrayRef<const uint8_t> opcodes;
      if (dwarf::ReadFDE<Elf_Addr>(&data, &addr, &size, &opcodes)) {
        visit_fde(addr, size, opcodes);
      }
    }
  }

  // Process embedded compressed ELF file.
  if (gnu_debugdata != nullptr) {
    ArrayRef<const uint8_t> compressed(elf + gnu_debugdata->sh_offset, gnu_debugdata->sh_size);
    std::vector<uint8_t> decompressed;
    XzDecompress(compressed, &decompressed);
    ReadElfSymbols<ElfTypes>(decompressed.data(), visit_sym, visit_fde);
  }
}

}  // namespace debug
}  // namespace art
#endif  // ART_COMPILER_DEBUG_ELF_DEBUG_READER_H_
