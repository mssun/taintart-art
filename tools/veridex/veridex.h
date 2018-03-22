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

#ifndef ART_TOOLS_VERIDEX_VERIDEX_H_
#define ART_TOOLS_VERIDEX_VERIDEX_H_

#include <map>

#include "dex/dex_file.h"
#include "dex/primitive.h"

namespace art {

/**
 * Abstraction for classes defined, or implicitly defined (for arrays and primitives)
 * in dex files.
 */
class VeriClass {
 public:
  VeriClass(const VeriClass& other) = default;
  VeriClass() = default;
  VeriClass(Primitive::Type k, uint8_t dims, const DexFile::ClassDef* cl)
      : kind_(k), dimensions_(dims), class_def_(cl) {}

  bool IsUninitialized() const {
    return kind_ == Primitive::Type::kPrimNot && dimensions_ == 0 && class_def_ == nullptr;
  }

  bool IsPrimitive() const {
    return kind_ != Primitive::Type::kPrimNot && dimensions_ == 0;
  }

  bool IsArray() const {
    return dimensions_ != 0;
  }

 private:
  Primitive::Type kind_;
  uint8_t dimensions_;
  const DexFile::ClassDef* class_def_;
};

/**
 * Abstraction for fields defined in dex files. Currently, that's a pointer into their
 * `encoded_field` description.
 */
using VeriField = const uint8_t*;

/**
 * Abstraction for methods defined in dex files. Currently, that's a pointer into their
 * `encoded_method` description.
 */
using VeriMethod = const uint8_t*;

using TypeMap = std::map<std::string, VeriClass*>;

}  // namespace art

#endif  // ART_TOOLS_VERIDEX_VERIDEX_H_
