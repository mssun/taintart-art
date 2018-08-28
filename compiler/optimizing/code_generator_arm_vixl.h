/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef ART_COMPILER_OPTIMIZING_CODE_GENERATOR_ARM_VIXL_H_
#define ART_COMPILER_OPTIMIZING_CODE_GENERATOR_ARM_VIXL_H_

#include "base/enums.h"
#include "code_generator.h"
#include "common_arm.h"
#include "dex/string_reference.h"
#include "dex/type_reference.h"
#include "driver/compiler_options.h"
#include "nodes.h"
#include "parallel_move_resolver.h"
#include "utils/arm/assembler_arm_vixl.h"

// TODO(VIXL): make vixl clean wrt -Wshadow.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#include "aarch32/constants-aarch32.h"
#include "aarch32/instructions-aarch32.h"
#include "aarch32/macro-assembler-aarch32.h"
#pragma GCC diagnostic pop

namespace art {

namespace linker {
class Thumb2RelativePatcherTest;
}  // namespace linker

namespace arm {

// This constant is used as an approximate margin when emission of veneer and literal pools
// must be blocked.
static constexpr int kMaxMacroInstructionSizeInBytes =
    15 * vixl::aarch32::kMaxInstructionSizeInBytes;

static const vixl::aarch32::Register kParameterCoreRegistersVIXL[] = {
    vixl::aarch32::r1,
    vixl::aarch32::r2,
    vixl::aarch32::r3
};
static const size_t kParameterCoreRegistersLengthVIXL = arraysize(kParameterCoreRegistersVIXL);
static const vixl::aarch32::SRegister kParameterFpuRegistersVIXL[] = {
    vixl::aarch32::s0,
    vixl::aarch32::s1,
    vixl::aarch32::s2,
    vixl::aarch32::s3,
    vixl::aarch32::s4,
    vixl::aarch32::s5,
    vixl::aarch32::s6,
    vixl::aarch32::s7,
    vixl::aarch32::s8,
    vixl::aarch32::s9,
    vixl::aarch32::s10,
    vixl::aarch32::s11,
    vixl::aarch32::s12,
    vixl::aarch32::s13,
    vixl::aarch32::s14,
    vixl::aarch32::s15
};
static const size_t kParameterFpuRegistersLengthVIXL = arraysize(kParameterFpuRegistersVIXL);

static const vixl::aarch32::Register kMethodRegister = vixl::aarch32::r0;

static const vixl::aarch32::Register kCoreAlwaysSpillRegister = vixl::aarch32::r5;

// Callee saves core registers r5, r6, r7, r8 (except when emitting Baker
// read barriers, where it is used as Marking Register), r10, r11, and lr.
static const vixl::aarch32::RegisterList kCoreCalleeSaves = vixl::aarch32::RegisterList::Union(
    vixl::aarch32::RegisterList(vixl::aarch32::r5,
                                vixl::aarch32::r6,
                                vixl::aarch32::r7),
    // Do not consider r8 as a callee-save register with Baker read barriers.
    ((kEmitCompilerReadBarrier && kUseBakerReadBarrier)
         ? vixl::aarch32::RegisterList()
         : vixl::aarch32::RegisterList(vixl::aarch32::r8)),
    vixl::aarch32::RegisterList(vixl::aarch32::r10,
                                vixl::aarch32::r11,
                                vixl::aarch32::lr));

// Callee saves FP registers s16 to s31 inclusive.
static const vixl::aarch32::SRegisterList kFpuCalleeSaves =
    vixl::aarch32::SRegisterList(vixl::aarch32::s16, 16);

static const vixl::aarch32::Register kRuntimeParameterCoreRegistersVIXL[] = {
    vixl::aarch32::r0,
    vixl::aarch32::r1,
    vixl::aarch32::r2,
    vixl::aarch32::r3
};
static const size_t kRuntimeParameterCoreRegistersLengthVIXL =
    arraysize(kRuntimeParameterCoreRegistersVIXL);
static const vixl::aarch32::SRegister kRuntimeParameterFpuRegistersVIXL[] = {
    vixl::aarch32::s0,
    vixl::aarch32::s1,
    vixl::aarch32::s2,
    vixl::aarch32::s3
};
static const size_t kRuntimeParameterFpuRegistersLengthVIXL =
    arraysize(kRuntimeParameterFpuRegistersVIXL);

class LoadClassSlowPathARMVIXL;
class CodeGeneratorARMVIXL;

using VIXLInt32Literal = vixl::aarch32::Literal<int32_t>;
using VIXLUInt32Literal = vixl::aarch32::Literal<uint32_t>;

class JumpTableARMVIXL : public DeletableArenaObject<kArenaAllocSwitchTable> {
 public:
  explicit JumpTableARMVIXL(HPackedSwitch* switch_instr)
      : switch_instr_(switch_instr),
        table_start_(),
        bb_addresses_(switch_instr->GetAllocator()->Adapter(kArenaAllocCodeGenerator)) {
    uint32_t num_entries = switch_instr_->GetNumEntries();
    for (uint32_t i = 0; i < num_entries; i++) {
      VIXLInt32Literal *lit = new VIXLInt32Literal(0, vixl32::RawLiteral::kManuallyPlaced);
      bb_addresses_.emplace_back(lit);
    }
  }

  vixl::aarch32::Label* GetTableStartLabel() { return &table_start_; }

