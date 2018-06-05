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

#ifndef ART_LIBARTBASE_BASE_STATS_H_
#define ART_LIBARTBASE_BASE_STATS_H_

#include <unordered_map>

#include "globals.h"

namespace art {

// Simple structure to record tree of statistical values.
class Stats {
 public:
  double Value() const { return value_; }
  size_t Count() const { return count_; }
  Stats* Child(const char* name) { return &children_[name]; }
  const std::unordered_map<const char*, Stats>& Children() const { return children_; }

  void AddBytes(double bytes, size_t count = 1) { Add(bytes, count); }
  void AddBits(double bits, size_t count = 1) { Add(bits / kBitsPerByte, count); }
  void AddSeconds(double s, size_t count = 1) { Add(s, count); }
  void AddNanoSeconds(double ns, size_t count = 1) { Add(ns / 1000000000.0, count); }

  double SumChildrenValues() const {
    double sum = 0.0;
    for (auto it : children_) {
      sum += it.second.Value();
    }
    return sum;
  }

 private:
  void Add(double value, size_t count = 1) {
    value_ += value;
    count_ += count;
  }

  double value_ = 0.0;  // Commutative sum of the collected statistic in basic units.
  size_t count_ = 0;    // The number of samples for this node.
  std::unordered_map<const char*, Stats> children_;
};

}  // namespace art

#endif  // ART_LIBARTBASE_BASE_STATS_H_
