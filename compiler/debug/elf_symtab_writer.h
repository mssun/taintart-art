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

#ifndef ART_COMPILER_DEBUG_ELF_SYMTAB_WRITER_H_
#define ART_COMPILER_DEBUG_ELF_SYMTAB_WRITER_H_

#include <map>
#include <unordered_set>

#include "debug/debug_info.h"
#include "debug/method_debug_info.h"
#include "dex/dex_file-inl.h"
#include "dex/code_item_accessors.h"
#include "linker/elf_builder.h"
#include "utils.h"

namespace art {
namespace debug {

// The ARM specification defines three special mapping symbols
// $a, $t and $d which mark ARM, Thumb and data ranges respectively.
// These symbols can be used by tools, for example, to pretty
// print instructions correctly.  Objdump will use them if they
// exist, but it will still work well without them.
// However, these extra symbols take space, so let's just generate
// one symbol which marks the whole .text section as code.
constexpr bool kGenerateSingleArmMappingSymbol = true;

// Magic name for .symtab symbols which enumerate dex files used
// by this ELF file (currently mmapped inside the .dex section).
constexpr const char* kDexFileSymbolName = "$dexfile";

template <typename ElfTypes>
static void WriteDebugSymbols(linker::ElfBuilder<ElfTypes>* builder,
                              bool mini_debug_info,
                              const DebugInfo& debug_info) {
  uint64_t mapping_symbol_address = std::numeric_limits<uint64_t>::max();
  auto* strtab = builder->GetStrTab();
  auto* symtab = builder->GetSymTab();

  if (debug_info.Empty()) {
    return;
  }

  // Find all addresses which contain deduped methods.
  // The first instance of method is not marked deduped_, but the rest is.
  std::unordered_set<uint64_t> deduped_addresses;
  for (const MethodDebugInfo& info : debug_info.compiled_methods) {
    if (info.deduped) {
      deduped_addresses.insert(info.code_address);
    }
  }

  strtab->Start();
  strtab->Write("");  // strtab should start with empty string.
  // Add symbols for compiled methods.
  for (const MethodDebugInfo& info : debug_info.compiled_methods) {
    if (info.deduped) {
      continue;  // Add symbol only for the first instance.
    }
    size_t name_offset;
    if (!info.trampoline_name.empty()) {
      name_offset = strtab->Write(info.trampoline_name);
    } else {
      DCHECK(info.dex_file != nullptr);
      std::string name = info.dex_file->PrettyMethod(info.dex_method_index, !mini_debug_info);
      if (deduped_addresses.find(info.code_address) != deduped_addresses.end()) {
        name += " [DEDUPED]";
      }
      name_offset = strtab->Write(name);
    }

    const auto* text = builder->GetText();
    uint64_t address = info.code_address;
    address += info.is_code_address_text_relative ? text->GetAddress() : 0;
    // Add in code delta, e.g., thumb bit 0 for Thumb2 code.
    address += CompiledMethod::CodeDelta(info.isa);
    symtab->Add(name_offset, text, address, info.code_size, STB_GLOBAL, STT_FUNC);

    // Conforming to aaelf, add $t mapping symbol to indicate start of a sequence of thumb2
    // instructions, so that disassembler tools can correctly disassemble.
    // Note that even if we generate just a single mapping symbol, ARM's Streamline
    // requires it to match function symbol.  Just address 0 does not work.
    if (info.isa == InstructionSet::kThumb2) {
      if (address < mapping_symbol_address || !kGenerateSingleArmMappingSymbol) {
        symtab->Add(strtab->Write("$t"), text, address & ~1, 0, STB_LOCAL, STT_NOTYPE);
        mapping_symbol_address = address;
      }
    }
  }
  // Add symbols for interpreted methods (with address range of the method's bytecode).
  if (!debug_info.dex_files.empty() && builder->GetDex()->Exists()) {
    auto dex = builder->GetDex();
    for (auto it : debug_info.dex_files) {
      uint64_t dex_address = dex->GetAddress() + it.first /* offset within the section */;
      const DexFile* dex_file = it.second;
      typename ElfTypes::Word dex_name = strtab->Write(kDexFileSymbolName);
      symtab->Add(dex_name, dex, dex_address, dex_file->Size(), STB_LOCAL, STT_NOTYPE);
      if (mini_debug_info) {
        continue;  // Don't add interpreter method names to mini-debug-info for now.
      }
      for (uint32_t i = 0; i < dex_file->NumClassDefs(); ++i) {
        const DexFile::ClassDef& class_def = dex_file->GetClassDef(i);
        const uint8_t* class_data = dex_file->GetClassData(class_def);
        if (class_data == nullptr) {
          continue;
        }
        for (ClassDataItemIterator item(*dex_file, class_data); item.HasNext(); item.Next()) {
          if (!item.IsAtMethod()) {
            continue;
          }
          const DexFile::CodeItem* code_item = item.GetMethodCodeItem();
          if (code_item == nullptr) {
            continue;
          }
          CodeItemInstructionAccessor code(*dex_file, code_item);
          DCHECK(code.HasCodeItem());
          std::string name = dex_file->PrettyMethod(item.GetMemberIndex(), !mini_debug_info);
          size_t name_offset = strtab->Write(name);
          uint64_t offset = reinterpret_cast<const uint8_t*>(code.Insns()) - dex_file->Begin();
          uint64_t address = dex_address + offset;
          size_t size = code.InsnsSizeInCodeUnits() * sizeof(uint16_t);
          symtab->Add(name_offset, dex, address, size, STB_GLOBAL, STT_FUNC);
        }
      }
    }
  }
  strtab->End();

  // Symbols are buffered and written after names (because they are smaller).
  symtab->WriteCachedSection();
}

}  // namespace debug
}  // namespace art

#endif  // ART_COMPILER_DEBUG_ELF_SYMTAB_WRITER_H_

