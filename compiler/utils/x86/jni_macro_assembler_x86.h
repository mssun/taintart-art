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

#ifndef ART_COMPILER_UTILS_X86_JNI_MACRO_ASSEMBLER_X86_H_
#define ART_COMPILER_UTILS_X86_JNI_MACRO_ASSEMBLER_X86_H_

#include <vector>

#include "assembler_x86.h"
#include "base/arena_containers.h"
#include "base/array_ref.h"
#include "base/enums.h"
#include "base/macros.h"
#include "offsets.h"
#include "utils/jni_macro_assembler.h"

namespace art {
namespace x86 {

class X86JNIMacroLabel;

class X86JNIMacroAssembler final : public JNIMacroAssemblerFwd<X86Assembler, PointerSize::k32> {
 public:
  explicit X86JNIMacroAssembler(ArenaAllocator* allocator) : JNIMacroAssemblerFwd(allocator) {}
  virtual ~X86JNIMacroAssembler() {}

  //
  // Overridden common assembler high-level functionality
  //

  // Emit code that will create an activation on the stack
  void BuildFrame(size_t frame_size,
                  ManagedRegister method_reg,
                  ArrayRef<const ManagedRegister> callee_save_regs,
                  const ManagedRegisterEntrySpills& entry_spills) override;

  // Emit code that will remove an activation from the stack
  void RemoveFrame(size_t frame_size,
                   ArrayRef<const ManagedRegister> callee_save_regs,
                   bool may_suspend) override;

  void IncreaseFrameSize(size_t adjust) override;
  void DecreaseFrameSize(size_t adjust) override;

  // Store routines
  void Store(FrameOffset offs, ManagedRegister src, size_t size) override;
  void StoreRef(FrameOffset dest, ManagedRegister src) override;
  void StoreRawPtr(FrameOffset dest, ManagedRegister src) override;

  void StoreImmediateToFrame(FrameOffset dest, uint32_t imm, ManagedRegister scratch) override;

  void StoreStackOffsetToThread(ThreadOffset32 thr_offs,
                                FrameOffset fr_offs,
                                ManagedRegister scratch) override;

  void StoreStackPointerToThread(ThreadOffset32 thr_offs) override;

  void StoreSpanning(FrameOffset dest, ManagedRegister src, FrameOffset in_off,
                     ManagedRegister scratch) override;

  // Load routines
  void Load(ManagedRegister dest, FrameOffset src, size_t size) override;

  void LoadFromThread(ManagedRegister dest, ThreadOffset32 src, size_t size) override;

  void LoadRef(ManagedRegister dest, FrameOffset src) override;

  void LoadRef(ManagedRegister dest, ManagedRegister base, MemberOffset offs,
               bool unpoison_reference) override;

  void LoadRawPtr(ManagedRegister dest, ManagedRegister base, Offset offs) override;

  void LoadRawPtrFromThread(ManagedRegister dest, ThreadOffset32 offs) override;

  // Copying routines
  void Move(ManagedRegister dest, ManagedRegister src, size_t size) override;

  void CopyRawPtrFromThread(FrameOffset fr_offs,
                            ThreadOffset32 thr_offs,
                            ManagedRegister scratch) override;

  void CopyRawPtrToThread(ThreadOffset32 thr_offs, FrameOffset fr_offs, ManagedRegister scratch)
      override;

  void CopyRef(FrameOffset dest, FrameOffset src, ManagedRegister scratch) override;

  void Copy(FrameOffset dest, FrameOffset src, ManagedRegister scratch, size_t size) override;

  void Copy(FrameOffset dest, ManagedRegister src_base, Offset src_offset, ManagedRegister scratch,
            size_t size) override;

  void Copy(ManagedRegister dest_base, Offset dest_offset, FrameOffset src, ManagedRegister scratch,
            size_t size) override;

  void Copy(FrameOffset dest, FrameOffset src_base, Offset src_offset, ManagedRegister scratch,
            size_t size) override;

  void Copy(ManagedRegister dest, Offset dest_offset, ManagedRegister src, Offset src_offset,
            ManagedRegister scratch, size_t size) override;

  void Copy(FrameOffset dest, Offset dest_offset, FrameOffset src, Offset src_offset,
            ManagedRegister scratch, size_t size) override;

  void MemoryBarrier(ManagedRegister) override;

  // Sign extension
  void SignExtend(ManagedRegister mreg, size_t size) override;

  // Zero extension
  void ZeroExtend(ManagedRegister mreg, size_t size) override;

  // Exploit fast access in managed code to Thread::Current()
  void GetCurrentThread(ManagedRegister tr) override;
  void GetCurrentThread(FrameOffset dest_offset, ManagedRegister scratch) override;

  // Set up out_reg to hold a Object** into the handle scope, or to be null if the
  // value is null and null_allowed. in_reg holds a possibly stale reference
  // that can be used to avoid loading the handle scope entry to see if the value is
  // null.
  void CreateHandleScopeEntry(ManagedRegister out_reg, FrameOffset handlescope_offset,
                              ManagedRegister in_reg, bool null_allowed) override;

  // Set up out_off to hold a Object** into the handle scope, or to be null if the
  // value is null and null_allowed.
  void CreateHandleScopeEntry(FrameOffset out_off, FrameOffset handlescope_offset,
                              ManagedRegister scratch, bool null_allowed) override;

  // src holds a handle scope entry (Object**) load this into dst
  void LoadReferenceFromHandleScope(ManagedRegister dst, ManagedRegister src) override;

  // Heap::VerifyObject on src. In some cases (such as a reference to this) we
  // know that src may not be null.
  void VerifyObject(ManagedRegister src, bool could_be_null) override;
  void VerifyObject(FrameOffset src, bool could_be_null) override;

  // Call to address held at [base+offset]
  void Call(ManagedRegister base, Offset offset, ManagedRegister scratch) override;
  void Call(FrameOffset base, Offset offset, ManagedRegister scratch) override;
  void CallFromThread(ThreadOffset32 offset, ManagedRegister scratch) override;

  // Generate code to check if Thread::Current()->exception_ is non-null
  // and branch to a ExceptionSlowPath if it is.
  void ExceptionPoll(ManagedRegister scratch, size_t stack_adjust) override;

  // Create a new label that can be used with Jump/Bind calls.
  std::unique_ptr<JNIMacroLabel> CreateLabel() override;
  // Emit an unconditional jump to the label.
  void Jump(JNIMacroLabel* label) override;
  // Emit a conditional jump to the label by applying a unary condition test to the register.
  void Jump(JNIMacroLabel* label, JNIMacroUnaryCondition cond, ManagedRegister test) override;
  // Code at this offset will serve as the target for the Jump call.
  void Bind(JNIMacroLabel* label) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(X86JNIMacroAssembler);
};

class X86JNIMacroLabel final
    : public JNIMacroLabelCommon<X86JNIMacroLabel,
                                 art::Label,
                                 InstructionSet::kX86> {
 public:
  art::Label* AsX86() {
    return AsPlatformLabel();
  }
};

}  // namespace x86
}  // namespace art

#endif  // ART_COMPILER_UTILS_X86_JNI_MACRO_ASSEMBLER_X86_H_
