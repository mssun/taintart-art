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

#ifndef ART_COMPILER_OPTIMIZING_NODES_VECTOR_H_
#define ART_COMPILER_OPTIMIZING_NODES_VECTOR_H_

// This #include should never be used by compilation, because this header file (nodes_vector.h)
// is included in the header file nodes.h itself. However it gives editing tools better context.
#include "nodes.h"

namespace art {

// Memory alignment, represented as an offset relative to a base, where 0 <= offset < base,
// and base is a power of two. For example, the value Alignment(16, 0) means memory is
// perfectly aligned at a 16-byte boundary, whereas the value Alignment(16, 4) means
// memory is always exactly 4 bytes above such a boundary.
class Alignment {
 public:
  Alignment(size_t base, size_t offset) : base_(base), offset_(offset) {
    DCHECK_LT(offset, base);
    DCHECK(IsPowerOfTwo(base));
  }

  // Returns true if memory is at least aligned at the given boundary.
  // Assumes requested base is power of two.
  bool IsAlignedAt(size_t base) const {
    DCHECK_NE(0u, base);
    DCHECK(IsPowerOfTwo(base));
    return ((offset_ | base_) & (base - 1u)) == 0;
  }

  size_t Base() const { return base_; }

  size_t Offset() const { return offset_; }

  std::string ToString() const {
    return "ALIGN(" + std::to_string(base_) + "," + std::to_string(offset_) + ")";
  }

  bool operator==(const Alignment& other) const {
    return base_ == other.base_ && offset_ == other.offset_;
  }

 private:
  size_t base_;
  size_t offset_;
};

//
// Definitions of abstract vector operations in HIR.
//

// Abstraction of a vector operation, i.e., an operation that performs
// GetVectorLength() x GetPackedType() operations simultaneously.
class HVecOperation : public HVariableInputSizeInstruction {
 public:
  // A SIMD operation looks like a FPU location.
  // TODO: we could introduce SIMD types in HIR.
  static constexpr DataType::Type kSIMDType = DataType::Type::kFloat64;

  HVecOperation(ArenaAllocator* allocator,
                DataType::Type packed_type,
                SideEffects side_effects,
                size_t number_of_inputs,
                size_t vector_length,
                uint32_t dex_pc)
      : HVariableInputSizeInstruction(side_effects,
                                      dex_pc,
                                      allocator,
                                      number_of_inputs,
                                      kArenaAllocVectorNode),
        vector_length_(vector_length) {
    SetPackedField<TypeField>(packed_type);
    DCHECK_LT(1u, vector_length);
  }

  // Returns the number of elements packed in a vector.
  size_t GetVectorLength() const {
    return vector_length_;
  }

  // Returns the number of bytes in a full vector.
  size_t GetVectorNumberOfBytes() const {
    return vector_length_ * DataType::Size(GetPackedType());
  }

  // Returns the type of the vector operation.
  DataType::Type GetType() const OVERRIDE {
    return kSIMDType;
  }

  // Returns the true component type packed in a vector.
  DataType::Type GetPackedType() const {
    return GetPackedField<TypeField>();
  }

  // Assumes vector nodes cannot be moved by default. Each concrete implementation
  // that can be moved should override this method and return true.
  bool CanBeMoved() const OVERRIDE { return false; }

  // Tests if all data of a vector node (vector length and packed type) is equal.
  // Each concrete implementation that adds more fields should test equality of
  // those fields in its own method *and* call all super methods.
  bool InstructionDataEquals(const HInstruction* other) const OVERRIDE {
    DCHECK(other->IsVecOperation());
    const HVecOperation* o = other->AsVecOperation();
    return GetVectorLength() == o->GetVectorLength() && GetPackedType() == o->GetPackedType();
  }

  // Maps an integral type to the same-size signed type and leaves other types alone.
  // Can be used to test relaxed type consistency in which packed same-size integral
  // types can co-exist, but other type mixes are an error.
  static DataType::Type ToSignedType(DataType::Type type) {
    switch (type) {
      case DataType::Type::kBool:  // 1-byte storage unit
      case DataType::Type::kUint8:
        return DataType::Type::kInt8;
      case DataType::Type::kUint16:
        return DataType::Type::kInt16;
      default:
        DCHECK(type != DataType::Type::kVoid && type != DataType::Type::kReference) << type;
        return type;
    }
  }

  // Maps an integral type to the same-size unsigned type and leaves other types alone.
  static DataType::Type ToUnsignedType(DataType::Type type) {
    switch (type) {
      case DataType::Type::kBool:  // 1-byte storage unit
      case DataType::Type::kInt8:
        return DataType::Type::kUint8;
      case DataType::Type::kInt16:
        return DataType::Type::kUint16;
      default:
        DCHECK(type != DataType::Type::kVoid && type != DataType::Type::kReference) << type;
        return type;
    }
  }

  DECLARE_ABSTRACT_INSTRUCTION(VecOperation);