  void EmitTable(CodeGeneratorARMVIXL* codegen);
  void FixTable(CodeGeneratorARMVIXL* codegen);

 private:
  HPackedSwitch* const switch_instr_;
  vixl::aarch32::Label table_start_;
  ArenaVector<std::unique_ptr<VIXLInt32Literal>> bb_addresses_;

  DISALLOW_COPY_AND_ASSIGN(JumpTableARMVIXL);
};

class InvokeRuntimeCallingConventionARMVIXL
    : public CallingConvention<vixl::aarch32::Register, vixl::aarch32::SRegister> {
 public:
  InvokeRuntimeCallingConventionARMVIXL()
      : CallingConvention(kRuntimeParameterCoreRegistersVIXL,
                          kRuntimeParameterCoreRegistersLengthVIXL,
                          kRuntimeParameterFpuRegistersVIXL,
                          kRuntimeParameterFpuRegistersLengthVIXL,
                          kArmPointerSize) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(InvokeRuntimeCallingConventionARMVIXL);
};

class InvokeDexCallingConventionARMVIXL
    : public CallingConvention<vixl::aarch32::Register, vixl::aarch32::SRegister> {
 public:
  InvokeDexCallingConventionARMVIXL()
      : CallingConvention(kParameterCoreRegistersVIXL,
                          kParameterCoreRegistersLengthVIXL,
                          kParameterFpuRegistersVIXL,
                          kParameterFpuRegistersLengthVIXL,
                          kArmPointerSize) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(InvokeDexCallingConventionARMVIXL);
};

class InvokeDexCallingConventionVisitorARMVIXL : public InvokeDexCallingConventionVisitor {
 public:
  InvokeDexCallingConventionVisitorARMVIXL() {}
  virtual ~InvokeDexCallingConventionVisitorARMVIXL() {}

  Location GetNextLocation(DataType::Type type) override;
  Location GetReturnLocation(DataType::Type type) const override;
  Location GetMethodLocation() const override;

 private:
  InvokeDexCallingConventionARMVIXL calling_convention;
  uint32_t double_index_ = 0;

  DISALLOW_COPY_AND_ASSIGN(InvokeDexCallingConventionVisitorARMVIXL);
};

class FieldAccessCallingConventionARMVIXL : public FieldAccessCallingConvention {
 public:
  FieldAccessCallingConventionARMVIXL() {}

  Location GetObjectLocation() const override {
    return helpers::LocationFrom(vixl::aarch32::r1);
  }
  Location GetFieldIndexLocation() const override {
    return helpers::LocationFrom(vixl::aarch32::r0);
  }
  Location GetReturnLocation(DataType::Type type) const override {
    return DataType::Is64BitType(type)
        ? helpers::LocationFrom(vixl::aarch32::r0, vixl::aarch32::r1)
        : helpers::LocationFrom(vixl::aarch32::r0);
  }
  Location GetSetValueLocation(DataType::Type type, bool is_instance) const override {
    return DataType::Is64BitType(type)
        ? helpers::LocationFrom(vixl::aarch32::r2, vixl::aarch32::r3)
        : (is_instance
            ? helpers::LocationFrom(vixl::aarch32::r2)
            : helpers::LocationFrom(vixl::aarch32::r1));
  }
  Location GetFpuLocation(DataType::Type type) const override {
    return DataType::Is64BitType(type)
        ? helpers::LocationFrom(vixl::aarch32::s0, vixl::aarch32::s1)
        : helpers::LocationFrom(vixl::aarch32::s0);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FieldAccessCallingConventionARMVIXL);
};

class SlowPathCodeARMVIXL : public SlowPathCode {
 public:
  explicit SlowPathCodeARMVIXL(HInstruction* instruction)
      : SlowPathCode(instruction), entry_label_(), exit_label_() {}

  vixl::aarch32::Label* GetEntryLabel() { return &entry_label_; }
  vixl::aarch32::Label* GetExitLabel() { return &exit_label_; }

  void SaveLiveRegisters(CodeGenerator* codegen, LocationSummary* locations) override;
  void RestoreLiveRegisters(CodeGenerator* codegen, LocationSummary* locations) override;

 private:
  vixl::aarch32::Label entry_label_;
  vixl::aarch32::Label exit_label_;

  DISALLOW_COPY_AND_ASSIGN(SlowPathCodeARMVIXL);
};

class ParallelMoveResolverARMVIXL : public ParallelMoveResolverWithSwap {
 public:
  ParallelMoveResolverARMVIXL(ArenaAllocator* allocator, CodeGeneratorARMVIXL* codegen)
      : ParallelMoveResolverWithSwap(allocator), codegen_(codegen) {}

  void EmitMove(size_t index) override;
  void EmitSwap(size_t index) override;
  void SpillScratch(int reg) override;
  void RestoreScratch(int reg) override;

  ArmVIXLAssembler* GetAssembler() const;

 private:
  void Exchange(vixl32::Register reg, int mem);
  void Exchange(int mem1, int mem2);

  CodeGeneratorARMVIXL* const codegen_;

  DISALLOW_COPY_AND_ASSIGN(ParallelMoveResolverARMVIXL);
};

class LocationsBuilderARMVIXL : public HGraphVisitor {
 public:
  LocationsBuilderARMVIXL(HGraph* graph, CodeGeneratorARMVIXL* codegen)
      : HGraphVisitor(graph), codegen_(codegen) {}

#define DECLARE_VISIT_INSTRUCTION(name, super)     \
  void Visit##name(H##name* instr) override;

