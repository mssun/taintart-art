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

// Base helper class to prevent duplicated comparators.
class DexInstructionIteratorBase : public std::iterator<std::forward_iterator_tag, Instruction> {
 public:
  using value_type = std::iterator<std::forward_iterator_tag, Instruction>::value_type;
  using difference_type = std::iterator<std::forward_iterator_tag, value_type>::difference_type;

  DexInstructionIteratorBase() = default;
  explicit DexInstructionIteratorBase(const value_type* inst) : inst_(inst) {}

  const value_type* Inst() const {
    return inst_;
  }

  // Return the dex pc for an iterator compared to the code item begin.
  uint32_t GetDexPC(const DexInstructionIteratorBase& code_item_begin) {
    return reinterpret_cast<const uint16_t*>(inst_) -
        reinterpret_cast<const uint16_t*>(code_item_begin.inst_);
  }

 protected:
  const value_type* inst_ = nullptr;
};


static ALWAYS_INLINE inline bool operator==(const DexInstructionIteratorBase& lhs,
                                            const DexInstructionIteratorBase& rhs) {
  return lhs.Inst() == rhs.Inst();
}

static inline bool operator!=(const DexInstructionIteratorBase& lhs,
                              const DexInstructionIteratorBase& rhs) {
  return !(lhs == rhs);
}

static inline bool operator<(const DexInstructionIteratorBase& lhs,
                             const DexInstructionIteratorBase& rhs) {
  return lhs.Inst() < rhs.Inst();
}

static inline bool operator>(const DexInstructionIteratorBase& lhs,
                             const DexInstructionIteratorBase& rhs) {
  return rhs < lhs;
}

static inline bool operator<=(const DexInstructionIteratorBase& lhs,
                              const DexInstructionIteratorBase& rhs) {
  return !(rhs < lhs);
}

static inline bool operator>=(const DexInstructionIteratorBase& lhs,
                              const DexInstructionIteratorBase& rhs) {
  return !(lhs < rhs);
}

class DexInstructionIterator : public DexInstructionIteratorBase {
 public:
  using value_type = std::iterator<std::forward_iterator_tag, Instruction>::value_type;
  using difference_type = std::iterator<std::forward_iterator_tag, value_type>::difference_type;
  using DexInstructionIteratorBase::DexInstructionIteratorBase;

  explicit DexInstructionIterator(const uint16_t* inst)
      : DexInstructionIteratorBase(value_type::At(inst)) {}

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
};

class SafeDexInstructionIterator : public DexInstructionIteratorBase {
 public:
  explicit SafeDexInstructionIterator(const DexInstructionIteratorBase& start,
                                      const DexInstructionIteratorBase& end)
      : DexInstructionIteratorBase(start.Inst())
      , end_(end.Inst()) {}

  // Value after modification, does not read past the end of the allowed region. May increment past
  // the end of the code item though.
  SafeDexInstructionIterator& operator++() {
    AssertValid();
    const size_t size_code_units = Inst()->CodeUnitsRequiredForSizeComputation();
    const size_t available = reinterpret_cast<const uint16_t*>(end_) -
        reinterpret_cast<const uint16_t*>(Inst());
    if (UNLIKELY(size_code_units > available)) {
      error_state_ = true;
      return *this;
    }
    const size_t instruction_size = inst_->SizeInCodeUnits();
    if (UNLIKELY(instruction_size > available)) {
      error_state_ = true;
      return *this;
    }
    inst_ = inst_->RelativeAt(instruction_size);
    return *this;
  }

  // Value before modification.
  SafeDexInstructionIterator operator++(int) {
    SafeDexInstructionIterator temp = *this;
    ++*this;
    return temp;
  }

  const value_type& operator*() const {
    AssertValid();
    return *inst_;
  }

  const value_type* operator->() const {
    AssertValid();
    return &**this;
  }

  // Returns true if the iterator is in an error state. This occurs when an instruction couldn't
  // have its size computed without reading past the end iterator.
  bool IsErrorState() const {
    return error_state_;
  }

 private:
  ALWAYS_INLINE void AssertValid() const {
    DCHECK(!IsErrorState());
    DCHECK_LT(Inst(), end_);
  }

  const value_type* end_ = nullptr;
  bool error_state_ = false;
};

}  // namespace art

#endif  // ART_RUNTIME_DEX_INSTRUCTION_ITERATOR_H_