 protected:
  // Additional packed bits.
  static constexpr size_t kFieldType = HInstruction::kNumberOfGenericPackedBits;
  static constexpr size_t kFieldTypeSize =
      MinimumBitsToStore(static_cast<size_t>(DataType::Type::kLast));
  static constexpr size_t kNumberOfVectorOpPackedBits = kFieldType + kFieldTypeSize;
  static_assert(kNumberOfVectorOpPackedBits <= kMaxNumberOfPackedBits, "Too many packed fields.");
  using TypeField = BitField<DataType::Type, kFieldType, kFieldTypeSize>;

 private:
  const size_t vector_length_;

  DISALLOW_COPY_AND_ASSIGN(HVecOperation);
};

// Abstraction of a unary vector operation.
class HVecUnaryOperation : public HVecOperation {
 public:
  HVecUnaryOperation(ArenaAllocator* allocator,
                     HInstruction* input,
                     DataType::Type packed_type,
                     size_t vector_length,
                     uint32_t dex_pc)
      : HVecOperation(allocator,
                      packed_type,
                      SideEffects::None(),
                      /* number_of_inputs */ 1,
                      vector_length,
                      dex_pc) {
    SetRawInputAt(0, input);
  }

  HInstruction* GetInput() const { return InputAt(0); }

  DECLARE_ABSTRACT_INSTRUCTION(VecUnaryOperation);

 private:
  DISALLOW_COPY_AND_ASSIGN(HVecUnaryOperation);
};

// Abstraction of a binary vector operation.
class HVecBinaryOperation : public HVecOperation {
 public:
  HVecBinaryOperation(ArenaAllocator* allocator,
                      HInstruction* left,
                      HInstruction* right,
                      DataType::Type packed_type,
                      size_t vector_length,
                      uint32_t dex_pc)
      : HVecOperation(allocator,
                      packed_type,
                      SideEffects::None(),
                      /* number_of_inputs */ 2,
                      vector_length,
                      dex_pc) {
    SetRawInputAt(0, left);
    SetRawInputAt(1, right);
  }

  HInstruction* GetLeft() const { return InputAt(0); }
  HInstruction* GetRight() const { return InputAt(1); }

  DECLARE_ABSTRACT_INSTRUCTION(VecBinaryOperation);

 private:
  DISALLOW_COPY_AND_ASSIGN(HVecBinaryOperation);
};

// Abstraction of a vector operation that references memory, with an alignment.
// The Android runtime guarantees elements have at least natural alignment.
class HVecMemoryOperation : public HVecOperation {
 public:
  HVecMemoryOperation(ArenaAllocator* allocator,
                      DataType::Type packed_type,
                      SideEffects side_effects,
                      size_t number_of_inputs,
                      size_t vector_length,
                      uint32_t dex_pc)
      : HVecOperation(allocator,
                      packed_type,
                      side_effects,
                      number_of_inputs,
                      vector_length,
                      dex_pc),
        alignment_(DataType::Size(packed_type), 0) {
    DCHECK_GE(number_of_inputs, 2u);
  }

  void SetAlignment(Alignment alignment) { alignment_ = alignment; }

  Alignment GetAlignment() const { return alignment_; }

  HInstruction* GetArray() const { return InputAt(0); }
  HInstruction* GetIndex() const { return InputAt(1); }

  bool InstructionDataEquals(const HInstruction* other) const OVERRIDE {
    DCHECK(other->IsVecMemoryOperation());
    const HVecMemoryOperation* o = other->AsVecMemoryOperation();
    return HVecOperation::InstructionDataEquals(o) && GetAlignment() == o->GetAlignment();
  }

  DECLARE_ABSTRACT_INSTRUCTION(VecMemoryOperation);

 private:
  Alignment alignment_;

  DISALLOW_COPY_AND_ASSIGN(HVecMemoryOperation);
};

// Packed type consistency checker ("same vector length" integral types may mix freely).
inline static bool HasConsistentPackedTypes(HInstruction* input, DataType::Type type) {
  if (input->IsPhi()) {
    return input->GetType() == HVecOperation::kSIMDType;  // carries SIMD
  }
  DCHECK(input->IsVecOperation());
  DataType::Type input_type = input->AsVecOperation()->GetPackedType();
  DCHECK_EQ(HVecOperation::ToUnsignedType(input_type) == HVecOperation::ToUnsignedType(type),
            HVecOperation::ToSignedType(input_type) == HVecOperation::ToSignedType(type));
  return HVecOperation::ToSignedType(input_type) == HVecOperation::ToSignedType(type);
}

//
// Definitions of concrete unary vector operations in HIR.
//

// Replicates the given scalar into a vector,
// viz. replicate(x) = [ x, .. , x ].
class HVecReplicateScalar FINAL : public HVecUnaryOperation {
 public:
  HVecReplicateScalar(ArenaAllocator* allocator,
                      HInstruction* scalar,
                      DataType::Type packed_type,
                      size_t vector_length,
                      uint32_t dex_pc)
      : HVecUnaryOperation(allocator, scalar, packed_type, vector_length, dex_pc) {
    DCHECK(!scalar->IsVecOperation());
  }

  // A replicate needs to stay in place, since SIMD registers are not
  // kept alive across vector loop boundaries (yet).
  bool CanBeMoved() const OVERRIDE { return false; }