  FOR_EACH_CONCRETE_INSTRUCTION_COMMON(DECLARE_VISIT_INSTRUCTION)
  FOR_EACH_CONCRETE_INSTRUCTION_ARM(DECLARE_VISIT_INSTRUCTION)
  FOR_EACH_CONCRETE_INSTRUCTION_SHARED(DECLARE_VISIT_INSTRUCTION)

#undef DECLARE_VISIT_INSTRUCTION

  void VisitInstruction(HInstruction* instruction) override {
    LOG(FATAL) << "Unreachable instruction " << instruction->DebugName()
               << " (id " << instruction->GetId() << ")";
  }

 private:
  void HandleInvoke(HInvoke* invoke);
  void HandleBitwiseOperation(HBinaryOperation* operation, Opcode opcode);
  void HandleCondition(HCondition* condition);
  void HandleIntegerRotate(LocationSummary* locations);
  void HandleLongRotate(LocationSummary* locations);
  void HandleShift(HBinaryOperation* operation);
  void HandleFieldSet(HInstruction* instruction, const FieldInfo& field_info);
  void HandleFieldGet(HInstruction* instruction, const FieldInfo& field_info);

  Location ArithmeticZeroOrFpuRegister(HInstruction* input);
  Location ArmEncodableConstantOrRegister(HInstruction* constant, Opcode opcode);
  bool CanEncodeConstantAsImmediate(HConstant* input_cst, Opcode opcode);

  CodeGeneratorARMVIXL* const codegen_;
  InvokeDexCallingConventionVisitorARMVIXL parameter_visitor_;

  DISALLOW_COPY_AND_ASSIGN(LocationsBuilderARMVIXL);
};

class InstructionCodeGeneratorARMVIXL : public InstructionCodeGenerator {
 public:
  InstructionCodeGeneratorARMVIXL(HGraph* graph, CodeGeneratorARMVIXL* codegen);

#define DECLARE_VISIT_INSTRUCTION(name, super)     \
  void Visit##name(H##name* instr) override;

  FOR_EACH_CONCRETE_INSTRUCTION_COMMON(DECLARE_VISIT_INSTRUCTION)
  FOR_EACH_CONCRETE_INSTRUCTION_ARM(DECLARE_VISIT_INSTRUCTION)
  FOR_EACH_CONCRETE_INSTRUCTION_SHARED(DECLARE_VISIT_INSTRUCTION)

#undef DECLARE_VISIT_INSTRUCTION

  void VisitInstruction(HInstruction* instruction) override {
    LOG(FATAL) << "Unreachable instruction " << instruction->DebugName()
               << " (id " << instruction->GetId() << ")";
  }

  ArmVIXLAssembler* GetAssembler() const { return assembler_; }
  ArmVIXLMacroAssembler* GetVIXLAssembler() { return GetAssembler()->GetVIXLAssembler(); }

 private:
  // Generate code for the given suspend check. If not null, `successor`
  // is the block to branch to if the suspend check is not needed, and after
  // the suspend call.
  void GenerateSuspendCheck(HSuspendCheck* instruction, HBasicBlock* successor);
  void GenerateClassInitializationCheck(LoadClassSlowPathARMVIXL* slow_path,
                                        vixl32::Register class_reg);
  void GenerateBitstringTypeCheckCompare(HTypeCheckInstruction* check,
                                         vixl::aarch32::Register temp,
                                         vixl::aarch32::FlagsUpdate flags_update);
  void GenerateAndConst(vixl::aarch32::Register out, vixl::aarch32::Register first, uint32_t value);
  void GenerateOrrConst(vixl::aarch32::Register out, vixl::aarch32::Register first, uint32_t value);
  void GenerateEorConst(vixl::aarch32::Register out, vixl::aarch32::Register first, uint32_t value);
  void GenerateAddLongConst(Location out, Location first, uint64_t value);
  void HandleBitwiseOperation(HBinaryOperation* operation);
  void HandleCondition(HCondition* condition);
  void HandleIntegerRotate(HRor* ror);
  void HandleLongRotate(HRor* ror);
  void HandleShift(HBinaryOperation* operation);

  void GenerateWideAtomicStore(vixl::aarch32::Register addr,
                               uint32_t offset,
                               vixl::aarch32::Register value_lo,
                               vixl::aarch32::Register value_hi,
                               vixl::aarch32::Register temp1,
                               vixl::aarch32::Register temp2,
                               HInstruction* instruction);
  void GenerateWideAtomicLoad(vixl::aarch32::Register addr,
                              uint32_t offset,
                              vixl::aarch32::Register out_lo,
                              vixl::aarch32::Register out_hi);

  void HandleFieldSet(HInstruction* instruction,
                      const FieldInfo& field_info,
                      bool value_can_be_null);
  void HandleFieldGet(HInstruction* instruction, const FieldInfo& field_info);

  void GenerateMinMaxInt(LocationSummary* locations, bool is_min);
  void GenerateMinMaxLong(LocationSummary* locations, bool is_min);
  void GenerateMinMaxFloat(HInstruction* minmax, bool is_min);
  void GenerateMinMaxDouble(HInstruction* minmax, bool is_min);
  void GenerateMinMax(HBinaryOperation* minmax, bool is_min);

