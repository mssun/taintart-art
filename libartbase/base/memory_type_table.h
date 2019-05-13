/*
 * Copyright (C) 2019 The Android Open Source Project
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

#ifndef ART_LIBARTBASE_BASE_MEMORY_TYPE_TABLE_H_
#define ART_LIBARTBASE_BASE_MEMORY_TYPE_TABLE_H_

#include <iostream>
#include <map>
#include <vector>

#include <android-base/macros.h>   // For DISALLOW_COPY_AND_ASSIGN
#include <android-base/logging.h>  // For DCHECK macros

namespace art {

// Class representing a memory range together with type attribute.
template <typename T>
class MemoryTypeRange final {
 public:
  MemoryTypeRange(uintptr_t start, uintptr_t limit, const T& type)
      : start_(start), limit_(limit), type_(type) {}
  MemoryTypeRange() : start_(0), limit_(0), type_() {}
  MemoryTypeRange(MemoryTypeRange&& other) = default;
  MemoryTypeRange(const MemoryTypeRange& other) = default;
  MemoryTypeRange& operator=(const MemoryTypeRange& other) = default;

  uintptr_t Start() const {
    DCHECK(IsValid());
    return start_;
  }

  uintptr_t Limit() const {
    DCHECK(IsValid());
    return limit_;
  }

  uintptr_t Size() const { return Limit() - Start(); }

  const T& Type() const { return type_; }

  bool IsValid() const { return start_ <= limit_; }

  bool Contains(uintptr_t address) const {
    return address >= Start() && address < Limit();
  }

  bool Overlaps(const MemoryTypeRange& other) const {
    bool disjoint = Limit() <= other.Start() || Start() >= other.Limit();
    return !disjoint;
  }

  bool Adjoins(const MemoryTypeRange& other) const {
    return other.Start() == Limit() || other.Limit() == Start();
  }

  bool CombinableWith(const MemoryTypeRange& other) const {
    return Type() == other.Type() && Adjoins(other);
  }

 private:
  uintptr_t start_;
  uintptr_t limit_;
  T type_;
};

// Class representing a table of memory ranges.
template <typename T>
class MemoryTypeTable final {
 public:
  // Class used to construct and populate MemoryTypeTable instances.
  class Builder;

  MemoryTypeTable() {}

  MemoryTypeTable(MemoryTypeTable&& table) : ranges_(std::move(table.ranges_)) {}

  MemoryTypeTable& operator=(MemoryTypeTable&& table) {
    ranges_ = std::move(table.ranges_);
    return *this;
  }

  // Find the range containing |address|.
  // Returns a pointer to a range on success, nullptr otherwise.
  const MemoryTypeRange<T>* Lookup(uintptr_t address) const {
    int last = static_cast<int>(ranges_.size()) - 1;
    for (int l = 0, r = last; l <= r;) {
      int m = l + (r - l) / 2;
      if (address < ranges_[m].Start()) {
        r = m - 1;
      } else if (address >= ranges_[m].Limit()) {
        l = m + 1;
      } else {
        DCHECK(ranges_[m].Contains(address))
            << reinterpret_cast<void*>(address) << " " << ranges_[m];
        return &ranges_[m];
      }
    }
    return nullptr;
  }

  size_t Size() const { return ranges_.size(); }

  void Print(std::ostream& os) const {
    for (const auto& range : ranges_) {
      os << range << std::endl;
    }
  }

 private:
  std::vector<MemoryTypeRange<T>> ranges_;

  DISALLOW_COPY_AND_ASSIGN(MemoryTypeTable);
};

// Class for building MemoryTypeTable instances. Supports
// adding ranges and looking up ranges.
template <typename T>
class MemoryTypeTable<T>::Builder final {
 public:
  Builder() {}

  // Adds a range if it is valid and doesn't overlap with existing ranges.  If the range adjoins
  // with an existing range, then the ranges are merged.
  //
  // Overlapping ranges and ranges of zero size are not supported.
  //
  // Returns true on success, false otherwise.
  inline bool Add(const MemoryTypeRange<T>& region);

  // Find the range containing |address|.
  // Returns a pointer to a range on success, nullptr otherwise.
  inline const MemoryTypeRange<T>* Lookup(uintptr_t address) const;

  // Returns number of unique ranges.
  inline size_t Size() const { return ranges_.size(); }

  // Generates a MemoryTypeTable for added ranges.
  MemoryTypeTable Build() const {
    MemoryTypeTable table;
    for (const auto& it : ranges_) {
      table.ranges_.push_back(it.second);
    }
    return table;
  }

 private:
  std::map<uintptr_t, MemoryTypeRange<T>> ranges_;

  DISALLOW_COPY_AND_ASSIGN(Builder);
};

template <typename T>
bool operator==(const MemoryTypeRange<T>& lhs, const MemoryTypeRange<T>& rhs) {
  return (lhs.Start() == rhs.Start() && lhs.Limit() == rhs.Limit() && lhs.Type() == rhs.Type());
}

template <typename T>
bool operator!=(const MemoryTypeRange<T>& lhs, const MemoryTypeRange<T>& rhs) {
  return !(lhs == rhs);
}

template <typename T>
std::ostream& operator<<(std::ostream& os, const MemoryTypeRange<T>& range) {
  os << reinterpret_cast<void*>(range.Start())
     << '-'
     << reinterpret_cast<void*>(range.Limit())
     << ' '
     << range.Type();
  return os;
}

template <typename T>
std::ostream& operator<<(std::ostream& os, const MemoryTypeTable<T>& table) {
  table.Print(os);
  return os;
}

template <typename T>
bool MemoryTypeTable<T>::Builder::Add(const MemoryTypeRange<T>& range) {
  if (UNLIKELY(!range.IsValid() || range.Size() == 0u)) {
    return false;
  }

  typename std::map<uintptr_t, MemoryTypeRange<T>>::iterator pred, succ;

  // Find an iterator for the next element in the ranges.
  succ = ranges_.lower_bound(range.Limit());

  // Find an iterator for a predecessor element.
  if (succ == ranges_.begin()) {
    // No predecessor element if the successor is at the beginning of ranges.
    pred = ranges_.end();
  } else if (succ != ranges_.end()) {
    // Predecessor is element before successor.
    pred = std::prev(succ);
  } else {
    // Predecessor is the last element in a non-empty map when there is no successor.
    pred = ranges_.find(ranges_.rbegin()->first);
  }

  // Invalidate |succ| if it cannot be combined with |range|.
  if (succ != ranges_.end()) {
    DCHECK_GE(succ->second.Limit(), range.Start());
    if (range.Overlaps(succ->second)) {
      return false;
    }
    if (!range.CombinableWith(succ->second)) {
      succ = ranges_.end();
    }
  }

  // Invalidate |pred| if it cannot be combined with |range|.
  if (pred != ranges_.end()) {
    if (range.Overlaps(pred->second)) {
      return false;
    }
    if (!range.CombinableWith(pred->second)) {
      pred = ranges_.end();
    }
  }

  if (pred == ranges_.end()) {
    if (succ == ranges_.end()) {
      // |pred| is invalid, |succ| is invalid.
      // No compatible neighbors for merging.
      DCHECK(ranges_.find(range.Limit()) == ranges_.end());
      ranges_[range.Limit()] = range;
    } else {
      // |pred| is invalid, |succ| is valid. Merge into |succ|.
      const uintptr_t limit = succ->second.Limit();
      DCHECK_GT(limit, range.Limit());
      ranges_.erase(succ);
      ranges_[limit] = MemoryTypeRange<T>(range.Start(), limit, range.Type());
    }
  } else {
    if (succ == ranges_.end()) {
      // |pred| is valid, |succ| is invalid. Merge into |pred|.
      const uintptr_t start = pred->second.Start();
      const uintptr_t limit = range.Limit();
      DCHECK_LT(start, range.Start());
      DCHECK_GT(limit, pred->second.Limit());
      ranges_.erase(pred);
      ranges_[limit] = MemoryTypeRange<T>(start, limit, range.Type());
    } else {
      // |pred| is valid, |succ| is valid. Merge between |pred| and |succ|.
      DCHECK_LT(pred->second.Start(), range.Start());
      DCHECK_GT(succ->second.Limit(), range.Limit());
      const uintptr_t start = pred->second.Start();
      const uintptr_t limit = succ->second.Limit();
      ranges_.erase(pred, ++succ);
      ranges_[limit] = MemoryTypeRange<T>(start, limit, range.Type());
    }
  }
  return true;
}

template <typename T>
const MemoryTypeRange<T>* MemoryTypeTable<T>::Builder::Lookup(uintptr_t address) const {
  auto it = ranges_.upper_bound(address);
  if (it != ranges_.end() && it->second.Contains(address)) {
    return &it->second;
  } else {
    return nullptr;
  }
}

}  // namespace art

#endif  // ART_LIBARTBASE_BASE_MEMORY_TYPE_TABLE_H_