  DECLARE_INSTRUCTION(VecReplicateScalar);

 private:
  DISALLOW_COPY_AND_ASSIGN(HVecReplicateScalar);
};

// Extracts a particular scalar from the given vector,
// viz. extract[ x1, .. , xn ] = x_i.
//
// TODO: for now only i == 1 case supported.
class HVecExtractScalar FINAL : public HVecUnaryOperation {
 public:
  HVecExtractScalar(ArenaAllocator* allocator,
                    HInstruction* input,
                    DataType::Type packed_type,
                    size_t vector_length,
                    size_t index,
                    uint32_t dex_pc)
      : HVecUnaryOperation(allocator, input, packed_type, vector_length, dex_pc) {
    DCHECK(HasConsistentPackedTypes(input, packed_type));
    DCHECK_LT(index, vector_length);
    DCHECK_EQ(index, 0u);
  }

  // Yields a single component in the vector.
  DataType::Type GetType() const OVERRIDE {
    return GetPackedType();
  }

  // An extract needs to stay in place, since SIMD registers are not
  // kept alive across vector loop boundaries (yet).
  bool CanBeMoved() const OVERRIDE { return false; }

  DECLARE_INSTRUCTION(VecExtractScalar);

 private:
  DISALLOW_COPY_AND_ASSIGN(HVecExtractScalar);
};

// Reduces the given vector into the first element as sum/min/max,
// viz. sum-reduce[ x1, .. , xn ] = [ y, ---- ], where y = sum xi
// and the "-" denotes "don't care" (implementation dependent).
class HVecReduce FINAL : public HVecUnaryOperation {
 public:
  enum ReductionKind {
    kSum = 1,
    kMin = 2,
    kMax = 3
  };

  HVecReduce(ArenaAllocator* allocator,
             HInstruction* input,
             DataType::Type packed_type,
             size_t vector_length,
             ReductionKind kind,
             uint32_t dex_pc)
      : HVecUnaryOperation(allocator, input, packed_type, vector_length, dex_pc),
        kind_(kind) {
    DCHECK(HasConsistentPackedTypes(input, packed_type));
  }

  ReductionKind GetKind() const { return kind_; }

  bool CanBeMoved() const OVERRIDE { return true; }

  bool InstructionDataEquals(const HInstruction* other) const OVERRIDE {
    DCHECK(other->IsVecReduce());
    const HVecReduce* o = other->AsVecReduce();
    return HVecOperation::InstructionDataEquals(o) && GetKind() == o->GetKind();
  }

  DECLARE_INSTRUCTION(VecReduce);

 private:
  const ReductionKind kind_;

  DISALLOW_COPY_AND_ASSIGN(HVecReduce);
};

// Converts every component in the vector,
// viz. cnv[ x1, .. , xn ]  = [ cnv(x1), .. , cnv(xn) ].
class HVecCnv FINAL : public HVecUnaryOperation {
 public:
  HVecCnv(ArenaAllocator* allocator,
          HInstruction* input,
          DataType::Type packed_type,
          size_t vector_length,
          uint32_t dex_pc)
      : HVecUnaryOperation(allocator, input, packed_type, vector_length, dex_pc) {
    DCHECK(input->IsVecOperation());
    DCHECK_NE(GetInputType(), GetResultType());  // actual convert
  }

  DataType::Type GetInputType() const { return InputAt(0)->AsVecOperation()->GetPackedType(); }
  DataType::Type GetResultType() const { return GetPackedType(); }

  bool CanBeMoved() const OVERRIDE { return true; }

  DECLARE_INSTRUCTION(VecCnv);

 private:
  DISALLOW_COPY_AND_ASSIGN(HVecCnv);
};

// Negates every component in the vector,
// viz. neg[ x1, .. , xn ]  = [ -x1, .. , -xn ].
class HVecNeg FINAL : public HVecUnaryOperation {
 public:
  HVecNeg(ArenaAllocator* allocator,
          HInstruction* input,
          DataType::Type packed_type,
          size_t vector_length,
          uint32_t dex_pc)
      : HVecUnaryOperation(allocator, input, packed_type, vector_length, dex_pc) {
    DCHECK(HasConsistentPackedTypes(input, packed_type));
  }

  bool CanBeMoved() const OVERRIDE { return true; }

  DECLARE_INSTRUCTION(VecNeg);

 private:
  DISALLOW_COPY_AND_ASSIGN(HVecNeg);
};

// Takes absolute value of every component in the vector,
// viz. abs[ x1, .. , xn ]  = [ |x1|, .. , |xn| ]
// for signed operand x.
class HVecAbs FINAL : public HVecUnaryOperation {
 public:
  HVecAbs(ArenaAllocator* allocator,
          HInstruction* input,
          DataType::Type packed_type,
          size_t vector_length,
          uint32_t dex_pc)
      : HVecUnaryOperation(allocator, input, packed_type, vector_length, dex_pc) {
    DCHECK(HasConsistentPackedTypes(input, packed_type));
  }