  // Generate a heap reference load using one register `out`:
  //
  //   out <- *(out + offset)
  //
  // while honoring heap poisoning and/or read barriers (if any).
  //
  // Location `maybe_temp` is used when generating a read barrier and
  // shall be a register in that case; it may be an invalid location
  // otherwise.
  void GenerateReferenceLoadOneRegister(HInstruction* instruction,
                                        Location out,
                                        uint32_t offset,
                                        Location maybe_temp,
                                        ReadBarrierOption read_barrier_option);
  // Generate a heap reference load using two different registers
  // `out` and `obj`:
  //
  //   out <- *(obj + offset)
  //
  // while honoring heap poisoning and/or read barriers (if any).
  //
  // Location `maybe_temp` is used when generating a Baker's (fast
  // path) read barrier and shall be a register in that case; it may
  // be an invalid location otherwise.
  void GenerateReferenceLoadTwoRegisters(HInstruction* instruction,
                                         Location out,
                                         Location obj,
                                         uint32_t offset,
                                         Location maybe_temp,
                                         ReadBarrierOption read_barrier_option);
  void GenerateTestAndBranch(HInstruction* instruction,
                             size_t condition_input_index,
                             vixl::aarch32::Label* true_target,
                             vixl::aarch32::Label* false_target,
                             bool far_target = true);
  void GenerateCompareTestAndBranch(HCondition* condition,
                                    vixl::aarch32::Label* true_target,
                                    vixl::aarch32::Label* false_target,
                                    bool is_far_target = true);
  void DivRemOneOrMinusOne(HBinaryOperation* instruction);
  void DivRemByPowerOfTwo(HBinaryOperation* instruction);
  void GenerateDivRemWithAnyConstant(HBinaryOperation* instruction);
  void GenerateDivRemConstantIntegral(HBinaryOperation* instruction);
  void HandleGoto(HInstruction* got, HBasicBlock* successor);

  vixl::aarch32::MemOperand VecAddress(
      HVecMemoryOperation* instruction,
      // This function may acquire a scratch register.
      vixl::aarch32::UseScratchRegisterScope* temps_scope,
      /*out*/ vixl32::Register* scratch);
  vixl::aarch32::AlignedMemOperand VecAddressUnaligned(
      HVecMemoryOperation* instruction,
      // This function may acquire a scratch register.
      vixl::aarch32::UseScratchRegisterScope* temps_scope,
      /*out*/ vixl32::Register* scratch);

  ArmVIXLAssembler* const assembler_;
  CodeGeneratorARMVIXL* const codegen_;

  DISALLOW_COPY_AND_ASSIGN(InstructionCodeGeneratorARMVIXL);
};

class CodeGeneratorARMVIXL : public CodeGenerator {
 public:
  CodeGeneratorARMVIXL(HGraph* graph,
                       const CompilerOptions& compiler_options,
                       OptimizingCompilerStats* stats = nullptr);
  virtual ~CodeGeneratorARMVIXL() {}

  void GenerateFrameEntry() override;
  void GenerateFrameExit() override;
  void Bind(HBasicBlock* block) override;
  void MoveConstant(Location destination, int32_t value) override;
  void MoveLocation(Location dst, Location src, DataType::Type dst_type) override;
  void AddLocationAsTemp(Location location, LocationSummary* locations) override;

  size_t SaveCoreRegister(size_t stack_index, uint32_t reg_id) override;
  size_t RestoreCoreRegister(size_t stack_index, uint32_t reg_id) override;
  size_t SaveFloatingPointRegister(size_t stack_index, uint32_t reg_id) override;
  size_t RestoreFloatingPointRegister(size_t stack_index, uint32_t reg_id) override;

  size_t GetWordSize() const override {
    return static_cast<size_t>(kArmPointerSize);
  }

  size_t GetFloatingPointSpillSlotSize() const override { return vixl::aarch32::kRegSizeInBytes; }

  HGraphVisitor* GetLocationBuilder() override { return &location_builder_; }

  HGraphVisitor* GetInstructionVisitor() override { return &instruction_visitor_; }

  ArmVIXLAssembler* GetAssembler() override { return &assembler_; }

  const ArmVIXLAssembler& GetAssembler() const override { return assembler_; }

  ArmVIXLMacroAssembler* GetVIXLAssembler() { return GetAssembler()->GetVIXLAssembler(); }

  uintptr_t GetAddressOf(HBasicBlock* block) override {
    vixl::aarch32::Label* block_entry_label = GetLabelOf(block);
    DCHECK(block_entry_label->IsBound());
    return block_entry_label->GetLocation();
  }

  void FixJumpTables();
  void SetupBlockedRegisters() const override;

  void DumpCoreRegister(std::ostream& stream, int reg) const override;
  void DumpFloatingPointRegister(std::ostream& stream, int reg) const override;

  ParallelMoveResolver* GetMoveResolver() override { return &move_resolver_; }
  InstructionSet GetInstructionSet() const override { return InstructionSet::kThumb2; }

  const ArmInstructionSetFeatures& GetInstructionSetFeatures() const;

  // Helper method to move a 32-bit value between two locations.
  void Move32(Location destination, Location source);

  void LoadFromShiftedRegOffset(DataType::Type type,
                                Location out_loc,
                                vixl::aarch32::Register base,
                                vixl::aarch32::Register reg_index,
                                vixl::aarch32::Condition cond = vixl::aarch32::al);
  void StoreToShiftedRegOffset(DataType::Type type,
                               Location out_loc,
                               vixl::aarch32::Register base,
                               vixl::aarch32::Register reg_index,
                               vixl::aarch32::Condition cond = vixl::aarch32::al);

