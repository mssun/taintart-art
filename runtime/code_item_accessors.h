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

// TODO: Dex helpers have ART specific APIs, we may want to refactor these for use in dexdump.

#ifndef ART_RUNTIME_CODE_ITEM_ACCESSORS_H_
#define ART_RUNTIME_CODE_ITEM_ACCESSORS_H_

#include "base/mutex.h"
#include "cdex/compact_dex_file.h"
#include "dex_file.h"
#include "dex_instruction_iterator.h"
#include "standard_dex_file.h"

namespace art {

class ArtMethod;

// Abstracts accesses to the instruction fields of code items for CompactDexFile and
// StandardDexFile.
class CodeItemInstructionAccessor {
 public:
  ALWAYS_INLINE CodeItemInstructionAccessor(const DexFile* dex_file,
                                            const DexFile::CodeItem* code_item);

  ALWAYS_INLINE explicit CodeItemInstructionAccessor(ArtMethod* method);

  ALWAYS_INLINE DexInstructionIterator begin() const;

  ALWAYS_INLINE DexInstructionIterator end() const;

  uint32_t InsnsSizeInCodeUnits() const {
    return insns_size_in_code_units_;
  }

  const uint16_t* Insns() const {
    return insns_;
  }

  // Return the instruction for a dex pc.
  const Instruction& InstructionAt(uint32_t dex_pc) const {
    return *Instruction::At(insns_ + dex_pc);
  }

  // Return true if the accessor has a code item.
  bool HasCodeItem() const {
    return Insns() != nullptr;
  }

  // CreateNullable allows ArtMethods that have a null code item.
  ALWAYS_INLINE static CodeItemInstructionAccessor CreateNullable(ArtMethod* method)
      REQUIRES_SHARED(Locks::mutator_lock_);

 protected:
  CodeItemInstructionAccessor() = default;

  ALWAYS_INLINE void Init(const CompactDexFile::CodeItem& code_item);
  ALWAYS_INLINE void Init(const StandardDexFile::CodeItem& code_item);
  ALWAYS_INLINE void Init(const DexFile* dex_file, const DexFile::CodeItem* code_item);

 private:
  // size of the insns array, in 2 byte code units. 0 if there is no code item.
  uint32_t insns_size_in_code_units_ = 0;

  // Pointer to the instructions, null if there is no code item.
  const uint16_t* insns_ = 0;
};

// Abstracts accesses to code item fields other than debug info for CompactDexFile and
// StandardDexFile.
class CodeItemDataAccessor : public CodeItemInstructionAccessor {
 public:
  ALWAYS_INLINE CodeItemDataAccessor(const DexFile* dex_file, const DexFile::CodeItem* code_item);

  ALWAYS_INLINE explicit CodeItemDataAccessor(ArtMethod* method);

  uint16_t RegistersSize() const {
    return registers_size_;
  }

  uint16_t InsSize() const {
    return ins_size_;
  }

  uint16_t OutsSize() const {
    return outs_size_;
  }

  uint16_t TriesSize() const {
    return tries_size_;
  }

  IterationRange<const DexFile::TryItem*> TryItems() const;

  const uint8_t* GetCatchHandlerData(size_t offset = 0) const;

  const DexFile::TryItem* FindTryItem(uint32_t try_dex_pc) const;

  // CreateNullable allows ArtMethods that have a null code item.
  ALWAYS_INLINE static CodeItemDataAccessor CreateNullable(ArtMethod* method)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE static CodeItemDataAccessor CreateNullable(
      const DexFile* dex_file,
      const DexFile::CodeItem* code_item);

 protected:
  CodeItemDataAccessor() = default;

  ALWAYS_INLINE void Init(const CompactDexFile::CodeItem& code_item);
  ALWAYS_INLINE void Init(const StandardDexFile::CodeItem& code_item);
  ALWAYS_INLINE void Init(const DexFile* dex_file, const DexFile::CodeItem* code_item);

 private:
  // Fields mirrored from the dex/cdex code item.
  uint16_t registers_size_;
  uint16_t ins_size_;
  uint16_t outs_size_;
  uint16_t tries_size_;
};

}  // namespace art

#endif  // ART_RUNTIME_CODE_ITEM_ACCESSORS_H_