  bool CanBeMoved() const OVERRIDE { return true; }

  DECLARE_INSTRUCTION(VecAbs);

 private:
  DISALLOW_COPY_AND_ASSIGN(HVecAbs);
};

// Bitwise- or boolean-nots every component in the vector,
// viz. not[ x1, .. , xn ]  = [ ~x1, .. , ~xn ], or
//      not[ x1, .. , xn ]  = [ !x1, .. , !xn ] for boolean.
class HVecNot FINAL : public HVecUnaryOperation {
 public:
  HVecNot(ArenaAllocator* allocator,
          HInstruction* input,
          DataType::Type packed_type,
          size_t vector_length,
          uint32_t dex_pc)
      : HVecUnaryOperation(allocator, input, packed_type, vector_length, dex_pc) {
    DCHECK(input->IsVecOperation());
  }

  bool CanBeMoved() const OVERRIDE { return true; }

  DECLARE_INSTRUCTION(VecNot);

 private:
  DISALLOW_COPY_AND_ASSIGN(HVecNot);
};

//
// Definitions of concrete binary vector operations in HIR.
//

// Adds every component in the two vectors,
// viz. [ x1, .. , xn ] + [ y1, .. , yn ] = [ x1 + y1, .. , xn + yn ].
class HVecAdd FINAL : public HVecBinaryOperation {
 public:
  HVecAdd(ArenaAllocator* allocator,
          HInstruction* left,
          HInstruction* right,
          DataType::Type packed_type,
          size_t vector_length,
          uint32_t dex_pc)
      : HVecBinaryOperation(allocator, left, right, packed_type, vector_length, dex_pc) {
    DCHECK(HasConsistentPackedTypes(left, packed_type));
    DCHECK(HasConsistentPackedTypes(right, packed_type));
  }

  bool CanBeMoved() const OVERRIDE { return true; }

  DECLARE_INSTRUCTION(VecAdd);

 private:
  DISALLOW_COPY_AND_ASSIGN(HVecAdd);
};

// Performs halving add on every component in the two vectors, viz.
// rounded   [ x1, .. , xn ] hradd [ y1, .. , yn ] = [ (x1 + y1 + 1) >> 1, .. , (xn + yn + 1) >> 1 ]
// truncated [ x1, .. , xn ] hadd  [ y1, .. , yn ] = [ (x1 + y1)     >> 1, .. , (xn + yn )    >> 1 ]
// for either both signed or both unsigned operands x, y.
class HVecHalvingAdd FINAL : public HVecBinaryOperation {
 public:
  HVecHalvingAdd(ArenaAllocator* allocator,
                 HInstruction* left,
                 HInstruction* right,
                 DataType::Type packed_type,
                 size_t vector_length,
                 bool is_rounded,
                 bool is_unsigned,
                 uint32_t dex_pc)
      : HVecBinaryOperation(allocator, left, right, packed_type, vector_length, dex_pc) {
    // The `is_unsigned` flag should be used exclusively with the Int32 or Int64.
    // This flag is a temporary measure while we do not have the Uint32 and Uint64 data types.
    DCHECK(!is_unsigned ||
           packed_type == DataType::Type::kInt32 ||
           packed_type == DataType::Type::kInt64) << packed_type;
    DCHECK(HasConsistentPackedTypes(left, packed_type));
    DCHECK(HasConsistentPackedTypes(right, packed_type));
    SetPackedFlag<kFieldHAddIsUnsigned>(is_unsigned);
    SetPackedFlag<kFieldHAddIsRounded>(is_rounded);
  }

  bool IsUnsigned() const { return GetPackedFlag<kFieldHAddIsUnsigned>(); }
  bool IsRounded() const { return GetPackedFlag<kFieldHAddIsRounded>(); }

  bool CanBeMoved() const OVERRIDE { return true; }

  bool InstructionDataEquals(const HInstruction* other) const OVERRIDE {
    DCHECK(other->IsVecHalvingAdd());
    const HVecHalvingAdd* o = other->AsVecHalvingAdd();
    return HVecOperation::InstructionDataEquals(o) &&
        IsUnsigned() == o->IsUnsigned() &&
        IsRounded() == o->IsRounded();
  }

  DECLARE_INSTRUCTION(VecHalvingAdd);

 private:
  // Additional packed bits.
  static constexpr size_t kFieldHAddIsUnsigned = HVecOperation::kNumberOfVectorOpPackedBits;
  static constexpr size_t kFieldHAddIsRounded = kFieldHAddIsUnsigned + 1;
  static constexpr size_t kNumberOfHAddPackedBits = kFieldHAddIsRounded + 1;
  static_assert(kNumberOfHAddPackedBits <= kMaxNumberOfPackedBits, "Too many packed fields.");

  DISALLOW_COPY_AND_ASSIGN(HVecHalvingAdd);
};

// Subtracts every component in the two vectors,
// viz. [ x1, .. , xn ] - [ y1, .. , yn ] = [ x1 - y1, .. , xn - yn ].
class HVecSub FINAL : public HVecBinaryOperation {
 public:
  HVecSub(ArenaAllocator* allocator,
          HInstruction* left,
          HInstruction* right,
          DataType::Type packed_type,
          size_t vector_length,
          uint32_t dex_pc)
      : HVecBinaryOperation(allocator, left, right, packed_type, vector_length, dex_pc) {
    DCHECK(HasConsistentPackedTypes(left, packed_type));
    DCHECK(HasConsistentPackedTypes(right, packed_type));
  }

