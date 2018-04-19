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

#include "flow_analysis.h"

#include "dex/bytecode_utils.h"
#include "dex/code_item_accessors-inl.h"
#include "dex/dex_instruction-inl.h"
#include "dex/dex_file-inl.h"
#include "dex/dex_file_exception_helpers.h"
#include "resolver.h"
#include "veridex.h"

namespace art {


void VeriFlowAnalysis::SetAsBranchTarget(uint32_t dex_pc) {
  if (dex_registers_[dex_pc] == nullptr) {
    dex_registers_[dex_pc].reset(
        new std::vector<RegisterValue>(code_item_accessor_.RegistersSize()));
  }
}

bool VeriFlowAnalysis::IsBranchTarget(uint32_t dex_pc) {
  return dex_registers_[dex_pc] != nullptr;
}

bool VeriFlowAnalysis::MergeRegisterValues(uint32_t dex_pc) {
  // TODO: Do the merging. Right now, just return that we should continue
  // the iteration if the instruction has not been visited.
  if (!instruction_infos_[dex_pc].has_been_visited) {
    dex_registers_[dex_pc]->assign(current_registers_.begin(), current_registers_.end());
    return true;
  }
  return false;
}

void VeriFlowAnalysis::SetVisited(uint32_t dex_pc) {
  instruction_infos_[dex_pc].has_been_visited = true;
}

void VeriFlowAnalysis::FindBranches() {
  SetAsBranchTarget(0);

  if (code_item_accessor_.TriesSize() != 0) {
    // TODO: We need to mark the range of dex pcs as flowing in the handlers.
    /*
    for (const DexFile::TryItem& try_item : code_item_accessor_.TryItems()) {
      uint32_t dex_pc_start = try_item.start_addr_;
      uint32_t dex_pc_end = dex_pc_start + try_item.insn_count_;
    }
    */

    // Create branch targets for exception handlers.
    const uint8_t* handlers_ptr = code_item_accessor_.GetCatchHandlerData();
    uint32_t handlers_size = DecodeUnsignedLeb128(&handlers_ptr);
    for (uint32_t idx = 0; idx < handlers_size; ++idx) {
      CatchHandlerIterator iterator(handlers_ptr);
      for (; iterator.HasNext(); iterator.Next()) {
        SetAsBranchTarget(iterator.GetHandlerAddress());
      }
      handlers_ptr = iterator.EndDataPointer();
    }
  }

  // Iterate over all instructions and find branching instructions.
  for (const DexInstructionPcPair& pair : code_item_accessor_) {
    const uint32_t dex_pc = pair.DexPc();
    const Instruction& instruction = pair.Inst();

    if (instruction.IsBranch()) {
      SetAsBranchTarget(dex_pc + instruction.GetTargetOffset());
    } else if (instruction.IsSwitch()) {
      DexSwitchTable table(instruction, dex_pc);
      for (DexSwitchTableIterator s_it(table); !s_it.Done(); s_it.Advance()) {
        SetAsBranchTarget(dex_pc + s_it.CurrentTargetOffset());
        if (table.ShouldBuildDecisionTree() && !s_it.IsLast()) {
          SetAsBranchTarget(s_it.GetDexPcForCurrentIndex());
        }
      }
    }
  }
}

void VeriFlowAnalysis::UpdateRegister(uint32_t dex_register,
                                      RegisterSource kind,
                                      VeriClass* cls,
                                      uint32_t source_id) {
  current_registers_[dex_register] = RegisterValue(
      kind, DexFileReference(&resolver_->GetDexFile(), source_id), cls);
}

void VeriFlowAnalysis::UpdateRegister(uint32_t dex_register, const RegisterValue& value) {
  current_registers_[dex_register] = value;
}

void VeriFlowAnalysis::UpdateRegister(uint32_t dex_register, const VeriClass* cls) {
  current_registers_[dex_register] =
      RegisterValue(RegisterSource::kNone, DexFileReference(nullptr, 0), cls);
}

const RegisterValue& VeriFlowAnalysis::GetRegister(uint32_t dex_register) {
  return current_registers_[dex_register];
}

RegisterValue VeriFlowAnalysis::GetReturnType(uint32_t method_index) {
  const DexFile& dex_file = resolver_->GetDexFile();
  const DexFile::MethodId& method_id = dex_file.GetMethodId(method_index);
  const DexFile::ProtoId& proto_id = dex_file.GetMethodPrototype(method_id);
  VeriClass* cls = resolver_->GetVeriClass(proto_id.return_type_idx_);
  return RegisterValue(RegisterSource::kMethod, DexFileReference(&dex_file, method_index), cls);
}

RegisterValue VeriFlowAnalysis::GetFieldType(uint32_t field_index) {
  const DexFile& dex_file = resolver_->GetDexFile();
  const DexFile::FieldId& field_id = dex_file.GetFieldId(field_index);
  VeriClass* cls = resolver_->GetVeriClass(field_id.type_idx_);
  return RegisterValue(RegisterSource::kField, DexFileReference(&dex_file, field_index), cls);
}

void VeriFlowAnalysis::AnalyzeCode() {
  std::vector<uint32_t> work_list;
  work_list.push_back(0);
  // Iterate over the code.
  // When visiting unconditional branches (goto), move to that instruction.
  // When visiting conditional branches, move to one destination, and put the other
  // in the worklist.
  while (!work_list.empty()) {
    uint32_t dex_pc = work_list.back();
    work_list.pop_back();
    CHECK(IsBranchTarget(dex_pc));
    current_registers_ = *dex_registers_[dex_pc].get();
    while (true) {
      const uint16_t* insns = code_item_accessor_.Insns() + dex_pc;
      const Instruction& inst = *Instruction::At(insns);
      ProcessDexInstruction(inst);
      SetVisited(dex_pc);

      int opcode_flags = Instruction::FlagsOf(inst.Opcode());
      if ((opcode_flags & Instruction::kContinue) != 0) {
        if ((opcode_flags & Instruction::kBranch) != 0) {
          uint32_t branch_dex_pc = dex_pc + inst.GetTargetOffset();
          if (MergeRegisterValues(branch_dex_pc)) {
            work_list.push_back(branch_dex_pc);
          }
        }
        dex_pc += inst.SizeInCodeUnits();
      } else if ((opcode_flags & Instruction::kBranch) != 0) {
        dex_pc += inst.GetTargetOffset();
        DCHECK(IsBranchTarget(dex_pc));
      } else {
        break;
      }

      if (IsBranchTarget(dex_pc)) {
        if (MergeRegisterValues(dex_pc)) {
          current_registers_ = *dex_registers_[dex_pc].get();
        } else {
          break;
        }
      }
    }
  }
}

void VeriFlowAnalysis::ProcessDexInstruction(const Instruction& instruction) {
  switch (instruction.Opcode()) {
    case Instruction::CONST_4:
    case Instruction::CONST_16:
    case Instruction::CONST:
    case Instruction::CONST_HIGH16: {
      int32_t register_index = instruction.VRegA();
      UpdateRegister(register_index, VeriClass::integer_);
      break;
    }

    case Instruction::CONST_WIDE_16:
    case Instruction::CONST_WIDE_32:
    case Instruction::CONST_WIDE:
    case Instruction::CONST_WIDE_HIGH16: {
      int32_t register_index = instruction.VRegA();
      UpdateRegister(register_index, VeriClass::long_);
      break;
    }

    case Instruction::MOVE:
    case Instruction::MOVE_FROM16:
    case Instruction::MOVE_16: {
      UpdateRegister(instruction.VRegA(), GetRegister(instruction.VRegB()));
      break;
    }

    case Instruction::MOVE_WIDE:
    case Instruction::MOVE_WIDE_FROM16:
    case Instruction::MOVE_WIDE_16: {
      UpdateRegister(instruction.VRegA(), GetRegister(instruction.VRegB()));
      break;
    }

    case Instruction::MOVE_OBJECT:
    case Instruction::MOVE_OBJECT_16:
    case Instruction::MOVE_OBJECT_FROM16: {
      UpdateRegister(instruction.VRegA(), GetRegister(instruction.VRegB()));
      break;
    }
    case Instruction::CONST_CLASS: {
      UpdateRegister(instruction.VRegA_21c(),
                     RegisterSource::kClass,
                     VeriClass::class_,
                     instruction.VRegB_21c());
      break;
    }
    case Instruction::CONST_STRING: {
      UpdateRegister(instruction.VRegA_21c(),
                     RegisterSource::kString,
                     VeriClass::string_,
                     instruction.VRegB_21c());
      break;
    }

    case Instruction::CONST_STRING_JUMBO: {
      UpdateRegister(instruction.VRegA_31c(),
                     RegisterSource::kString,
                     VeriClass::string_,
                     instruction.VRegB_31c());
      break;
    }
    case Instruction::INVOKE_DIRECT:
    case Instruction::INVOKE_INTERFACE:
    case Instruction::INVOKE_STATIC:
    case Instruction::INVOKE_SUPER:
    case Instruction::INVOKE_VIRTUAL: {
      VeriMethod method = resolver_->GetMethod(instruction.VRegB_35c());
      uint32_t args[5];
      instruction.GetVarArgs(args);
      if (method == VeriClass::forName_) {
        RegisterValue value = GetRegister(args[0]);
        last_result_ = RegisterValue(
            value.GetSource(), value.GetDexFileReference(), VeriClass::class_);
      } else if (IsGetField(method)) {
        RegisterValue cls = GetRegister(args[0]);
        RegisterValue name = GetRegister(args[1]);
        field_uses_.push_back(std::make_pair(cls, name));
        last_result_ = GetReturnType(instruction.VRegB_35c());
      } else if (IsGetMethod(method)) {
        RegisterValue cls = GetRegister(args[0]);
        RegisterValue name = GetRegister(args[1]);
        method_uses_.push_back(std::make_pair(cls, name));
        last_result_ = GetReturnType(instruction.VRegB_35c());
      } else if (method == VeriClass::getClass_) {
        RegisterValue obj = GetRegister(args[0]);
        last_result_ = RegisterValue(
            obj.GetSource(), obj.GetDexFileReference(), VeriClass::class_);
      } else if (method == VeriClass::loadClass_) {
        RegisterValue value = GetRegister(args[1]);
        last_result_ = RegisterValue(
            value.GetSource(), value.GetDexFileReference(), VeriClass::class_);
      } else {
        last_result_ = GetReturnType(instruction.VRegB_35c());
      }
      break;
    }

    case Instruction::INVOKE_DIRECT_RANGE:
    case Instruction::INVOKE_INTERFACE_RANGE:
    case Instruction::INVOKE_STATIC_RANGE:
    case Instruction::INVOKE_SUPER_RANGE:
    case Instruction::INVOKE_VIRTUAL_RANGE: {
      last_result_ = GetReturnType(instruction.VRegB_3rc());
      break;
    }

    case Instruction::MOVE_RESULT:
    case Instruction::MOVE_RESULT_WIDE:
    case Instruction::MOVE_RESULT_OBJECT: {
      UpdateRegister(instruction.VRegA(), last_result_);
      break;
    }
    case Instruction::RETURN_VOID:
    case Instruction::RETURN_OBJECT:
    case Instruction::RETURN_WIDE:
    case Instruction::RETURN: {
      break;
    }
    #define IF_XX(cond) \
    case Instruction::IF_##cond: break; \
    case Instruction::IF_##cond##Z: break

    IF_XX(EQ);
    IF_XX(NE);
    IF_XX(LT);
    IF_XX(LE);
    IF_XX(GT);
    IF_XX(GE);

    case Instruction::GOTO:
    case Instruction::GOTO_16:
    case Instruction::GOTO_32: {
      break;
    }
    case Instruction::INVOKE_POLYMORPHIC: {
      // TODO
      break;
    }

    case Instruction::INVOKE_POLYMORPHIC_RANGE: {
      // TODO
      break;
    }

    case Instruction::NEG_INT:
    case Instruction::NEG_LONG:
    case Instruction::NEG_FLOAT:
    case Instruction::NEG_DOUBLE:
    case Instruction::NOT_INT:
    case Instruction::NOT_LONG: {
      UpdateRegister(instruction.VRegA(), VeriClass::integer_);
      break;
    }

    case Instruction::INT_TO_LONG:
    case Instruction::INT_TO_FLOAT:
    case Instruction::INT_TO_DOUBLE:
    case Instruction::LONG_TO_INT:
    case Instruction::LONG_TO_FLOAT:
    case Instruction::LONG_TO_DOUBLE:
    case Instruction::FLOAT_TO_INT:
    case Instruction::FLOAT_TO_LONG:
    case Instruction::FLOAT_TO_DOUBLE:
    case Instruction::DOUBLE_TO_INT:
    case Instruction::DOUBLE_TO_LONG:
    case Instruction::DOUBLE_TO_FLOAT:
    case Instruction::INT_TO_BYTE:
    case Instruction::INT_TO_SHORT:
    case Instruction::INT_TO_CHAR: {
      UpdateRegister(instruction.VRegA(), VeriClass::integer_);
      break;
    }

    case Instruction::ADD_INT:
    case Instruction::ADD_LONG:
    case Instruction::ADD_DOUBLE:
    case Instruction::ADD_FLOAT:
    case Instruction::SUB_INT:
    case Instruction::SUB_LONG:
    case Instruction::SUB_FLOAT:
    case Instruction::SUB_DOUBLE:
    case Instruction::MUL_INT:
    case Instruction::MUL_LONG:
    case Instruction::MUL_FLOAT:
    case Instruction::MUL_DOUBLE:
    case Instruction::DIV_INT:
    case Instruction::DIV_LONG:
    case Instruction::DIV_FLOAT:
    case Instruction::DIV_DOUBLE:
    case Instruction::REM_INT:
    case Instruction::REM_LONG:
    case Instruction::REM_FLOAT:
    case Instruction::REM_DOUBLE:
    case Instruction::AND_INT:
    case Instruction::AND_LONG:
    case Instruction::SHL_INT:
    case Instruction::SHL_LONG:
    case Instruction::SHR_INT:
    case Instruction::SHR_LONG:
    case Instruction::USHR_INT:
    case Instruction::USHR_LONG:
    case Instruction::OR_INT:
    case Instruction::OR_LONG:
    case Instruction::XOR_INT:
    case Instruction::XOR_LONG: {
      UpdateRegister(instruction.VRegA(), VeriClass::integer_);
      break;
    }

    case Instruction::ADD_INT_2ADDR:
    case Instruction::ADD_LONG_2ADDR:
    case Instruction::ADD_DOUBLE_2ADDR:
    case Instruction::ADD_FLOAT_2ADDR:
    case Instruction::SUB_INT_2ADDR:
    case Instruction::SUB_LONG_2ADDR:
    case Instruction::SUB_FLOAT_2ADDR:
    case Instruction::SUB_DOUBLE_2ADDR:
    case Instruction::MUL_INT_2ADDR:
    case Instruction::MUL_LONG_2ADDR:
    case Instruction::MUL_FLOAT_2ADDR:
    case Instruction::MUL_DOUBLE_2ADDR:
    case Instruction::DIV_INT_2ADDR:
    case Instruction::DIV_LONG_2ADDR:
    case Instruction::REM_INT_2ADDR:
    case Instruction::REM_LONG_2ADDR:
    case Instruction::REM_FLOAT_2ADDR:
    case Instruction::REM_DOUBLE_2ADDR:
    case Instruction::SHL_INT_2ADDR:
    case Instruction::SHL_LONG_2ADDR:
    case Instruction::SHR_INT_2ADDR:
    case Instruction::SHR_LONG_2ADDR:
    case Instruction::USHR_INT_2ADDR:
    case Instruction::USHR_LONG_2ADDR:
    case Instruction::DIV_FLOAT_2ADDR:
    case Instruction::DIV_DOUBLE_2ADDR:
    case Instruction::AND_INT_2ADDR:
    case Instruction::AND_LONG_2ADDR:
    case Instruction::OR_INT_2ADDR:
    case Instruction::OR_LONG_2ADDR:
    case Instruction::XOR_INT_2ADDR:
    case Instruction::XOR_LONG_2ADDR: {
      UpdateRegister(instruction.VRegA(), VeriClass::integer_);
      break;
    }

    case Instruction::ADD_INT_LIT16:
    case Instruction::AND_INT_LIT16:
    case Instruction::OR_INT_LIT16:
    case Instruction::XOR_INT_LIT16:
    case Instruction::RSUB_INT:
    case Instruction::MUL_INT_LIT16:
    case Instruction::DIV_INT_LIT16:
    case Instruction::REM_INT_LIT16: {
      UpdateRegister(instruction.VRegA(), VeriClass::integer_);
      break;
    }

    case Instruction::ADD_INT_LIT8:
    case Instruction::AND_INT_LIT8:
    case Instruction::OR_INT_LIT8:
    case Instruction::XOR_INT_LIT8:
    case Instruction::RSUB_INT_LIT8:
    case Instruction::MUL_INT_LIT8:
    case Instruction::DIV_INT_LIT8:
    case Instruction::REM_INT_LIT8:
    case Instruction::SHL_INT_LIT8:
    case Instruction::SHR_INT_LIT8: {
    case Instruction::USHR_INT_LIT8: {
      UpdateRegister(instruction.VRegA(), VeriClass::integer_);
      break;
    }

    case Instruction::NEW_INSTANCE: {
      VeriClass* cls = resolver_->GetVeriClass(dex::TypeIndex(instruction.VRegB_21c()));
      UpdateRegister(instruction.VRegA(), cls);
      break;
    }

    case Instruction::NEW_ARRAY: {
      dex::TypeIndex type_index(instruction.VRegC_22c());
      VeriClass* cls = resolver_->GetVeriClass(type_index);
      UpdateRegister(instruction.VRegA_22c(), cls);
      break;
    }

    case Instruction::FILLED_NEW_ARRAY: {
      dex::TypeIndex type_index(instruction.VRegB_35c());
      VeriClass* cls = resolver_->GetVeriClass(type_index);
      UpdateRegister(instruction.VRegA_22c(), cls);
      break;
    }

    case Instruction::FILLED_NEW_ARRAY_RANGE: {
      dex::TypeIndex type_index(instruction.VRegB_3rc());
      uint32_t register_index = instruction.VRegC_3rc();
      VeriClass* cls = resolver_->GetVeriClass(type_index);
      UpdateRegister(register_index, cls);
      break;
    }

    case Instruction::FILL_ARRAY_DATA: {
      break;
    }

    case Instruction::CMP_LONG:
    case Instruction::CMPG_FLOAT:
    case Instruction::CMPG_DOUBLE:
    case Instruction::CMPL_FLOAT:
    case Instruction::CMPL_DOUBLE:
      UpdateRegister(instruction.VRegA(), VeriClass::integer_);
      break;
    }

    case Instruction::NOP:
      break;

    case Instruction::IGET:
    case Instruction::IGET_WIDE:
    case Instruction::IGET_OBJECT:
    case Instruction::IGET_BOOLEAN:
    case Instruction::IGET_BYTE:
    case Instruction::IGET_CHAR:
    case Instruction::IGET_SHORT: {
      UpdateRegister(instruction.VRegA_22c(), GetFieldType(instruction.VRegC_22c()));
      break;
    }

    case Instruction::IPUT:
    case Instruction::IPUT_WIDE:
    case Instruction::IPUT_OBJECT:
    case Instruction::IPUT_BOOLEAN:
    case Instruction::IPUT_BYTE:
    case Instruction::IPUT_CHAR:
    case Instruction::IPUT_SHORT: {
      break;
    }

    case Instruction::SGET:
    case Instruction::SGET_WIDE:
    case Instruction::SGET_OBJECT:
    case Instruction::SGET_BOOLEAN:
    case Instruction::SGET_BYTE:
    case Instruction::SGET_CHAR:
    case Instruction::SGET_SHORT: {
      UpdateRegister(instruction.VRegA_22c(), GetFieldType(instruction.VRegC_22c()));
      break;
    }

    case Instruction::SPUT:
    case Instruction::SPUT_WIDE:
    case Instruction::SPUT_OBJECT:
    case Instruction::SPUT_BOOLEAN:
    case Instruction::SPUT_BYTE:
    case Instruction::SPUT_CHAR:
    case Instruction::SPUT_SHORT: {
      break;
    }

#define ARRAY_XX(kind, anticipated_type)                                          \
    case Instruction::AGET##kind: {                                               \
      UpdateRegister(instruction.VRegA_23x(), anticipated_type);                  \
      break;                                                                      \
    }                                                                             \
    case Instruction::APUT##kind: {                                               \
      break;                                                                      \
    }