  // Generate code to invoke a runtime entry point.
  void InvokeRuntime(QuickEntrypointEnum entrypoint,
                     HInstruction* instruction,
                     uint32_t dex_pc,
                     SlowPathCode* slow_path = nullptr) override;

  // Generate code to invoke a runtime entry point, but do not record
  // PC-related information in a stack map.
  void InvokeRuntimeWithoutRecordingPcInfo(int32_t entry_point_offset,
                                           HInstruction* instruction,
                                           SlowPathCode* slow_path);

  // Emit a write barrier.
  void MarkGCCard(vixl::aarch32::Register temp,
                  vixl::aarch32::Register card,
                  vixl::aarch32::Register object,
                  vixl::aarch32::Register value,
                  bool can_be_null);

  void GenerateMemoryBarrier(MemBarrierKind kind);

  vixl::aarch32::Label* GetLabelOf(HBasicBlock* block) {
    block = FirstNonEmptyBlock(block);
    return &(block_labels_[block->GetBlockId()]);
  }

  vixl32::Label* GetFinalLabel(HInstruction* instruction, vixl32::Label* final_label);

  void Initialize() override {
    block_labels_.resize(GetGraph()->GetBlocks().size());
  }

  void Finalize(CodeAllocator* allocator) override;

  bool NeedsTwoRegisters(DataType::Type type) const override {
    return type == DataType::Type::kFloat64 || type == DataType::Type::kInt64;
  }

  void ComputeSpillMask() override;

  vixl::aarch32::Label* GetFrameEntryLabel() { return &frame_entry_label_; }

  // Check if the desired_string_load_kind is supported. If it is, return it,
  // otherwise return a fall-back kind that should be used instead.
  HLoadString::LoadKind GetSupportedLoadStringKind(
      HLoadString::LoadKind desired_string_load_kind) override;

  // Check if the desired_class_load_kind is supported. If it is, return it,
  // otherwise return a fall-back kind that should be used instead.
  HLoadClass::LoadKind GetSupportedLoadClassKind(
      HLoadClass::LoadKind desired_class_load_kind) override;

  // Check if the desired_dispatch_info is supported. If it is, return it,
  // otherwise return a fall-back info that should be used instead.
  HInvokeStaticOrDirect::DispatchInfo GetSupportedInvokeStaticOrDirectDispatch(
      const HInvokeStaticOrDirect::DispatchInfo& desired_dispatch_info,
      HInvokeStaticOrDirect* invoke) override;

  void GenerateStaticOrDirectCall(
      HInvokeStaticOrDirect* invoke, Location temp, SlowPathCode* slow_path = nullptr) override;
  void GenerateVirtualCall(
      HInvokeVirtual* invoke, Location temp, SlowPathCode* slow_path = nullptr) override;

  void MoveFromReturnRegister(Location trg, DataType::Type type) override;

  // The PcRelativePatchInfo is used for PC-relative addressing of methods/strings/types,
  // whether through .data.bimg.rel.ro, .bss, or directly in the boot image.
  //
  // The PC-relative address is loaded with three instructions,
  // MOVW+MOVT to load the offset to base_reg and then ADD base_reg, PC. The offset
  // is calculated from the ADD's effective PC, i.e. PC+4 on Thumb2. Though we
  // currently emit these 3 instructions together, instruction scheduling could
  // split this sequence apart, so we keep separate labels for each of them.
  struct PcRelativePatchInfo {
    PcRelativePatchInfo(const DexFile* dex_file, uint32_t off_or_idx)
        : target_dex_file(dex_file), offset_or_index(off_or_idx) { }
    PcRelativePatchInfo(PcRelativePatchInfo&& other) = default;

    // Target dex file or null for .data.bmig.rel.ro patches.
    const DexFile* target_dex_file;
    // Either the boot image offset (to write to .data.bmig.rel.ro) or string/type/method index.
    uint32_t offset_or_index;
    vixl::aarch32::Label movw_label;
    vixl::aarch32::Label movt_label;
    vixl::aarch32::Label add_pc_label;
  };

  PcRelativePatchInfo* NewBootImageIntrinsicPatch(uint32_t intrinsic_data);
  PcRelativePatchInfo* NewBootImageRelRoPatch(uint32_t boot_image_offset);
  PcRelativePatchInfo* NewBootImageMethodPatch(MethodReference target_method);
  PcRelativePatchInfo* NewMethodBssEntryPatch(MethodReference target_method);
  PcRelativePatchInfo* NewBootImageTypePatch(const DexFile& dex_file, dex::TypeIndex type_index);
  PcRelativePatchInfo* NewTypeBssEntryPatch(const DexFile& dex_file, dex::TypeIndex type_index);
  PcRelativePatchInfo* NewBootImageStringPatch(const DexFile& dex_file,
                                               dex::StringIndex string_index);
  PcRelativePatchInfo* NewStringBssEntryPatch(const DexFile& dex_file,
                                              dex::StringIndex string_index);

  // Emit the BNE instruction for baker read barrier and record
  // the associated patch for AOT or slow path for JIT.
  void EmitBakerReadBarrierBne(uint32_t custom_data);