  bool CanBeMoved() const OVERRIDE { return true; }

  DECLARE_INSTRUCTION(VecSub);

 private:
  DISALLOW_COPY_AND_ASSIGN(HVecSub);
};

// Multiplies every component in the two vectors,
// viz. [ x1, .. , xn ] * [ y1, .. , yn ] = [ x1 * y1, .. , xn * yn ].
class HVecMul FINAL : public HVecBinaryOperation {
 public:
  HVecMul(ArenaAllocator* allocator,
          HInstruction* left,
          HInstruction* right,
          DataType::Type packed_type,
          size_t vector_length,
          uint32_t dex_pc)
      : HVecBinaryOperation(allocator, left, right, packed_type, vector_length, dex_pc) {
    DCHECK(HasConsistentPackedTypes(left, packed_type));
    DCHECK(HasConsistentPackedTypes(right, packed_type));
  }

  bool CanBeMoved() const OVERRIDE { return true; }

  DECLARE_INSTRUCTION(VecMul);

 private:
  DISALLOW_COPY_AND_ASSIGN(HVecMul);
};

// Divides every component in the two vectors,
// viz. [ x1, .. , xn ] / [ y1, .. , yn ] = [ x1 / y1, .. , xn / yn ].
class HVecDiv FINAL : public HVecBinaryOperation {
 public:
  HVecDiv(ArenaAllocator* allocator,
          HInstruction* left,
          HInstruction* right,
          DataType::Type packed_type,
          size_t vector_length,
          uint32_t dex_pc)
      : HVecBinaryOperation(allocator, left, right, packed_type, vector_length, dex_pc) {
    DCHECK(HasConsistentPackedTypes(left, packed_type));
    DCHECK(HasConsistentPackedTypes(right, packed_type));
  }

  bool CanBeMoved() const OVERRIDE { return true; }

  DECLARE_INSTRUCTION(VecDiv);

 private:
  DISALLOW_COPY_AND_ASSIGN(HVecDiv);
};

// Takes minimum of every component in the two vectors,
// viz. MIN( [ x1, .. , xn ] , [ y1, .. , yn ]) = [ min(x1, y1), .. , min(xn, yn) ]
// for either both signed or both unsigned operands x, y.
class HVecMin FINAL : public HVecBinaryOperation {
 public:
  HVecMin(ArenaAllocator* allocator,
          HInstruction* left,
          HInstruction* right,
          DataType::Type packed_type,
          size_t vector_length,
          bool is_unsigned,
          uint32_t dex_pc)
      : HVecBinaryOperation(allocator, left, right, packed_type, vector_length, dex_pc) {
    // The `is_unsigned` flag should be used exclusively with the Int32 or Int64.
    // This flag is a temporary measure while we do not have the Uint32 and Uint64 data types.
    DCHECK(!is_unsigned ||
           packed_type == DataType::Type::kInt32 ||
           packed_type == DataType::Type::kInt64) << packed_type;
    DCHECK(HasConsistentPackedTypes(left, packed_type));
    DCHECK(HasConsistentPackedTypes(right, packed_type));
    SetPackedFlag<kFieldMinOpIsUnsigned>(is_unsigned);
  }

  bool IsUnsigned() const { return GetPackedFlag<kFieldMinOpIsUnsigned>(); }

  bool CanBeMoved() const OVERRIDE { return true; }

  bool InstructionDataEquals(const HInstruction* other) const OVERRIDE {
    DCHECK(other->IsVecMin());
    const HVecMin* o = other->AsVecMin();
    return HVecOperation::InstructionDataEquals(o) && IsUnsigned() == o->IsUnsigned();
  }

  DECLARE_INSTRUCTION(VecMin);

 private:
  // Additional packed bits.
  static constexpr size_t kFieldMinOpIsUnsigned = HVecOperation::kNumberOfVectorOpPackedBits;
  static constexpr size_t kNumberOfMinOpPackedBits = kFieldMinOpIsUnsigned + 1;
  static_assert(kNumberOfMinOpPackedBits <= kMaxNumberOfPackedBits, "Too many packed fields.");

