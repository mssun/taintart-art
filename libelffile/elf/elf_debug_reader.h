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

#ifndef ART_LIBELFFILE_ELF_ELF_DEBUG_READER_H_
#define ART_LIBELFFILE_ELF_ELF_DEBUG_READER_H_

#include "base/array_ref.h"
#include "dwarf/headers.h"
#include "elf/elf_utils.h"
#include "xz_utils.h"

#include <map>
#include <string_view>

namespace art {

// Trivial ELF file reader.
//
// It is the bare minimum needed to read mini-debug-info symbols for unwinding.
// We use it to merge JIT mini-debug-infos together or to prune them after GC.
template <typename ElfTypes>
class ElfDebugReader {
 public:
  // Note that the input buffer might be misaligned.
  typedef typename ElfTypes::Ehdr ALIGNED(1) Elf_Ehdr;
  typedef typename ElfTypes::Shdr ALIGNED(1) Elf_Shdr;
  typedef typename ElfTypes::Sym ALIGNED(1) Elf_Sym;
  typedef typename ElfTypes::Addr ALIGNED(1) Elf_Addr;

  // Call Frame Information.
  struct CFI {
    uint32_t length;  // Length excluding the size of this field.
    int32_t cie_pointer;  // Offset in the section or -1 for CIE.

    const uint8_t* data() const { return reinterpret_cast<const uint8_t*>(this); }
    size_t size() const { return sizeof(uint32_t) + length; }
  } PACKED(1);

  // Common Information Entry.
  struct CIE : public CFI {
  } PACKED(1);

  // Frame Description Entry.
  struct FDE : public CFI {
    Elf_Addr sym_addr;
    Elf_Addr sym_size;
  } PACKED(1);

  explicit ElfDebugReader(ArrayRef<const uint8_t> file) : file_(file) {
    header_ = Read<Elf_Ehdr>(/*offset=*/ 0);
    CHECK_EQ(header_->e_ident[0], ELFMAG0);
    CHECK_EQ(header_->e_ident[1], ELFMAG1);
    CHECK_EQ(header_->e_ident[2], ELFMAG2);
    CHECK_EQ(header_->e_ident[3], ELFMAG3);
    CHECK_EQ(header_->e_ehsize, sizeof(Elf_Ehdr));
    CHECK_EQ(header_->e_shentsize, sizeof(Elf_Shdr));

    // Find all ELF sections.
    sections_ = Read<Elf_Shdr>(header_->e_shoff, header_->e_shnum);
    for (const Elf_Shdr& section : sections_) {
      const char* name = Read<char>(sections_[header_->e_shstrndx].sh_offset + section.sh_name);
      section_map_[std::string_view(name)] = &section;
    }

    // Decompressed embedded debug symbols, if any.
    const Elf_Shdr* gnu_debugdata = section_map_[".gnu_debugdata"];
    if (gnu_debugdata != nullptr) {
      auto compressed = Read<uint8_t>(gnu_debugdata->sh_offset, gnu_debugdata->sh_size);
      XzDecompress(compressed, &decompressed_gnu_debugdata_);
      gnu_debugdata_reader_.reset(new ElfDebugReader(decompressed_gnu_debugdata_));
    }
  }

  explicit ElfDebugReader(std::vector<uint8_t>& file)
      : ElfDebugReader(ArrayRef<const uint8_t>(file)) {
  }

  const Elf_Ehdr* GetHeader() { return header_; }

  ArrayRef<Elf_Shdr> GetSections() { return sections_; }

  const Elf_Shdr* GetSection(const char* name) { return section_map_[name]; }

  template <typename VisitSym>
  void VisitFunctionSymbols(VisitSym visit_sym) {
    const Elf_Shdr* symtab = GetSection(".symtab");
    const Elf_Shdr* strtab = GetSection(".strtab");
    const Elf_Shdr* text = GetSection(".text");
    if (symtab != nullptr && strtab != nullptr) {
      CHECK_EQ(symtab->sh_entsize, sizeof(Elf_Sym));
      size_t count = symtab->sh_size / sizeof(Elf_Sym);
      for (const Elf_Sym& symbol : Read<Elf_Sym>(symtab->sh_offset, count)) {
        if (ELF_ST_TYPE(symbol.st_info) == STT_FUNC && &sections_[symbol.st_shndx] == text) {
          visit_sym(symbol, Read<char>(strtab->sh_offset + symbol.st_name));
        }
      }
    }
    if (gnu_debugdata_reader_ != nullptr) {
      gnu_debugdata_reader_->VisitFunctionSymbols(visit_sym);
    }
  }

  template <typename VisitSym>
  void VisitDynamicSymbols(VisitSym visit_sym) {
    const Elf_Shdr* dynsym = GetSection(".dynsym");
    const Elf_Shdr* dynstr = GetSection(".dynstr");
    if (dynsym != nullptr && dynstr != nullptr) {
      CHECK_EQ(dynsym->sh_entsize, sizeof(Elf_Sym));
      size_t count = dynsym->sh_size / sizeof(Elf_Sym);
      for (const Elf_Sym& symbol : Read<Elf_Sym>(dynsym->sh_offset, count)) {
        visit_sym(symbol, Read<char>(dynstr->sh_offset + symbol.st_name));
      }
    }
  }

  template <typename VisitCIE, typename VisitFDE>
  void VisitDebugFrame(VisitCIE visit_cie, VisitFDE visit_fde) {
    const Elf_Shdr* debug_frame = GetSection(".debug_frame");
    if (debug_frame != nullptr) {
      for (size_t offset = 0; offset < debug_frame->sh_size;) {
        const CFI* entry = Read<CFI>(debug_frame->sh_offset + offset);
        DCHECK_LE(entry->size(), debug_frame->sh_size - offset);
        if (entry->cie_pointer == -1) {
          visit_cie(Read<CIE>(debug_frame->sh_offset + offset));
        } else {
          const FDE* fde = Read<FDE>(debug_frame->sh_offset + offset);
          visit_fde(fde, Read<CIE>(debug_frame->sh_offset + fde->cie_pointer));
        }
        offset += entry->size();
      }
    }
    if (gnu_debugdata_reader_ != nullptr) {
      gnu_debugdata_reader_->VisitDebugFrame(visit_cie, visit_fde);
    }
  }

 private:
  template<typename T>
  const T* Read(size_t offset) {
    DCHECK_LE(offset + sizeof(T), file_.size());
    return reinterpret_cast<const T*>(file_.data() + offset);
  }

  template<typename T>
  ArrayRef<const T> Read(size_t offset, size_t count) {
    DCHECK_LE(offset + count * sizeof(T), file_.size());
    return ArrayRef<const T>(Read<T>(offset), count);
  }

  ArrayRef<const uint8_t> const file_;
  const Elf_Ehdr* header_;
  ArrayRef<const Elf_Shdr> sections_;
  std::unordered_map<std::string_view, const Elf_Shdr*> section_map_;
  std::vector<uint8_t> decompressed_gnu_debugdata_;
  std::unique_ptr<ElfDebugReader> gnu_debugdata_reader_;

  DISALLOW_COPY_AND_ASSIGN(ElfDebugReader);
};

}  // namespace art
#endif  // ART_LIBELFFILE_ELF_ELF_DEBUG_READER_H_
