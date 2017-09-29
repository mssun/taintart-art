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

#ifndef ART_RUNTIME_DEX_INSTRUCTION_ITERATOR_H_
#define ART_RUNTIME_DEX_INSTRUCTION_ITERATOR_H_

#include <iterator>

#include "dex_instruction.h"
#include "base/logging.h"

namespace art {

class DexInstructionIterator : public std::iterator<std::forward_iterator_tag, Instruction> {
 public:
  using value_type = std::iterator<std::forward_iterator_tag, Instruction>::value_type;
  using difference_type = std::iterator<std::forward_iterator_tag, value_type>::difference_type;

  DexInstructionIterator() = default;
  DexInstructionIterator(const DexInstructionIterator&) = default;
  DexInstructionIterator(DexInstructionIterator&&) = default;
  DexInstructionIterator& operator=(const DexInstructionIterator&) = default;
  DexInstructionIterator& operator=(DexInstructionIterator&&) = default;

  explicit DexInstructionIterator(const value_type* inst) : inst_(inst) {}
  explicit DexInstructionIterator(const uint16_t* inst) : inst_(value_type::At(inst)) {}

  // Value after modification.
  DexInstructionIterator& operator++() {
    inst_ = inst_->Next();
    return *this;
  }

  // Value before modification.
  DexInstructionIterator operator++(int) {
    DexInstructionIterator temp = *this;
    ++*this;
    return temp;
  }

  const value_type& operator*() const {
    return *inst_;
  }

  const value_type* operator->() const {
    return &**this;
  }

  // Return the dex pc for an iterator compared to the code item begin.
  uint32_t GetDexPC(const DexInstructionIterator& code_item_begin) {
    return reinterpret_cast<const uint16_t*>(inst_) -
        reinterpret_cast<const uint16_t*>(code_item_begin.inst_);
  }

  const value_type* Inst() const {
    return inst_;
  }

 private:
  const value_type* inst_ = nullptr;
};

static ALWAYS_INLINE inline bool operator==(const DexInstructionIterator& lhs,
                                            const DexInstructionIterator& rhs) {
  return lhs.Inst() == rhs.Inst();
}

static inline bool operator!=(const DexInstructionIterator& lhs,
                              const DexInstructionIterator& rhs) {
  return !(lhs == rhs);
}

static inline bool operator<(const DexInstructionIterator& lhs,
                             const DexInstructionIterator& rhs) {
  return lhs.Inst() < rhs.Inst();
}

static inline bool operator>(const DexInstructionIterator& lhs,
                             const DexInstructionIterator& rhs) {
  return rhs < lhs;
}

static inline bool operator<=(const DexInstructionIterator& lhs,
                              const DexInstructionIterator& rhs) {
  return !(rhs < lhs);
}

static inline bool operator>=(const DexInstructionIterator& lhs,
                              const DexInstructionIterator& rhs) {
  return !(lhs < rhs);
}

}  // namespace art

#endif  // ART_RUNTIME_DEX_INSTRUCTION_ITERATOR_H_