  DISALLOW_COPY_AND_ASSIGN(HVecMin);
};

// Takes maximum of every component in the two vectors,
// viz. MAX( [ x1, .. , xn ] , [ y1, .. , yn ]) = [ max(x1, y1), .. , max(xn, yn) ]
// for either both signed or both unsigned operands x, y.
class HVecMax FINAL : public HVecBinaryOperation {
 public:
  HVecMax(ArenaAllocator* allocator,
          HInstruction* left,
          HInstruction* right,
          DataType::Type packed_type,
          size_t vector_length,
          bool is_unsigned,
          uint32_t dex_pc)
      : HVecBinaryOperation(allocator, left, right, packed_type, vector_length, dex_pc) {
    // The `is_unsigned` flag should be used exclusively with the Int32 or Int64.
    // This flag is a temporary measure while we do not have the Uint32 and Uint64 data types.
    DCHECK(!is_unsigned ||
           packed_type == DataType::Type::kInt32 ||
           packed_type == DataType::Type::kInt64) << packed_type;
    DCHECK(HasConsistentPackedTypes(left, packed_type));
    DCHECK(HasConsistentPackedTypes(right, packed_type));
    SetPackedFlag<kFieldMaxOpIsUnsigned>(is_unsigned);
  }

  bool IsUnsigned() const { return GetPackedFlag<kFieldMaxOpIsUnsigned>(); }

  bool CanBeMoved() const OVERRIDE { return true; }

  bool InstructionDataEquals(const HInstruction* other) const OVERRIDE {
    DCHECK(other->IsVecMax());
    const HVecMax* o = other->AsVecMax();
    return HVecOperation::InstructionDataEquals(o) && IsUnsigned() == o->IsUnsigned();
  }

  DECLARE_INSTRUCTION(VecMax);

 private:
  // Additional packed bits.
  static constexpr size_t kFieldMaxOpIsUnsigned = HVecOperation::kNumberOfVectorOpPackedBits;
  static constexpr size_t kNumberOfMaxOpPackedBits = kFieldMaxOpIsUnsigned + 1;
  static_assert(kNumberOfMaxOpPackedBits <= kMaxNumberOfPackedBits, "Too many packed fields.");

  DISALLOW_COPY_AND_ASSIGN(HVecMax);
};

// Bitwise-ands every component in the two vectors,
// viz. [ x1, .. , xn ] & [ y1, .. , yn ] = [ x1 & y1, .. , xn & yn ].
class HVecAnd FINAL : public HVecBinaryOperation {
 public:
  HVecAnd(ArenaAllocator* allocator,
          HInstruction* left,
          HInstruction* right,
          DataType::Type packed_type,
          size_t vector_length,
          uint32_t dex_pc)
      : HVecBinaryOperation(allocator, left, right, packed_type, vector_length, dex_pc) {
    DCHECK(left->IsVecOperation() && right->IsVecOperation());
  }

  bool CanBeMoved() const OVERRIDE { return true; }

  DECLARE_INSTRUCTION(VecAnd);

 private:
  DISALLOW_COPY_AND_ASSIGN(HVecAnd);
};

// Bitwise-and-nots every component in the two vectors,
// viz. [ x1, .. , xn ] and-not [ y1, .. , yn ] = [ ~x1 & y1, .. , ~xn & yn ].
class HVecAndNot FINAL : public HVecBinaryOperation {
 public:
  HVecAndNot(ArenaAllocator* allocator,
             HInstruction* left,
             HInstruction* right,
             DataType::Type packed_type,
             size_t vector_length,
             uint32_t dex_pc)
         : HVecBinaryOperation(allocator, left, right, packed_type, vector_length, dex_pc) {
    DCHECK(left->IsVecOperation() && right->IsVecOperation());
  }

  bool CanBeMoved() const OVERRIDE { return true; }

  DECLARE_INSTRUCTION(VecAndNot);

 private:
  DISALLOW_COPY_AND_ASSIGN(HVecAndNot);
};

// Bitwise-ors every component in the two vectors,
// viz. [ x1, .. , xn ] | [ y1, .. , yn ] = [ x1 | y1, .. , xn | yn ].
class HVecOr FINAL : public HVecBinaryOperation {
 public:
  HVecOr(ArenaAllocator* allocator,
         HInstruction* left,
         HInstruction* right,
         DataType::Type packed_type,
         size_t vector_length,
         uint32_t dex_pc)
      : HVecBinaryOperation(allocator, left, right, packed_type, vector_length, dex_pc) {
    DCHECK(left->IsVecOperation() && right->IsVecOperation());
  }

  bool CanBeMoved() const OVERRIDE { return true; }

  DECLARE_INSTRUCTION(VecOr);

 private:
  DISALLOW_COPY_AND_ASSIGN(HVecOr);
};

// Bitwise-xors every component in the two vectors,
// viz. [ x1, .. , xn ] ^ [ y1, .. , yn ] = [ x1 ^ y1, .. , xn ^ yn ].
class HVecXor FINAL : public HVecBinaryOperation {
 public:
  HVecXor(ArenaAllocator* allocator,
          HInstruction* left,
          HInstruction* right,
          DataType::Type packed_type,
          size_t vector_length,
          uint32_t dex_pc)
      : HVecBinaryOperation(allocator, left, right, packed_type, vector_length, dex_pc) {
    DCHECK(left->IsVecOperation() && right->IsVecOperation());
  }

  bool CanBeMoved() const OVERRIDE { return true; }

  DECLARE_INSTRUCTION(VecXor);

