/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include "dex_to_dex_compiler.h"

#include <android-base/logging.h>
#include <android-base/stringprintf.h>

#include "art_field-inl.h"
#include "art_method-inl.h"
#include "base/logging.h"  // For VLOG
#include "base/macros.h"
#include "base/mutex.h"
#include "bytecode_utils.h"
#include "compiled_method.h"
#include "dex/dex_file-inl.h"
#include "dex/dex_instruction-inl.h"
#include "dex_to_dex_decompiler.h"
#include "driver/compiler_driver.h"
#include "driver/dex_compilation_unit.h"
#include "mirror/dex_cache.h"
#include "quicken_info.h"
#include "thread-current-inl.h"

namespace art {
namespace optimizer {

using android::base::StringPrintf;

// Controls quickening activation.
const bool kEnableQuickening = true;
// Control check-cast elision.
const bool kEnableCheckCastEllision = true;

DexToDexCompiler::DexToDexCompiler(CompilerDriver* driver)
    : driver_(driver),
      lock_("Quicken lock", kDexToDexCompilerLock) {
  DCHECK(driver != nullptr);
}

void DexToDexCompiler::ClearState() {
  MutexLock lock(Thread::Current(), lock_);
  active_dex_file_ = nullptr;
  active_bit_vector_ = nullptr;
  seen_code_items_.clear();
  should_quicken_.clear();
  shared_code_items_.clear();
  blacklisted_code_items_.clear();
  shared_code_item_quicken_info_.clear();
}

size_t DexToDexCompiler::NumUniqueCodeItems(Thread* self) const {
  MutexLock lock(self, lock_);
  return seen_code_items_.size();
}

BitVector* DexToDexCompiler::GetOrAddBitVectorForDex(const DexFile* dex_file) {
  if (active_dex_file_ != dex_file) {
    active_dex_file_ = dex_file;
    auto inserted = should_quicken_.emplace(dex_file,
                                            BitVector(dex_file->NumMethodIds(),
                                                      /*expandable*/ false,
                                                      Allocator::GetMallocAllocator()));
    active_bit_vector_ = &inserted.first->second;
  }
  return active_bit_vector_;
}

void DexToDexCompiler::MarkForCompilation(Thread* self,
                                          const MethodReference& method_ref,
                                          const DexFile::CodeItem* code_item) {
  MutexLock lock(self, lock_);
  BitVector* const bitmap = GetOrAddBitVectorForDex(method_ref.dex_file);
  DCHECK(bitmap != nullptr);
  DCHECK(!bitmap->IsBitSet(method_ref.index));
  bitmap->SetBit(method_ref.index);
  // Detect the shared code items.
  if (!seen_code_items_.insert(code_item).second) {
    shared_code_items_.insert(code_item);
  }
}

DexToDexCompiler::CompilationState::CompilationState(DexToDexCompiler* compiler,
                                                     const DexCompilationUnit& unit,
                                                     const CompilationLevel compilation_level,
                                                     const std::vector<uint8_t>* quicken_data)
    : compiler_(compiler),
      driver_(*compiler->GetDriver()),
      unit_(unit),
      compilation_level_(compilation_level),
      already_quickened_(quicken_data != nullptr),
      existing_quicken_info_(already_quickened_
          ? ArrayRef<const uint8_t>(*quicken_data) : ArrayRef<const uint8_t>()) {}

uint16_t DexToDexCompiler::CompilationState::NextIndex() {
  DCHECK(already_quickened_);
  if (kIsDebugBuild && quicken_index_ >= existing_quicken_info_.NumIndices()) {
    for (const DexInstructionPcPair& pair : unit_.GetCodeItemAccessor()) {
      LOG(ERROR) << pair->DumpString(nullptr);
    }
    LOG(FATAL) << "Mismatched number of quicken slots.";
  }
  const uint16_t ret = existing_quicken_info_.GetData(quicken_index_);
  quicken_index_++;
  return ret;
}

uint16_t DexToDexCompiler::CompilationState::GetIndexForInstruction(const Instruction* inst,
                                                                    uint32_t index) {
  if (UNLIKELY(already_quickened_)) {
    return inst->IsQuickened() ? NextIndex() : index;
  }
  DCHECK(!inst->IsQuickened());
  return index;
}

bool DexToDexCompiler::ShouldCompileMethod(const MethodReference& ref) {
  // TODO: It's probably safe to avoid the lock here if the active_dex_file_ matches since we only
  // only call ShouldCompileMethod on one dex at a time.
  MutexLock lock(Thread::Current(), lock_);
  return GetOrAddBitVectorForDex(ref.dex_file)->IsBitSet(ref.index);
}

std::vector<uint8_t> DexToDexCompiler::CompilationState::Compile() {
  DCHECK_EQ(compilation_level_, CompilationLevel::kOptimize);
  const CodeItemDataAccessor& instructions = unit_.GetCodeItemAccessor();
  for (DexInstructionIterator it = instructions.begin(); it != instructions.end(); ++it) {
    const uint32_t dex_pc = it.DexPc();
    Instruction* inst = const_cast<Instruction*>(&it.Inst());

    if (!already_quickened_) {
      DCHECK(!inst->IsQuickened());
    }

    switch (inst->Opcode()) {
      case Instruction::RETURN_VOID:
        CompileReturnVoid(inst, dex_pc);
        break;

      case Instruction::CHECK_CAST:
        inst = CompileCheckCast(inst, dex_pc);
        if (inst->Opcode() == Instruction::NOP) {
          // We turned the CHECK_CAST into two NOPs, avoid visiting the second NOP twice since this
          // would add 2 quickening info entries.
          ++it;
        }
        break;

      case Instruction::IGET:
      case Instruction::IGET_QUICK:
        CompileInstanceFieldAccess(inst, dex_pc, Instruction::IGET_QUICK, false);
        break;

      case Instruction::IGET_WIDE:
      case Instruction::IGET_WIDE_QUICK:
        CompileInstanceFieldAccess(inst, dex_pc, Instruction::IGET_WIDE_QUICK, false);
        break;

      case Instruction::IGET_OBJECT:
      case Instruction::IGET_OBJECT_QUICK:
        CompileInstanceFieldAccess(inst, dex_pc, Instruction::IGET_OBJECT_QUICK, false);
        break;

      case Instruction::IGET_BOOLEAN:
      case Instruction::IGET_BOOLEAN_QUICK:
        CompileInstanceFieldAccess(inst, dex_pc, Instruction::IGET_BOOLEAN_QUICK, false);
        break;

      case Instruction::IGET_BYTE:
      case Instruction::IGET_BYTE_QUICK:
        CompileInstanceFieldAccess(inst, dex_pc, Instruction::IGET_BYTE_QUICK, false);
        break;

      case Instruction::IGET_CHAR:
      case Instruction::IGET_CHAR_QUICK:
        CompileInstanceFieldAccess(inst, dex_pc, Instruction::IGET_CHAR_QUICK, false);
        break;

      case Instruction::IGET_SHORT:
      case Instruction::IGET_SHORT_QUICK:
        CompileInstanceFieldAccess(inst, dex_pc, Instruction::IGET_SHORT_QUICK, false);
        break;

      case Instruction::IPUT:
      case Instruction::IPUT_QUICK:
        CompileInstanceFieldAccess(inst, dex_pc, Instruction::IPUT_QUICK, true);
        break;

      case Instruction::IPUT_BOOLEAN:
      case Instruction::IPUT_BOOLEAN_QUICK:
        CompileInstanceFieldAccess(inst, dex_pc, Instruction::IPUT_BOOLEAN_QUICK, true);
        break;

      case Instruction::IPUT_BYTE:
      case Instruction::IPUT_BYTE_QUICK:
        CompileInstanceFieldAccess(inst, dex_pc, Instruction::IPUT_BYTE_QUICK, true);
        break;

      case Instruction::IPUT_CHAR:
      case Instruction::IPUT_CHAR_QUICK:
        CompileInstanceFieldAccess(inst, dex_pc, Instruction::IPUT_CHAR_QUICK, true);
        break;

      case Instruction::IPUT_SHORT:
      case Instruction::IPUT_SHORT_QUICK:
        CompileInstanceFieldAccess(inst, dex_pc, Instruction::IPUT_SHORT_QUICK, true);
        break;

      case Instruction::IPUT_WIDE:
      case Instruction::IPUT_WIDE_QUICK:
        CompileInstanceFieldAccess(inst, dex_pc, Instruction::IPUT_WIDE_QUICK, true);
        break;

      case Instruction::IPUT_OBJECT:
      case Instruction::IPUT_OBJECT_QUICK:
        CompileInstanceFieldAccess(inst, dex_pc, Instruction::IPUT_OBJECT_QUICK, true);
        break;

      case Instruction::INVOKE_VIRTUAL:
      case Instruction::INVOKE_VIRTUAL_QUICK:
        CompileInvokeVirtual(inst, dex_pc, Instruction::INVOKE_VIRTUAL_QUICK, false);
        break;

      case Instruction::INVOKE_VIRTUAL_RANGE:
      case Instruction::INVOKE_VIRTUAL_RANGE_QUICK:
        CompileInvokeVirtual(inst, dex_pc, Instruction::INVOKE_VIRTUAL_RANGE_QUICK, true);
        break;

      case Instruction::NOP:
        if (already_quickened_) {
          const uint16_t reference_index = NextIndex();
          quickened_info_.push_back(QuickenedInfo(dex_pc, reference_index));
          if (reference_index == DexFile::kDexNoIndex16) {
            // This means it was a normal nop and not a check-cast.
            break;
          }
          const uint16_t type_index = NextIndex();
          if (driver_.IsSafeCast(&unit_, dex_pc)) {
            quickened_info_.push_back(QuickenedInfo(dex_pc, type_index));
          }
          ++it;
        } else {
          // We need to differentiate between check cast inserted NOP and normal NOP, put an invalid
          // index in the map for normal nops. This should be rare in real code.
          quickened_info_.push_back(QuickenedInfo(dex_pc, DexFile::kDexNoIndex16));
        }
        break;

      default:
        // Nothing to do.
        break;
    }
  }

  if (already_quickened_) {
    DCHECK_EQ(quicken_index_, existing_quicken_info_.NumIndices());
  }

  if (GetQuickenedInfo().empty()) {
    // No need to create a CompiledMethod if there are no quickened opcodes.
    return std::vector<uint8_t>();
  }

  std::vector<uint8_t> quicken_data;
  if (kIsDebugBuild) {
    // Double check that the counts line up with the size of the quicken info.
    size_t quicken_count = 0;
    for (const DexInstructionPcPair& pair : instructions) {
      if (QuickenInfoTable::NeedsIndexForInstruction(&pair.Inst())) {
        ++quicken_count;
      }
    }
    CHECK_EQ(quicken_count, GetQuickenedInfo().size());
  }

  QuickenInfoTable::Builder builder(&quicken_data, GetQuickenedInfo().size());
  // Length is encoded by the constructor.
  for (const CompilationState::QuickenedInfo& info : GetQuickenedInfo()) {
    // Dex pc is not serialized, only used for checking the instructions. Since we access the
    // array based on the index of the quickened instruction, the indexes must line up perfectly.
    // The reader side uses the NeedsIndexForInstruction function too.
    const Instruction& inst = instructions.InstructionAt(info.dex_pc);
    CHECK(QuickenInfoTable::NeedsIndexForInstruction(&inst)) << inst.Opcode();
    builder.AddIndex(info.dex_member_index);
  }
  DCHECK(!quicken_data.empty());
  return quicken_data;
}

void DexToDexCompiler::CompilationState::CompileReturnVoid(Instruction* inst, uint32_t dex_pc) {
  DCHECK_EQ(inst->Opcode(), Instruction::RETURN_VOID);
  if (unit_.IsConstructor()) {
    // Are we compiling a non clinit constructor which needs a barrier ?
    if (!unit_.IsStatic() &&
        driver_.RequiresConstructorBarrier(Thread::Current(), unit_.GetDexFile(),
                                           unit_.GetClassDefIndex())) {
      return;
    }
  }
  // Replace RETURN_VOID by RETURN_VOID_NO_BARRIER.
  VLOG(compiler) << "Replacing " << Instruction::Name(inst->Opcode())
                 << " by " << Instruction::Name(Instruction::RETURN_VOID_NO_BARRIER)
                 << " at dex pc " << StringPrintf("0x%x", dex_pc) << " in method "
                 << GetDexFile().PrettyMethod(unit_.GetDexMethodIndex(), true);
  inst->SetOpcode(Instruction::RETURN_VOID_NO_BARRIER);
}

Instruction* DexToDexCompiler::CompilationState::CompileCheckCast(Instruction* inst,
                                                                  uint32_t dex_pc) {
  if (!kEnableCheckCastEllision) {
    return inst;
  }
  if (!driver_.IsSafeCast(&unit_, dex_pc)) {
    return inst;
  }
  // Ok, this is a safe cast. Since the "check-cast" instruction size is 2 code
  // units and a "nop" instruction size is 1 code unit, we need to replace it by
  // 2 consecutive NOP instructions.
  // Because the caller loops over instructions by calling Instruction::Next onto
  // the current instruction, we need to return the 2nd NOP instruction. Indeed,
  // its next instruction is the former check-cast's next instruction.
  VLOG(compiler) << "Removing " << Instruction::Name(inst->Opcode())
                 << " by replacing it with 2 NOPs at dex pc "
                 << StringPrintf("0x%x", dex_pc) << " in method "
                 << GetDexFile().PrettyMethod(unit_.GetDexMethodIndex(), true);
  if (!already_quickened_) {
    quickened_info_.push_back(QuickenedInfo(dex_pc, inst->VRegA_21c()));
    quickened_info_.push_back(QuickenedInfo(dex_pc, inst->VRegB_21c()));

    // We are modifying 4 consecutive bytes.
    inst->SetOpcode(Instruction::NOP);
    inst->SetVRegA_10x(0u);  // keep compliant with verifier.
    // Get to next instruction which is the second half of check-cast and replace
    // it by a NOP.
    inst = const_cast<Instruction*>(inst->Next());
    inst->SetOpcode(Instruction::NOP);
    inst->SetVRegA_10x(0u);  // keep compliant with verifier.
  }
  return inst;
}

void DexToDexCompiler::CompilationState::CompileInstanceFieldAccess(Instruction* inst,
                                                                    uint32_t dex_pc,
                                                                    Instruction::Code new_opcode,
                                                                    bool is_put) {
  if (!kEnableQuickening) {
    return;
  }
  uint32_t field_idx = GetIndexForInstruction(inst, inst->VRegC_22c());
  MemberOffset field_offset(0u);
  bool is_volatile;
  bool fast_path = driver_.ComputeInstanceFieldInfo(field_idx, &unit_, is_put,
                                                    &field_offset, &is_volatile);
  if (fast_path && !is_volatile && IsUint<16>(field_offset.Int32Value())) {
    VLOG(compiler) << "Quickening " << Instruction::Name(inst->Opcode())
                   << " to " << Instruction::Name(new_opcode)
                   << " by replacing field index " << field_idx
                   << " by field offset " << field_offset.Int32Value()
                   << " at dex pc " << StringPrintf("0x%x", dex_pc) << " in method "
                   << GetDexFile().PrettyMethod(unit_.GetDexMethodIndex(), true);
    if (!already_quickened_) {
      // We are modifying 4 consecutive bytes.
      inst->SetOpcode(new_opcode);
      // Replace field index by field offset.
      inst->SetVRegC_22c(static_cast<uint16_t>(field_offset.Int32Value()));
    }
    quickened_info_.push_back(QuickenedInfo(dex_pc, field_idx));
  }
}

const DexFile& DexToDexCompiler::CompilationState::GetDexFile() const {
  return *unit_.GetDexFile();
}

void DexToDexCompiler::CompilationState::CompileInvokeVirtual(Instruction* inst,
                                                              uint32_t dex_pc,
                                                              Instruction::Code new_opcode,
                                                              bool is_range) {
  if (!kEnableQuickening) {
    return;
  }
  uint32_t method_idx = GetIndexForInstruction(inst,
                                               is_range ? inst->VRegB_3rc() : inst->VRegB_35c());
  ScopedObjectAccess soa(Thread::Current());

  ClassLinker* class_linker = unit_.GetClassLinker();
  ArtMethod* resolved_method =
      class_linker->ResolveMethod<ClassLinker::ResolveMode::kCheckICCEAndIAE>(
          method_idx,
          unit_.GetDexCache(),
          unit_.GetClassLoader(),
          /* referrer */ nullptr,
          kVirtual);

  if (UNLIKELY(resolved_method == nullptr)) {
    // Clean up any exception left by type resolution.
    soa.Self()->ClearException();
    return;
  }

  uint32_t vtable_idx = resolved_method->GetMethodIndex();
  DCHECK(IsUint<16>(vtable_idx));
  VLOG(compiler) << "Quickening " << Instruction::Name(inst->Opcode())
                 << "(" << GetDexFile().PrettyMethod(method_idx, true) << ")"
                 << " to " << Instruction::Name(new_opcode)
                 << " by replacing method index " << method_idx
                 << " by vtable index " << vtable_idx
                 << " at dex pc " << StringPrintf("0x%x", dex_pc) << " in method "
                 << GetDexFile().PrettyMethod(unit_.GetDexMethodIndex(), true);
  if (!already_quickened_) {
    // We are modifying 4 consecutive bytes.
    inst->SetOpcode(new_opcode);
    // Replace method index by vtable index.
    if (is_range) {
      inst->SetVRegB_3rc(static_cast<uint16_t>(vtable_idx));
    } else {
      inst->SetVRegB_35c(static_cast<uint16_t>(vtable_idx));
    }
  }
  quickened_info_.push_back(QuickenedInfo(dex_pc, method_idx));
}

CompiledMethod* DexToDexCompiler::CompileMethod(
    const DexFile::CodeItem* code_item,
    uint32_t access_flags,
    InvokeType invoke_type ATTRIBUTE_UNUSED,
    uint16_t class_def_idx,
    uint32_t method_idx,
    Handle<mirror::ClassLoader> class_loader,
    const DexFile& dex_file,
    CompilationLevel compilation_level) {
  if (compilation_level == CompilationLevel::kDontDexToDexCompile) {
    return nullptr;
  }

  ScopedObjectAccess soa(Thread::Current());
  StackHandleScope<1> hs(soa.Self());
  ClassLinker* const class_linker = Runtime::Current()->GetClassLinker();
  art::DexCompilationUnit unit(
      class_loader,
      class_linker,
      dex_file,
      code_item,
      class_def_idx,
      method_idx,
      access_flags,
      driver_->GetVerifiedMethod(&dex_file, method_idx),
      hs.NewHandle(class_linker->FindDexCache(soa.Self(), dex_file)));

  std::vector<uint8_t> quicken_data;
  // If the code item is shared with multiple different method ids, make sure that we quicken only
  // once and verify that all the dequicken maps match.
  if (UNLIKELY(shared_code_items_.find(code_item) != shared_code_items_.end())) {
    // For shared code items, use a lock to prevent races.
    MutexLock mu(soa.Self(), lock_);
    // Blacklisted means there was a quickening conflict previously, bail early.
    if (blacklisted_code_items_.find(code_item) != blacklisted_code_items_.end()) {
      return nullptr;
    }
    auto existing = shared_code_item_quicken_info_.find(code_item);
    const bool already_quickened = existing != shared_code_item_quicken_info_.end();
    {
      CompilationState state(this,
                             unit,
                             compilation_level,
                             already_quickened ? &existing->second.quicken_data_ : nullptr);
      quicken_data = state.Compile();
    }

    // Already quickened, check that the data matches what was previously seen.
    MethodReference method_ref(&dex_file, method_idx);
    if (already_quickened) {
      QuickenState* const existing_data = &existing->second;
      if (existing_data->quicken_data_ != quicken_data) {
        VLOG(compiler) << "Quicken data mismatch, dequickening method "
                       << dex_file.PrettyMethod(method_idx);
        // Unquicken using the existing quicken data.
        optimizer::ArtDecompileDEX(dex_file,
                                   *code_item,
                                   ArrayRef<const uint8_t>(existing_data->quicken_data_),
                                   /* decompile_return_instruction*/ false);
        // Go clear the vmaps for all the methods that were already quickened to avoid writing them
        // out during oat writing.
        for (const MethodReference& ref : existing_data->methods_) {
          CompiledMethod* method = driver_->GetCompiledMethod(ref);
          DCHECK(method != nullptr);
          method->ReleaseVMapTable();
        }
        // Blacklist the method to never attempt to quicken it in the future.
        blacklisted_code_items_.insert(code_item);
        shared_code_item_quicken_info_.erase(existing);
        return nullptr;
      }
      existing_data->methods_.push_back(method_ref);
    } else {
      QuickenState new_state;
      new_state.methods_.push_back(method_ref);
      new_state.quicken_data_ = quicken_data;
      bool inserted = shared_code_item_quicken_info_.emplace(code_item, new_state).second;
      CHECK(inserted) << "Failed to insert " << dex_file.PrettyMethod(method_idx);
    }

    // Easy sanity check is to check that the existing stuff matches by re-quickening using the
    // newly produced quicken data.
    // Note that this needs to be behind the lock for this case since we may unquicken in another
    // thread.
    if (kIsDebugBuild) {
      CompilationState state2(this, unit, compilation_level, &quicken_data);
      std::vector<uint8_t> new_data = state2.Compile();
      CHECK(new_data == quicken_data) << "Mismatch producing new quicken data";
    }
  } else {
    CompilationState state(this, unit, compilation_level, /*quicken_data*/ nullptr);
    quicken_data = state.Compile();

    // Easy sanity check is to check that the existing stuff matches by re-quickening using the
    // newly produced quicken data.
    if (kIsDebugBuild) {
      CompilationState state2(this, unit, compilation_level, &quicken_data);
      std::vector<uint8_t> new_data = state2.Compile();
      CHECK(new_data == quicken_data) << "Mismatch producing new quicken data";
    }
  }

  if (quicken_data.empty()) {
    return nullptr;
  }

  // Create a `CompiledMethod`, with the quickened information in the vmap table.
  InstructionSet instruction_set = driver_->GetInstructionSet();
  if (instruction_set == InstructionSet::kThumb2) {
    // Don't use the thumb2 instruction set to avoid the one off code delta.
    instruction_set = InstructionSet::kArm;
  }
  CompiledMethod* ret = CompiledMethod::SwapAllocCompiledMethod(
      driver_,
      instruction_set,
      ArrayRef<const uint8_t>(),                   // no code
      0,
      0,
      0,
      ArrayRef<const uint8_t>(),                   // method_info
      ArrayRef<const uint8_t>(quicken_data),       // vmap_table
      ArrayRef<const uint8_t>(),                   // cfi data
      ArrayRef<const linker::LinkerPatch>());
  return ret;
}

}  // namespace optimizer

}  // namespace art