  VIXLUInt32Literal* DeduplicateBootImageAddressLiteral(uint32_t address);
  VIXLUInt32Literal* DeduplicateJitStringLiteral(const DexFile& dex_file,
                                                 dex::StringIndex string_index,
                                                 Handle<mirror::String> handle);
  VIXLUInt32Literal* DeduplicateJitClassLiteral(const DexFile& dex_file,
                                                dex::TypeIndex type_index,
                                                Handle<mirror::Class> handle);

  void LoadBootImageAddress(vixl::aarch32::Register reg, uint32_t boot_image_reference);
  void AllocateInstanceForIntrinsic(HInvokeStaticOrDirect* invoke, uint32_t boot_image_offset);

  void EmitLinkerPatches(ArenaVector<linker::LinkerPatch>* linker_patches) override;
  bool NeedsThunkCode(const linker::LinkerPatch& patch) const override;
  void EmitThunkCode(const linker::LinkerPatch& patch,
                     /*out*/ ArenaVector<uint8_t>* code,
                     /*out*/ std::string* debug_name) override;

  void EmitJitRootPatches(uint8_t* code, const uint8_t* roots_data) override;

  // Generate a GC root reference load:
  //
  //   root <- *(obj + offset)
  //
  // while honoring read barriers based on read_barrier_option.
  void GenerateGcRootFieldLoad(HInstruction* instruction,
                               Location root,
                               vixl::aarch32::Register obj,
                               uint32_t offset,
                               ReadBarrierOption read_barrier_option);
  // Generate ADD for UnsafeCASObject to reconstruct the old value from
  // `old_value - expected` and mark it with Baker read barrier.
  void GenerateUnsafeCasOldValueAddWithBakerReadBarrier(vixl::aarch32::Register old_value,
                                                        vixl::aarch32::Register adjusted_old_value,
                                                        vixl::aarch32::Register expected);
  // Fast path implementation of ReadBarrier::Barrier for a heap
  // reference field load when Baker's read barriers are used.
  // Overload suitable for Unsafe.getObject/-Volatile() intrinsic.
  void GenerateFieldLoadWithBakerReadBarrier(HInstruction* instruction,
                                             Location ref,
                                             vixl::aarch32::Register obj,
                                             const vixl::aarch32::MemOperand& src,
                                             bool needs_null_check);
  // Fast path implementation of ReadBarrier::Barrier for a heap
  // reference field load when Baker's read barriers are used.
  void GenerateFieldLoadWithBakerReadBarrier(HInstruction* instruction,
                                             Location ref,
                                             vixl::aarch32::Register obj,
                                             uint32_t offset,
                                             Location temp,
                                             bool needs_null_check);
  // Fast path implementation of ReadBarrier::Barrier for a heap
  // reference array load when Baker's read barriers are used.
  void GenerateArrayLoadWithBakerReadBarrier(Location ref,
                                             vixl::aarch32::Register obj,
                                             uint32_t data_offset,
                                             Location index,
                                             Location temp,
                                             bool needs_null_check);

  // Emit code checking the status of the Marking Register, and
  // aborting the program if MR does not match the value stored in the
  // art::Thread object. Code is only emitted in debug mode and if
  // CompilerOptions::EmitRunTimeChecksInDebugMode returns true.
  //
  // Argument `code` is used to identify the different occurrences of
  // MaybeGenerateMarkingRegisterCheck in the code generator, and is
  // used together with kMarkingRegisterCheckBreakCodeBaseCode to
  // create the value passed to the BKPT instruction. Note that unlike
  // in the ARM64 code generator, where `__LINE__` is passed as `code`
  // argument to
  // CodeGeneratorARM64::MaybeGenerateMarkingRegisterCheck, we cannot
  // realistically do that here, as Encoding T1 for the BKPT
  // instruction only accepts 8-bit immediate values.
  //
  // If `temp_loc` is a valid location, it is expected to be a
  // register and will be used as a temporary to generate code;
  // otherwise, a temporary will be fetched from the core register
  // scratch pool.
  virtual void MaybeGenerateMarkingRegisterCheck(int code,
                                                 Location temp_loc = Location::NoLocation());

  // Generate a read barrier for a heap reference within `instruction`
  // using a slow path.
  //
  // A read barrier for an object reference read from the heap is
  // implemented as a call to the artReadBarrierSlow runtime entry
  // point, which is passed the values in locations `ref`, `obj`, and
  // `offset`:
  //
  //   mirror::Object* artReadBarrierSlow(mirror::Object* ref,
  //                                      mirror::Object* obj,
  //                                      uint32_t offset);
  //
  // The `out` location contains the value returned by
  // artReadBarrierSlow.
  //
  // When `index` is provided (i.e. for array accesses), the offset
  // value passed to artReadBarrierSlow is adjusted to take `index`
  // into account.
  void GenerateReadBarrierSlow(HInstruction* instruction,
                               Location out,
                               Location ref,
                               Location obj,
                               uint32_t offset,
                               Location index = Location::NoLocation());

  // If read barriers are enabled, generate a read barrier for a heap
  // reference using a slow path. If heap poisoning is enabled, also
  // unpoison the reference in `out`.
  void MaybeGenerateReadBarrierSlow(HInstruction* instruction,
                                    Location out,
                                    Location ref,
                                    Location obj,
                                    uint32_t offset,
                                    Location index = Location::NoLocation());