 private:
  DISALLOW_COPY_AND_ASSIGN(HVecXor);
};

// Logically shifts every component in the vector left by the given distance,
// viz. [ x1, .. , xn ] << d = [ x1 << d, .. , xn << d ].
class HVecShl FINAL : public HVecBinaryOperation {
 public:
  HVecShl(ArenaAllocator* allocator,
          HInstruction* left,
          HInstruction* right,
          DataType::Type packed_type,
          size_t vector_length,
          uint32_t dex_pc)
      : HVecBinaryOperation(allocator, left, right, packed_type, vector_length, dex_pc) {
    DCHECK(HasConsistentPackedTypes(left, packed_type));
  }

  bool CanBeMoved() const OVERRIDE { return true; }

  DECLARE_INSTRUCTION(VecShl);

 private:
  DISALLOW_COPY_AND_ASSIGN(HVecShl);
};

// Arithmetically shifts every component in the vector right by the given distance,
// viz. [ x1, .. , xn ] >> d = [ x1 >> d, .. , xn >> d ].
class HVecShr FINAL : public HVecBinaryOperation {
 public:
  HVecShr(ArenaAllocator* allocator,
          HInstruction* left,
          HInstruction* right,
          DataType::Type packed_type,
          size_t vector_length,
          uint32_t dex_pc)
      : HVecBinaryOperation(allocator, left, right, packed_type, vector_length, dex_pc) {
    DCHECK(HasConsistentPackedTypes(left, packed_type));
  }

  bool CanBeMoved() const OVERRIDE { return true; }

  DECLARE_INSTRUCTION(VecShr);

 private:
  DISALLOW_COPY_AND_ASSIGN(HVecShr);
};

// Logically shifts every component in the vector right by the given distance,
// viz. [ x1, .. , xn ] >>> d = [ x1 >>> d, .. , xn >>> d ].
class HVecUShr FINAL : public HVecBinaryOperation {
 public:
  HVecUShr(ArenaAllocator* allocator,
           HInstruction* left,
           HInstruction* right,
           DataType::Type packed_type,
           size_t vector_length,
           uint32_t dex_pc)
      : HVecBinaryOperation(allocator, left, right, packed_type, vector_length, dex_pc) {
    DCHECK(HasConsistentPackedTypes(left, packed_type));
  }

  bool CanBeMoved() const OVERRIDE { return true; }

  DECLARE_INSTRUCTION(VecUShr);

 private:
  DISALLOW_COPY_AND_ASSIGN(HVecUShr);
};

//
// Definitions of concrete miscellaneous vector operations in HIR.
//

// Assigns the given scalar elements to a vector,
// viz. set( array(x1, .. , xn) ) = [ x1, .. ,            xn ] if n == m,
//      set( array(x1, .. , xm) ) = [ x1, .. , xm, 0, .. , 0 ] if m <  n.
class HVecSetScalars FINAL : public HVecOperation {
 public:
  HVecSetScalars(ArenaAllocator* allocator,
                 HInstruction* scalars[],
                 DataType::Type packed_type,
                 size_t vector_length,
                 size_t number_of_scalars,
                 uint32_t dex_pc)
      : HVecOperation(allocator,
                      packed_type,
                      SideEffects::None(),
                      number_of_scalars,
                      vector_length,
                      dex_pc) {
    for (size_t i = 0; i < number_of_scalars; i++) {
      DCHECK(!scalars[i]->IsVecOperation() || scalars[i]->IsVecExtractScalar());
      SetRawInputAt(0, scalars[i]);
    }
  }

  // Setting scalars needs to stay in place, since SIMD registers are not
  // kept alive across vector loop boundaries (yet).
  bool CanBeMoved() const OVERRIDE { return false; }

  DECLARE_INSTRUCTION(VecSetScalars);

 private:
  DISALLOW_COPY_AND_ASSIGN(HVecSetScalars);
};

// Multiplies every component in the two vectors, adds the result vector to the accumulator vector,
// viz. [ a1, .. , an ] + [ x1, .. , xn ] * [ y1, .. , yn ] = [ a1 + x1 * y1, .. , an + xn * yn ].
class HVecMultiplyAccumulate FINAL : public HVecOperation {
 public:
  HVecMultiplyAccumulate(ArenaAllocator* allocator,
                         InstructionKind op,
                         HInstruction* accumulator,
                         HInstruction* mul_left,
                         HInstruction* mul_right,
                         DataType::Type packed_type,
                         size_t vector_length,
                         uint32_t dex_pc)
      : HVecOperation(allocator,
                      packed_type,
                      SideEffects::None(),
                      /* number_of_inputs */ 3,
                      vector_length,
                      dex_pc),
        op_kind_(op) {
    DCHECK(op == InstructionKind::kAdd || op == InstructionKind::kSub);
    DCHECK(HasConsistentPackedTypes(accumulator, packed_type));
    DCHECK(HasConsistentPackedTypes(mul_left, packed_type));
    DCHECK(HasConsistentPackedTypes(mul_right, packed_type));
    SetRawInputAt(0, accumulator);
    SetRawInputAt(1, mul_left);
    SetRawInputAt(2, mul_right);
  }

  bool CanBeMoved() const OVERRIDE { return true; }

