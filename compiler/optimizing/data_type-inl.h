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

#ifndef ART_COMPILER_OPTIMIZING_DATA_TYPE_INL_H_
#define ART_COMPILER_OPTIMIZING_DATA_TYPE_INL_H_

#include "data_type.h"
#include "primitive.h"

namespace art {

// Note: Not declared in data_type.h to avoid pulling in "primitive.h".
constexpr DataType::Type DataTypeFromPrimitive(Primitive::Type type) {
  switch (type) {
    case Primitive::kPrimNot: return DataType::Type::kReference;
    case Primitive::kPrimBoolean: return DataType::Type::kBool;
    case Primitive::kPrimByte: return DataType::Type::kInt8;
    case Primitive::kPrimChar: return DataType::Type::kUint16;
    case Primitive::kPrimShort: return DataType::Type::kInt16;
    case Primitive::kPrimInt: return DataType::Type::kInt32;
    case Primitive::kPrimLong: return DataType::Type::kInt64;
    case Primitive::kPrimFloat: return DataType::Type::kFloat32;
    case Primitive::kPrimDouble: return DataType::Type::kFloat64;
    case Primitive::kPrimVoid: return DataType::Type::kVoid;
  }
  LOG(FATAL) << "Unreachable";
  UNREACHABLE();
}

constexpr DataType::Type DataType::FromShorty(char type) {
  return DataTypeFromPrimitive(Primitive::GetType(type));
}

constexpr char DataType::TypeId(DataType::Type type) {
  // Type id for visualizer.
  switch (type) {
    case DataType::Type::kBool: return 'z';
    case DataType::Type::kInt8: return 'b';
    case DataType::Type::kUint16: return 'c';
    case DataType::Type::kInt16: return 's';
    case DataType::Type::kInt32: return 'i';
    case DataType::Type::kInt64: return 'j';
    case DataType::Type::kFloat32: return 'f';
    case DataType::Type::kFloat64: return 'd';
    case DataType::Type::kReference: return 'l';
    case DataType::Type::kVoid: return 'v';
  }
  LOG(FATAL) << "Unreachable";
  UNREACHABLE();
}

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_DATA_TYPE_INL_H_