  // Generate a read barrier for a GC root within `instruction` using
  // a slow path.
  //
  // A read barrier for an object reference GC root is implemented as
  // a call to the artReadBarrierForRootSlow runtime entry point,
  // which is passed the value in location `root`:
  //
  //   mirror::Object* artReadBarrierForRootSlow(GcRoot<mirror::Object>* root);
  //
  // The `out` location contains the value returned by
  // artReadBarrierForRootSlow.
  void GenerateReadBarrierForRootSlow(HInstruction* instruction, Location out, Location root);

  void GenerateNop() override;

  void GenerateImplicitNullCheck(HNullCheck* instruction) override;
  void GenerateExplicitNullCheck(HNullCheck* instruction) override;

  JumpTableARMVIXL* CreateJumpTable(HPackedSwitch* switch_instr) {
    jump_tables_.emplace_back(new (GetGraph()->GetAllocator()) JumpTableARMVIXL(switch_instr));
    return jump_tables_.back().get();
  }
  void EmitJumpTables();

  void EmitMovwMovtPlaceholder(CodeGeneratorARMVIXL::PcRelativePatchInfo* labels,
                               vixl::aarch32::Register out);

  // `temp` is an extra temporary register that is used for some conditions;
  // callers may not specify it, in which case the method will use a scratch
  // register instead.
  void GenerateConditionWithZero(IfCondition condition,
                                 vixl::aarch32::Register out,
                                 vixl::aarch32::Register in,
                                 vixl::aarch32::Register temp = vixl32::Register());

 private:
  // Encoding of thunk type and data for link-time generated thunks for Baker read barriers.

  enum class BakerReadBarrierKind : uint8_t {
    kField,       // Field get or array get with constant offset (i.e. constant index).
    kArray,       // Array get with index in register.
    kGcRoot,      // GC root load.
    kUnsafeCas,   // UnsafeCASObject intrinsic.
    kLast = kUnsafeCas
  };

  enum class BakerReadBarrierWidth : uint8_t {
    kWide,          // 32-bit LDR (and 32-bit NEG if heap poisoning is enabled).
    kNarrow,        // 16-bit LDR (and 16-bit NEG if heap poisoning is enabled).
    kLast = kNarrow
  };

  static constexpr uint32_t kBakerReadBarrierInvalidEncodedReg = /* pc is invalid */ 15u;

  static constexpr size_t kBitsForBakerReadBarrierKind =
      MinimumBitsToStore(static_cast<size_t>(BakerReadBarrierKind::kLast));
  static constexpr size_t kBakerReadBarrierBitsForRegister =
      MinimumBitsToStore(kBakerReadBarrierInvalidEncodedReg);
  using BakerReadBarrierKindField =
      BitField<BakerReadBarrierKind, 0, kBitsForBakerReadBarrierKind>;
  using BakerReadBarrierFirstRegField =
      BitField<uint32_t, kBitsForBakerReadBarrierKind, kBakerReadBarrierBitsForRegister>;
  using BakerReadBarrierSecondRegField =
      BitField<uint32_t,
               kBitsForBakerReadBarrierKind + kBakerReadBarrierBitsForRegister,
               kBakerReadBarrierBitsForRegister>;
  static constexpr size_t kBitsForBakerReadBarrierWidth =
      MinimumBitsToStore(static_cast<size_t>(BakerReadBarrierWidth::kLast));
  using BakerReadBarrierWidthField =
      BitField<BakerReadBarrierWidth,
               kBitsForBakerReadBarrierKind + 2 * kBakerReadBarrierBitsForRegister,
               kBitsForBakerReadBarrierWidth>;

  static void CheckValidReg(uint32_t reg) {
    DCHECK(reg < vixl::aarch32::ip.GetCode() && reg != mr.GetCode()) << reg;
  }

  static uint32_t EncodeBakerReadBarrierFieldData(uint32_t base_reg,
                                                  uint32_t holder_reg,
                                                  bool narrow) {
    CheckValidReg(base_reg);
    CheckValidReg(holder_reg);
    DCHECK(!narrow || base_reg < 8u) << base_reg;
    BakerReadBarrierWidth width =
        narrow ? BakerReadBarrierWidth::kNarrow : BakerReadBarrierWidth::kWide;
    return BakerReadBarrierKindField::Encode(BakerReadBarrierKind::kField) |
           BakerReadBarrierFirstRegField::Encode(base_reg) |
           BakerReadBarrierSecondRegField::Encode(holder_reg) |
           BakerReadBarrierWidthField::Encode(width);
  }

  static uint32_t EncodeBakerReadBarrierArrayData(uint32_t base_reg) {
    CheckValidReg(base_reg);
    return BakerReadBarrierKindField::Encode(BakerReadBarrierKind::kArray) |
           BakerReadBarrierFirstRegField::Encode(base_reg) |
           BakerReadBarrierSecondRegField::Encode(kBakerReadBarrierInvalidEncodedReg) |
           BakerReadBarrierWidthField::Encode(BakerReadBarrierWidth::kWide);
  }

  static uint32_t EncodeBakerReadBarrierGcRootData(uint32_t root_reg, bool narrow) {
    CheckValidReg(root_reg);
    DCHECK(!narrow || root_reg < 8u) << root_reg;
    BakerReadBarrierWidth width =
        narrow ? BakerReadBarrierWidth::kNarrow : BakerReadBarrierWidth::kWide;
    return BakerReadBarrierKindField::Encode(BakerReadBarrierKind::kGcRoot) |
           BakerReadBarrierFirstRegField::Encode(root_reg) |
           BakerReadBarrierSecondRegField::Encode(kBakerReadBarrierInvalidEncodedReg) |
           BakerReadBarrierWidthField::Encode(width);
  }

