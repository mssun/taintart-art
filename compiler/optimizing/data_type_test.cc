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

#include <gtest/gtest.h>

#include "data_type-inl.h"

#include "primitive.h"

namespace art {

template <DataType::Type data_type, Primitive::Type primitive_type>
static void CheckConversion() {
  static_assert(data_type == DataTypeFromPrimitive(primitive_type), "Conversion check.");
  static_assert(DataType::Size(data_type) == Primitive::ComponentSize(primitive_type),
                "Size check.");
}

TEST(DataType, SizeAgainstPrimitive) {
  CheckConversion<DataType::Type::kVoid, Primitive::kPrimVoid>();
  CheckConversion<DataType::Type::kBool, Primitive::kPrimBoolean>();
  CheckConversion<DataType::Type::kInt8, Primitive::kPrimByte>();
  CheckConversion<DataType::Type::kUint16, Primitive::kPrimChar>();
  CheckConversion<DataType::Type::kInt16, Primitive::kPrimShort>();
  CheckConversion<DataType::Type::kInt32, Primitive::kPrimInt>();
  CheckConversion<DataType::Type::kInt64, Primitive::kPrimLong>();
  CheckConversion<DataType::Type::kFloat32, Primitive::kPrimFloat>();
  CheckConversion<DataType::Type::kFloat64, Primitive::kPrimDouble>();
  CheckConversion<DataType::Type::kReference, Primitive::kPrimNot>();
}

TEST(DataType, Names) {
#define CHECK_NAME(type) EXPECT_STREQ(#type, DataType::PrettyDescriptor(DataType::Type::k##type))
  CHECK_NAME(Void);
  CHECK_NAME(Bool);
  CHECK_NAME(Int8);
  CHECK_NAME(Uint16);
  CHECK_NAME(Int16);
  CHECK_NAME(Int32);
  CHECK_NAME(Int64);
  CHECK_NAME(Float32);
  CHECK_NAME(Float64);
  CHECK_NAME(Reference);
#undef CHECK_NAME
}

}  // namespace art