    ARRAY_XX(, VeriClass::integer_);
    ARRAY_XX(_WIDE, VeriClass::long_);
    ARRAY_XX(_BOOLEAN, VeriClass::boolean_);
    ARRAY_XX(_BYTE, VeriClass::byte_);
    ARRAY_XX(_CHAR, VeriClass::char_);
    ARRAY_XX(_SHORT, VeriClass::short_);

    case Instruction::AGET_OBJECT: {
      // TODO: take the component type.
      UpdateRegister(instruction.VRegA_23x(), VeriClass::object_);
      break;
    }

    case Instruction::APUT_OBJECT: {
      break;
    }

    case Instruction::ARRAY_LENGTH: {
      UpdateRegister(instruction.VRegA_12x(), VeriClass::integer_);
      break;
    }

    case Instruction::MOVE_EXCEPTION: {
      UpdateRegister(instruction.VRegA_11x(), VeriClass::throwable_);
      break;
    }

    case Instruction::THROW: {
      break;
    }

    case Instruction::INSTANCE_OF: {
      uint8_t destination = instruction.VRegA_22c();
      UpdateRegister(destination, VeriClass::boolean_);
      break;
    }

    case Instruction::CHECK_CAST: {
      uint8_t reference = instruction.VRegA_21c();
      dex::TypeIndex type_index(instruction.VRegB_21c());
      UpdateRegister(reference, resolver_->GetVeriClass(type_index));
      break;
    }

    case Instruction::MONITOR_ENTER:
    case Instruction::MONITOR_EXIT: {
      break;
    }

    case Instruction::SPARSE_SWITCH:
    case Instruction::PACKED_SWITCH:
      break;

    default:
      break;
  }
}

void VeriFlowAnalysis::Run() {
  FindBranches();
  AnalyzeCode();
}

}  // namespace art
