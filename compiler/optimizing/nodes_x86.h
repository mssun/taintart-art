/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef ART_COMPILER_OPTIMIZING_NODES_X86_H_
#define ART_COMPILER_OPTIMIZING_NODES_X86_H_

namespace art {

// Compute the address of the method for X86 Constant area support.
class HX86ComputeBaseMethodAddress final : public HExpression<0> {
 public:
  // Treat the value as an int32_t, but it is really a 32 bit native pointer.
  HX86ComputeBaseMethodAddress()
      : HExpression(kX86ComputeBaseMethodAddress,
                    DataType::Type::kInt32,
                    SideEffects::None(),
                    kNoDexPc) {
  }

  bool CanBeMoved() const override { return true; }

  DECLARE_INSTRUCTION(X86ComputeBaseMethodAddress);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(X86ComputeBaseMethodAddress);
};

// Load a constant value from the constant table.
class HX86LoadFromConstantTable final : public HExpression<2> {
 public:
  HX86LoadFromConstantTable(HX86ComputeBaseMethodAddress* method_base,
                            HConstant* constant)
      : HExpression(kX86LoadFromConstantTable,
                    constant->GetType(),
                    SideEffects::None(),
                    kNoDexPc) {
    SetRawInputAt(0, method_base);
    SetRawInputAt(1, constant);
  }

  HX86ComputeBaseMethodAddress* GetBaseMethodAddress() const {
    return InputAt(0)->AsX86ComputeBaseMethodAddress();
  }

  HConstant* GetConstant() const {
    return InputAt(1)->AsConstant();
  }

  DECLARE_INSTRUCTION(X86LoadFromConstantTable);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(X86LoadFromConstantTable);
};

// Version of HNeg with access to the constant table for FP types.
class HX86FPNeg final : public HExpression<2> {
 public:
  HX86FPNeg(DataType::Type result_type,
            HInstruction* input,
            HX86ComputeBaseMethodAddress* method_base,
            uint32_t dex_pc)
      : HExpression(kX86FPNeg, result_type, SideEffects::None(), dex_pc) {
    DCHECK(DataType::IsFloatingPointType(result_type));
    SetRawInputAt(0, input);
    SetRawInputAt(1, method_base);
  }

  HX86ComputeBaseMethodAddress* GetBaseMethodAddress() const {
    return InputAt(1)->AsX86ComputeBaseMethodAddress();
  }

  DECLARE_INSTRUCTION(X86FPNeg);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(X86FPNeg);
};

// X86 version of HPackedSwitch that holds a pointer to the base method address.
class HX86PackedSwitch final : public HExpression<2> {
 public:
  HX86PackedSwitch(int32_t start_value,
                   int32_t num_entries,
                   HInstruction* input,
                   HX86ComputeBaseMethodAddress* method_base,
                   uint32_t dex_pc)
    : HExpression(kX86PackedSwitch, SideEffects::None(), dex_pc),
      start_value_(start_value),
      num_entries_(num_entries) {
    SetRawInputAt(0, input);
    SetRawInputAt(1, method_base);
  }

  bool IsControlFlow() const override { return true; }

  int32_t GetStartValue() const { return start_value_; }

  int32_t GetNumEntries() const { return num_entries_; }

  HX86ComputeBaseMethodAddress* GetBaseMethodAddress() const {
    return InputAt(1)->AsX86ComputeBaseMethodAddress();
  }

  HBasicBlock* GetDefaultBlock() const {
    // Last entry is the default block.
    return GetBlock()->GetSuccessors()[num_entries_];
  }

  DECLARE_INSTRUCTION(X86PackedSwitch);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(X86PackedSwitch);

 private:
  const int32_t start_value_;
  const int32_t num_entries_;
};

class HX86AndNot final : public HBinaryOperation {
 public:
  HX86AndNot(DataType::Type result_type,
       HInstruction* left,
       HInstruction* right,
       uint32_t dex_pc = kNoDexPc)
      : HBinaryOperation(kX86AndNot, result_type, left, right, SideEffects::None(), dex_pc) {
  }

  bool IsCommutative() const override { return false; }

  template <typename T> static T Compute(T x, T y) { return ~x & y; }

  HConstant* Evaluate(HIntConstant* x, HIntConstant* y) const override {
    return GetBlock()->GetGraph()->GetIntConstant(
        Compute(x->GetValue(), y->GetValue()), GetDexPc());
  }
  HConstant* Evaluate(HLongConstant* x, HLongConstant* y) const override {
    return GetBlock()->GetGraph()->GetLongConstant(
        Compute(x->GetValue(), y->GetValue()), GetDexPc());
  }
  HConstant* Evaluate(HFloatConstant* x ATTRIBUTE_UNUSED,
                      HFloatConstant* y ATTRIBUTE_UNUSED) const override {
    LOG(FATAL) << DebugName() << " is not defined for float values";
    UNREACHABLE();
  }
  HConstant* Evaluate(HDoubleConstant* x ATTRIBUTE_UNUSED,
                      HDoubleConstant* y ATTRIBUTE_UNUSED) const override {
    LOG(FATAL) << DebugName() << " is not defined for double values";
    UNREACHABLE();
  }

  DECLARE_INSTRUCTION(X86AndNot);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(X86AndNot);
};

class HX86MaskOrResetLeastSetBit final : public HUnaryOperation {
 public:
  HX86MaskOrResetLeastSetBit(DataType::Type result_type, InstructionKind op,
                             HInstruction* input, uint32_t dex_pc = kNoDexPc)
      : HUnaryOperation(kX86MaskOrResetLeastSetBit, result_type, input, dex_pc),
        op_kind_(op) {
    DCHECK_EQ(result_type, DataType::Kind(input->GetType()));
    DCHECK(op == HInstruction::kAnd || op == HInstruction::kXor) << op;
  }
  template <typename T>
  auto Compute(T x) const -> decltype(x & (x-1)) {
    static_assert(std::is_same<decltype(x & (x-1)), decltype(x ^(x-1))>::value,
                  "Inconsistent  bitwise types");
    switch (op_kind_) {
      case HInstruction::kAnd:
        return x & (x-1);
      case HInstruction::kXor:
        return x ^ (x-1);
      default:
        LOG(FATAL) << "Unreachable";
        UNREACHABLE();
    }
  }

  HConstant* Evaluate(HIntConstant* x) const override {
    return GetBlock()->GetGraph()->GetIntConstant(Compute(x->GetValue()), GetDexPc());
  }
  HConstant* Evaluate(HLongConstant* x) const override {
    return GetBlock()->GetGraph()->GetLongConstant(Compute(x->GetValue()), GetDexPc());
  }
  HConstant* Evaluate(HFloatConstant* x ATTRIBUTE_UNUSED) const override {
    LOG(FATAL) << DebugName() << "is not defined for float values";
    UNREACHABLE();
  }
  HConstant* Evaluate(HDoubleConstant* x ATTRIBUTE_UNUSED) const override {
    LOG(FATAL) << DebugName() << "is not defined for double values";
    UNREACHABLE();
  }
  InstructionKind GetOpKind() const { return op_kind_; }

  DECLARE_INSTRUCTION(X86MaskOrResetLeastSetBit);

 protected:
  const InstructionKind op_kind_;

  DEFAULT_COPY_CONSTRUCTOR(X86MaskOrResetLeastSetBit);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_NODES_X86_H_