  bool InstructionDataEquals(const HInstruction* other) const OVERRIDE {
    DCHECK(other->IsVecMultiplyAccumulate());
    const HVecMultiplyAccumulate* o = other->AsVecMultiplyAccumulate();
    return HVecOperation::InstructionDataEquals(o) && GetOpKind() == o->GetOpKind();
  }

  InstructionKind GetOpKind() const { return op_kind_; }

  DECLARE_INSTRUCTION(VecMultiplyAccumulate);

 private:
  // Indicates if this is a MADD or MSUB.
  const InstructionKind op_kind_;

  DISALLOW_COPY_AND_ASSIGN(HVecMultiplyAccumulate);
};

// Takes the absolute difference of two vectors, and adds the results to
// same-precision or wider-precision components in the accumulator,
// viz. SAD([ a1, .. , am ], [ x1, .. , xn ], [ y1, .. , yn ]) =
//          [ a1 + sum abs(xi-yi), .. , am + sum abs(xj-yj) ],
//      for m <= n, non-overlapping sums, and signed operands x, y.
class HVecSADAccumulate FINAL : public HVecOperation {
 public:
  HVecSADAccumulate(ArenaAllocator* allocator,
                    HInstruction* accumulator,
                    HInstruction* sad_left,
                    HInstruction* sad_right,
                    DataType::Type packed_type,
                    size_t vector_length,
                    uint32_t dex_pc)
      : HVecOperation(allocator,
                      packed_type,
                      SideEffects::None(),
                      /* number_of_inputs */ 3,
                      vector_length,
                      dex_pc) {
    DCHECK(HasConsistentPackedTypes(accumulator, packed_type));
    DCHECK(sad_left->IsVecOperation());
    DCHECK(sad_right->IsVecOperation());
    DCHECK_EQ(ToSignedType(sad_left->AsVecOperation()->GetPackedType()),
              ToSignedType(sad_right->AsVecOperation()->GetPackedType()));
    SetRawInputAt(0, accumulator);
    SetRawInputAt(1, sad_left);
    SetRawInputAt(2, sad_right);
  }

  DECLARE_INSTRUCTION(VecSADAccumulate);

 private:
  DISALLOW_COPY_AND_ASSIGN(HVecSADAccumulate);
};

// Loads a vector from memory, viz. load(mem, 1)
// yield the vector [ mem(1), .. , mem(n) ].
class HVecLoad FINAL : public HVecMemoryOperation {
 public:
  HVecLoad(ArenaAllocator* allocator,
           HInstruction* base,
           HInstruction* index,
           DataType::Type packed_type,
           SideEffects side_effects,
           size_t vector_length,
           bool is_string_char_at,
           uint32_t dex_pc)
      : HVecMemoryOperation(allocator,
                            packed_type,
                            side_effects,
                            /* number_of_inputs */ 2,
                            vector_length,
                            dex_pc) {
    SetRawInputAt(0, base);
    SetRawInputAt(1, index);
    SetPackedFlag<kFieldIsStringCharAt>(is_string_char_at);
  }

  bool IsStringCharAt() const { return GetPackedFlag<kFieldIsStringCharAt>(); }

  bool CanBeMoved() const OVERRIDE { return true; }

  bool InstructionDataEquals(const HInstruction* other) const OVERRIDE {
    DCHECK(other->IsVecLoad());
    const HVecLoad* o = other->AsVecLoad();
    return HVecMemoryOperation::InstructionDataEquals(o) && IsStringCharAt() == o->IsStringCharAt();
  }

  DECLARE_INSTRUCTION(VecLoad);

 private:
  // Additional packed bits.
  static constexpr size_t kFieldIsStringCharAt = HVecOperation::kNumberOfVectorOpPackedBits;
  static constexpr size_t kNumberOfVecLoadPackedBits = kFieldIsStringCharAt + 1;
  static_assert(kNumberOfVecLoadPackedBits <= kMaxNumberOfPackedBits, "Too many packed fields.");

  DISALLOW_COPY_AND_ASSIGN(HVecLoad);
};

// Stores a vector to memory, viz. store(m, 1, [x1, .. , xn] )
// sets mem(1) = x1, .. , mem(n) = xn.
class HVecStore FINAL : public HVecMemoryOperation {
 public:
  HVecStore(ArenaAllocator* allocator,
            HInstruction* base,
            HInstruction* index,
            HInstruction* value,
            DataType::Type packed_type,
            SideEffects side_effects,
            size_t vector_length,
            uint32_t dex_pc)
      : HVecMemoryOperation(allocator,
                            packed_type,
                            side_effects,
                            /* number_of_inputs */ 3,
                            vector_length,
                            dex_pc) {
    DCHECK(HasConsistentPackedTypes(value, packed_type));
    SetRawInputAt(0, base);
    SetRawInputAt(1, index);
    SetRawInputAt(2, value);
  }

  // A store needs to stay in place.
  bool CanBeMoved() const OVERRIDE { return false; }

  DECLARE_INSTRUCTION(VecStore);

 private:
  DISALLOW_COPY_AND_ASSIGN(HVecStore);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_NODES_VECTOR_H_