  static uint32_t EncodeBakerReadBarrierUnsafeCasData(uint32_t root_reg) {
    CheckValidReg(root_reg);
    return BakerReadBarrierKindField::Encode(BakerReadBarrierKind::kUnsafeCas) |
           BakerReadBarrierFirstRegField::Encode(root_reg) |
           BakerReadBarrierSecondRegField::Encode(kBakerReadBarrierInvalidEncodedReg) |
           BakerReadBarrierWidthField::Encode(BakerReadBarrierWidth::kWide);
  }

  void CompileBakerReadBarrierThunk(ArmVIXLAssembler& assembler,
                                    uint32_t encoded_data,
                                    /*out*/ std::string* debug_name);

  vixl::aarch32::Register GetInvokeStaticOrDirectExtraParameter(HInvokeStaticOrDirect* invoke,
                                                                vixl::aarch32::Register temp);

  using Uint32ToLiteralMap = ArenaSafeMap<uint32_t, VIXLUInt32Literal*>;
  using StringToLiteralMap = ArenaSafeMap<StringReference,
                                          VIXLUInt32Literal*,
                                          StringReferenceValueComparator>;
  using TypeToLiteralMap = ArenaSafeMap<TypeReference,
                                        VIXLUInt32Literal*,
                                        TypeReferenceValueComparator>;

  struct BakerReadBarrierPatchInfo {
    explicit BakerReadBarrierPatchInfo(uint32_t data) : label(), custom_data(data) { }

    vixl::aarch32::Label label;
    uint32_t custom_data;
  };

  VIXLUInt32Literal* DeduplicateUint32Literal(uint32_t value, Uint32ToLiteralMap* map);
  PcRelativePatchInfo* NewPcRelativePatch(const DexFile* dex_file,
                                          uint32_t offset_or_index,
                                          ArenaDeque<PcRelativePatchInfo>* patches);
  template <linker::LinkerPatch (*Factory)(size_t, const DexFile*, uint32_t, uint32_t)>
  static void EmitPcRelativeLinkerPatches(const ArenaDeque<PcRelativePatchInfo>& infos,
                                          ArenaVector<linker::LinkerPatch>* linker_patches);

  // Labels for each block that will be compiled.
  // We use a deque so that the `vixl::aarch32::Label` objects do not move in memory.
  ArenaDeque<vixl::aarch32::Label> block_labels_;  // Indexed by block id.
  vixl::aarch32::Label frame_entry_label_;

  ArenaVector<std::unique_ptr<JumpTableARMVIXL>> jump_tables_;
  LocationsBuilderARMVIXL location_builder_;
  InstructionCodeGeneratorARMVIXL instruction_visitor_;
  ParallelMoveResolverARMVIXL move_resolver_;

  ArmVIXLAssembler assembler_;

  // Deduplication map for 32-bit literals, used for non-patchable boot image addresses.
  Uint32ToLiteralMap uint32_literals_;
  // PC-relative method patch info for kBootImageLinkTimePcRelative/kBootImageRelRo.
  // Also used for type/string patches for kBootImageRelRo (same linker patch as for methods).
  ArenaDeque<PcRelativePatchInfo> boot_image_method_patches_;
  // PC-relative method patch info for kBssEntry.
  ArenaDeque<PcRelativePatchInfo> method_bss_entry_patches_;
  // PC-relative type patch info for kBootImageLinkTimePcRelative.
  ArenaDeque<PcRelativePatchInfo> boot_image_type_patches_;
  // PC-relative type patch info for kBssEntry.
  ArenaDeque<PcRelativePatchInfo> type_bss_entry_patches_;
  // PC-relative String patch info for kBootImageLinkTimePcRelative.
  ArenaDeque<PcRelativePatchInfo> boot_image_string_patches_;
  // PC-relative String patch info for kBssEntry.
  ArenaDeque<PcRelativePatchInfo> string_bss_entry_patches_;
  // PC-relative patch info for IntrinsicObjects.
  ArenaDeque<PcRelativePatchInfo> boot_image_intrinsic_patches_;
  // Baker read barrier patch info.
  ArenaDeque<BakerReadBarrierPatchInfo> baker_read_barrier_patches_;

  // Patches for string literals in JIT compiled code.
  StringToLiteralMap jit_string_patches_;
  // Patches for class literals in JIT compiled code.
  TypeToLiteralMap jit_class_patches_;

  // Baker read barrier slow paths, mapping custom data (uint32_t) to label.
  // Wrap the label to work around vixl::aarch32::Label being non-copyable
  // and non-moveable and as such unusable in ArenaSafeMap<>.
  struct LabelWrapper {
    LabelWrapper(const LabelWrapper& src)
        : label() {
      DCHECK(!src.label.IsReferenced() && !src.label.IsBound());
    }
    LabelWrapper() = default;
    vixl::aarch32::Label label;
  };
  ArenaSafeMap<uint32_t, LabelWrapper> jit_baker_read_barrier_slow_paths_;

  friend class linker::Thumb2RelativePatcherTest;
  DISALLOW_COPY_AND_ASSIGN(CodeGeneratorARMVIXL);
};

}  // namespace arm
}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_CODE_GENERATOR_ARM_VIXL_H_
