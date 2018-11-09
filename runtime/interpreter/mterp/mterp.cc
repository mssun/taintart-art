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

/*
 * Mterp entry point and support functions.
 */
#include "mterp.h"

#include "base/quasi_atomic.h"
#include "debugger.h"
#include "entrypoints/entrypoint_utils-inl.h"
#include "interpreter/interpreter_common.h"
#include "interpreter/interpreter_intrinsics.h"
#include "interpreter/shadow_frame-inl.h"
#include "mirror/string-alloc-inl.h"

namespace art {
namespace interpreter {
/*
 * Verify some constants used by the mterp interpreter.
 */
void CheckMterpAsmConstants() {
  /*
   * If we're using computed goto instruction transitions, make sure
   * none of the handlers overflows the byte limit.  This won't tell
   * which one did, but if any one is too big the total size will
   * overflow.
   */
  const int width = kMterpHandlerSize;
  int interp_size = (uintptr_t) artMterpAsmInstructionEnd -
                    (uintptr_t) artMterpAsmInstructionStart;
  if ((interp_size == 0) || (interp_size != (art::kNumPackedOpcodes * width))) {
      LOG(FATAL) << "ERROR: unexpected asm interp size " << interp_size
                 << "(did an instruction handler exceed " << width << " bytes?)";
  }
}

void InitMterpTls(Thread* self) {
  self->SetMterpCurrentIBase(artMterpAsmInstructionStart);
}

/*
 * Find the matching case.  Returns the offset to the handler instructions.
 *
 * Returns 3 if we don't find a match (it's the size of the sparse-switch
 * instruction).
 */
extern "C" ssize_t MterpDoSparseSwitch(const uint16_t* switchData, int32_t testVal) {
  const int kInstrLen = 3;
  uint16_t size;
  const int32_t* keys;
  const int32_t* entries;

  /*
   * Sparse switch data format:
   *  ushort ident = 0x0200   magic value
   *  ushort size             number of entries in the table; > 0
   *  int keys[size]          keys, sorted low-to-high; 32-bit aligned
   *  int targets[size]       branch targets, relative to switch opcode
   *
   * Total size is (2+size*4) 16-bit code units.
   */

  uint16_t signature = *switchData++;
  DCHECK_EQ(signature, static_cast<uint16_t>(art::Instruction::kSparseSwitchSignature));

  size = *switchData++;

  /* The keys are guaranteed to be aligned on a 32-bit boundary;
   * we can treat them as a native int array.
   */
  keys = reinterpret_cast<const int32_t*>(switchData);

  /* The entries are guaranteed to be aligned on a 32-bit boundary;
   * we can treat them as a native int array.
   */
  entries = keys + size;

  /*
   * Binary-search through the array of keys, which are guaranteed to
   * be sorted low-to-high.
   */
  int lo = 0;
  int hi = size - 1;
  while (lo <= hi) {
    int mid = (lo + hi) >> 1;

    int32_t foundVal = keys[mid];
    if (testVal < foundVal) {
      hi = mid - 1;
    } else if (testVal > foundVal) {
      lo = mid + 1;
    } else {
      return entries[mid];
    }
  }
  return kInstrLen;
}

extern "C" ssize_t MterpDoPackedSwitch(const uint16_t* switchData, int32_t testVal) {
  const int kInstrLen = 3;

  /*
   * Packed switch data format:
   *  ushort ident = 0x0100   magic value
   *  ushort size             number of entries in the table
   *  int first_key           first (and lowest) switch case value
   *  int targets[size]       branch targets, relative to switch opcode
   *
   * Total size is (4+size*2) 16-bit code units.
   */
  uint16_t signature = *switchData++;
  DCHECK_EQ(signature, static_cast<uint16_t>(art::Instruction::kPackedSwitchSignature));

  uint16_t size = *switchData++;

  int32_t firstKey = *switchData++;
  firstKey |= (*switchData++) << 16;

  int index = testVal - firstKey;
  if (index < 0 || index >= size) {
    return kInstrLen;
  }

  /*
   * The entries are guaranteed to be aligned on a 32-bit boundary;
   * we can treat them as a native int array.
   */
  const int32_t* entries = reinterpret_cast<const int32_t*>(switchData);
  return entries[index];
}

bool CanUseMterp()
    REQUIRES_SHARED(Locks::mutator_lock_) {
  const Runtime* const runtime = Runtime::Current();
  return
      runtime->IsStarted() &&
      !runtime->IsAotCompiler() &&
      !Dbg::IsDebuggerActive() &&
      !runtime->GetInstrumentation()->IsActive() &&
      // mterp only knows how to deal with the normal exits. It cannot handle any of the
      // non-standard force-returns.
      !runtime->AreNonStandardExitsEnabled() &&
      // An async exception has been thrown. We need to go to the switch interpreter. MTerp doesn't
      // know how to deal with these so we could end up never dealing with it if we are in an
      // infinite loop.
      !runtime->AreAsyncExceptionsThrown() &&
      (runtime->GetJit() == nullptr || !runtime->GetJit()->JitAtFirstUse());
}


extern "C" size_t MterpInvokeVirtual(Thread* self,
                                     ShadowFrame* shadow_frame,
                                     uint16_t* dex_pc_ptr,
                                     uint16_t inst_data)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  JValue* result_register = shadow_frame->GetResultRegister();
  const Instruction* inst = Instruction::At(dex_pc_ptr);
  return DoInvoke<kVirtual, /*is_range=*/ false, /*do_access_check=*/ false, /*is_mterp=*/ true>(
      self, *shadow_frame, inst, inst_data, result_register);
}

extern "C" size_t MterpInvokeSuper(Thread* self,
                                   ShadowFrame* shadow_frame,
                                   uint16_t* dex_pc_ptr,
                                   uint16_t inst_data)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  JValue* result_register = shadow_frame->GetResultRegister();
  const Instruction* inst = Instruction::At(dex_pc_ptr);
  return DoInvoke<kSuper, /*is_range=*/ false, /*do_access_check=*/ false, /*is_mterp=*/ true>(
      self, *shadow_frame, inst, inst_data, result_register);
}

extern "C" size_t MterpInvokeInterface(Thread* self,
                                       ShadowFrame* shadow_frame,
                                       uint16_t* dex_pc_ptr,
                                       uint16_t inst_data)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  JValue* result_register = shadow_frame->GetResultRegister();
  const Instruction* inst = Instruction::At(dex_pc_ptr);
  return DoInvoke<kInterface, /*is_range=*/ false, /*do_access_check=*/ false, /*is_mterp=*/ true>(
      self, *shadow_frame, inst, inst_data, result_register);
}

extern "C" size_t MterpInvokeDirect(Thread* self,
                                    ShadowFrame* shadow_frame,
                                    uint16_t* dex_pc_ptr,
                                    uint16_t inst_data)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  JValue* result_register = shadow_frame->GetResultRegister();
  const Instruction* inst = Instruction::At(dex_pc_ptr);
  return DoInvoke<kDirect, /*is_range=*/ false, /*do_access_check=*/ false, /*is_mterp=*/ true>(
      self, *shadow_frame, inst, inst_data, result_register);
}

extern "C" size_t MterpInvokeStatic(Thread* self,
                                    ShadowFrame* shadow_frame,
                                    uint16_t* dex_pc_ptr,
                                    uint16_t inst_data)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  JValue* result_register = shadow_frame->GetResultRegister();
  const Instruction* inst = Instruction::At(dex_pc_ptr);
  return DoInvoke<kStatic, /*is_range=*/ false, /*do_access_check=*/ false, /*is_mterp=*/ true>(
      self, *shadow_frame, inst, inst_data, result_register);
}

extern "C" size_t MterpInvokeCustom(Thread* self,
                                    ShadowFrame* shadow_frame,
                                    uint16_t* dex_pc_ptr,
                                    uint16_t inst_data)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  JValue* result_register = shadow_frame->GetResultRegister();
  const Instruction* inst = Instruction::At(dex_pc_ptr);
  return DoInvokeCustom</* is_range= */ false>(
      self, *shadow_frame, inst, inst_data, result_register);
}

extern "C" size_t MterpInvokePolymorphic(Thread* self,
                                         ShadowFrame* shadow_frame,
                                         uint16_t* dex_pc_ptr,
                                         uint16_t inst_data)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  JValue* result_register = shadow_frame->GetResultRegister();
  const Instruction* inst = Instruction::At(dex_pc_ptr);
  return DoInvokePolymorphic</* is_range= */ false>(
      self, *shadow_frame, inst, inst_data, result_register);
}

extern "C" size_t MterpInvokeVirtualRange(Thread* self,
                                          ShadowFrame* shadow_frame,
                                          uint16_t* dex_pc_ptr,
                                          uint16_t inst_data)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  JValue* result_register = shadow_frame->GetResultRegister();
  const Instruction* inst = Instruction::At(dex_pc_ptr);
  return DoInvoke<kVirtual, /*is_range=*/ true, /*do_access_check=*/ false, /*is_mterp=*/ true>(
      self, *shadow_frame, inst, inst_data, result_register);
}

extern "C" size_t MterpInvokeSuperRange(Thread* self,
                                        ShadowFrame* shadow_frame,
                                        uint16_t* dex_pc_ptr,
                                        uint16_t inst_data)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  JValue* result_register = shadow_frame->GetResultRegister();
  const Instruction* inst = Instruction::At(dex_pc_ptr);
  return DoInvoke<kSuper, /*is_range=*/ true, /*do_access_check=*/ false, /*is_mterp=*/ true>(
      self, *shadow_frame, inst, inst_data, result_register);
}

extern "C" size_t MterpInvokeInterfaceRange(Thread* self,
                                            ShadowFrame* shadow_frame,
                                            uint16_t* dex_pc_ptr,
                                            uint16_t inst_data)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  JValue* result_register = shadow_frame->GetResultRegister();
  const Instruction* inst = Instruction::At(dex_pc_ptr);
  return DoInvoke<kInterface, /*is_range=*/ true, /*do_access_check=*/ false, /*is_mterp=*/ true>(
      self, *shadow_frame, inst, inst_data, result_register);
}

extern "C" size_t MterpInvokeDirectRange(Thread* self,
                                         ShadowFrame* shadow_frame,
                                         uint16_t* dex_pc_ptr,
                                         uint16_t inst_data)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  JValue* result_register = shadow_frame->GetResultRegister();
  const Instruction* inst = Instruction::At(dex_pc_ptr);
  return DoInvoke<kDirect, /*is_range=*/ true, /*do_access_check=*/ false, /*is_mterp=*/ true>(
      self, *shadow_frame, inst, inst_data, result_register);
}

extern "C" size_t MterpInvokeStaticRange(Thread* self,
                                         ShadowFrame* shadow_frame,
                                         uint16_t* dex_pc_ptr,
                                         uint16_t inst_data)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  JValue* result_register = shadow_frame->GetResultRegister();
  const Instruction* inst = Instruction::At(dex_pc_ptr);
  return DoInvoke<kStatic, /*is_range=*/ true, /*do_access_check=*/ false, /*is_mterp=*/ true>(
      self, *shadow_frame, inst, inst_data, result_register);
}

extern "C" size_t MterpInvokeCustomRange(Thread* self,
                                         ShadowFrame* shadow_frame,
                                         uint16_t* dex_pc_ptr,
                                         uint16_t inst_data)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  JValue* result_register = shadow_frame->GetResultRegister();
  const Instruction* inst = Instruction::At(dex_pc_ptr);
  return DoInvokeCustom</*is_range=*/ true>(self, *shadow_frame, inst, inst_data, result_register);
}

extern "C" size_t MterpInvokePolymorphicRange(Thread* self,
                                              ShadowFrame* shadow_frame,
                                              uint16_t* dex_pc_ptr,
                                              uint16_t inst_data)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  JValue* result_register = shadow_frame->GetResultRegister();
  const Instruction* inst = Instruction::At(dex_pc_ptr);
  return DoInvokePolymorphic</* is_range= */ true>(
      self, *shadow_frame, inst, inst_data, result_register);
}

extern "C" size_t MterpInvokeVirtualQuick(Thread* self,
                                          ShadowFrame* shadow_frame,
                                          uint16_t* dex_pc_ptr,
                                          uint16_t inst_data)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  JValue* result_register = shadow_frame->GetResultRegister();
  const Instruction* inst = Instruction::At(dex_pc_ptr);
  return DoInvoke<kVirtual, /*is_range=*/ false, /*do_access_check=*/ false, /*is_mterp=*/ true,
      /*is_quick=*/ true>(self, *shadow_frame, inst, inst_data, result_register);
}

extern "C" size_t MterpInvokeVirtualQuickRange(Thread* self,
                                               ShadowFrame* shadow_frame,
                                               uint16_t* dex_pc_ptr,
                                               uint16_t inst_data)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  JValue* result_register = shadow_frame->GetResultRegister();
  const Instruction* inst = Instruction::At(dex_pc_ptr);
  return DoInvoke<kVirtual, /*is_range=*/ true, /*do_access_check=*/ false, /*is_mterp=*/ true,
      /*is_quick=*/ true>(self, *shadow_frame, inst, inst_data, result_register);
}

extern "C" void MterpThreadFenceForConstructor() {
  QuasiAtomic::ThreadFenceForConstructor();
}

extern "C" size_t MterpConstString(uint32_t index,
                                   uint32_t tgt_vreg,
                                   ShadowFrame* shadow_frame,
                                   Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ObjPtr<mirror::String> s = ResolveString(self, *shadow_frame, dex::StringIndex(index));
  if (UNLIKELY(s == nullptr)) {
    return true;
  }
  shadow_frame->SetVRegReference(tgt_vreg, s);
  return false;
}

extern "C" size_t MterpConstClass(uint32_t index,
                                  uint32_t tgt_vreg,
                                  ShadowFrame* shadow_frame,
                                  Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ObjPtr<mirror::Class> c = ResolveVerifyAndClinit(dex::TypeIndex(index),
                                                   shadow_frame->GetMethod(),
                                                   self,
                                                   /* can_run_clinit= */ false,
                                                   /* verify_access= */ false);
  if (UNLIKELY(c == nullptr)) {
    return true;
  }
  shadow_frame->SetVRegReference(tgt_vreg, c);
  return false;
}

extern "C" size_t MterpConstMethodHandle(uint32_t index,
                                         uint32_t tgt_vreg,
                                         ShadowFrame* shadow_frame,
                                         Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ObjPtr<mirror::MethodHandle> mh = ResolveMethodHandle(self, index, shadow_frame->GetMethod());
  if (UNLIKELY(mh == nullptr)) {
    return true;
  }
  shadow_frame->SetVRegReference(tgt_vreg, mh);
  return false;
}

extern "C" size_t MterpConstMethodType(uint32_t index,
                                       uint32_t tgt_vreg,
                                       ShadowFrame* shadow_frame,
                                       Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ObjPtr<mirror::MethodType> mt =
      ResolveMethodType(self, dex::ProtoIndex(index), shadow_frame->GetMethod());
  if (UNLIKELY(mt == nullptr)) {
    return true;
  }
  shadow_frame->SetVRegReference(tgt_vreg, mt);
  return false;
}

extern "C" size_t MterpCheckCast(uint32_t index,
                                 StackReference<mirror::Object>* vreg_addr,
                                 art::ArtMethod* method,
                                 Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ObjPtr<mirror::Class> c = ResolveVerifyAndClinit(dex::TypeIndex(index),
                                                   method,
                                                   self,
                                                   false,
                                                   false);
  if (UNLIKELY(c == nullptr)) {
    return true;
  }
  // Must load obj from vreg following ResolveVerifyAndClinit due to moving gc.
  mirror::Object* obj = vreg_addr->AsMirrorPtr();
  if (UNLIKELY(obj != nullptr && !obj->InstanceOf(c))) {
    ThrowClassCastException(c, obj->GetClass());
    return true;
  }
  return false;
}

extern "C" size_t MterpInstanceOf(uint32_t index,
                                  StackReference<mirror::Object>* vreg_addr,
                                  art::ArtMethod* method,
                                  Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ObjPtr<mirror::Class> c = ResolveVerifyAndClinit(dex::TypeIndex(index),
                                                   method,
                                                   self,
                                                   false,
                                                   false);
  if (UNLIKELY(c == nullptr)) {
    return false;  // Caller will check for pending exception.  Return value unimportant.
  }
  // Must load obj from vreg following ResolveVerifyAndClinit due to moving gc.
  mirror::Object* obj = vreg_addr->AsMirrorPtr();
  return (obj != nullptr) && obj->InstanceOf(c);
}

extern "C" size_t MterpFillArrayData(mirror::Object* obj, const Instruction::ArrayDataPayload* payload)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  return FillArrayData(obj, payload);
}

extern "C" size_t MterpNewInstance(ShadowFrame* shadow_frame, Thread* self, uint32_t inst_data)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  const Instruction* inst = Instruction::At(shadow_frame->GetDexPCPtr());
  mirror::Object* obj = nullptr;
  ObjPtr<mirror::Class> c = ResolveVerifyAndClinit(dex::TypeIndex(inst->VRegB_21c()),
                                                   shadow_frame->GetMethod(),
                                                   self,
                                                   /* can_run_clinit= */ false,
                                                   /* verify_access= */ false);
  if (LIKELY(c != nullptr)) {
    if (UNLIKELY(c->IsStringClass())) {
      gc::AllocatorType allocator_type = Runtime::Current()->GetHeap()->GetCurrentAllocator();
      obj = mirror::String::AllocEmptyString<true>(self, allocator_type);
    } else {
      obj = AllocObjectFromCode<true>(c.Ptr(),
                                      self,
                                      Runtime::Current()->GetHeap()->GetCurrentAllocator());
    }
  }
  if (UNLIKELY(obj == nullptr)) {
    return false;
  }
  obj->GetClass()->AssertInitializedOrInitializingInThread(self);
  shadow_frame->SetVRegReference(inst->VRegA_21c(inst_data), obj);
  return true;
}

extern "C" size_t MterpIputObjectQuick(ShadowFrame* shadow_frame,
                                       uint16_t* dex_pc_ptr,
                                       uint32_t inst_data)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  const Instruction* inst = Instruction::At(dex_pc_ptr);
  return DoIPutQuick<Primitive::kPrimNot, false>(*shadow_frame, inst, inst_data);
}

extern "C" size_t MterpAputObject(ShadowFrame* shadow_frame,
                                  uint16_t* dex_pc_ptr,
                                  uint32_t inst_data)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  const Instruction* inst = Instruction::At(dex_pc_ptr);
  mirror::Object* a = shadow_frame->GetVRegReference(inst->VRegB_23x());
  if (UNLIKELY(a == nullptr)) {
    return false;
  }
  int32_t index = shadow_frame->GetVReg(inst->VRegC_23x());
  mirror::Object* val = shadow_frame->GetVRegReference(inst->VRegA_23x(inst_data));
  mirror::ObjectArray<mirror::Object>* array = a->AsObjectArray<mirror::Object>();
  if (array->CheckIsValidIndex(index) && array->CheckAssignable(val)) {
    array->SetWithoutChecks<false>(index, val);
    return true;
  }
  return false;
}

extern "C" size_t MterpFilledNewArray(ShadowFrame* shadow_frame,
                                      uint16_t* dex_pc_ptr,
                                      Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  const Instruction* inst = Instruction::At(dex_pc_ptr);
  return DoFilledNewArray<false, false, false>(inst, *shadow_frame, self,
                                               shadow_frame->GetResultRegister());
}

extern "C" size_t MterpFilledNewArrayRange(ShadowFrame* shadow_frame,
                                           uint16_t* dex_pc_ptr,
                                           Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  const Instruction* inst = Instruction::At(dex_pc_ptr);
  return DoFilledNewArray<true, false, false>(inst, *shadow_frame, self,
                                              shadow_frame->GetResultRegister());
}

extern "C" size_t MterpNewArray(ShadowFrame* shadow_frame,
                                uint16_t* dex_pc_ptr,
                                uint32_t inst_data, Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  const Instruction* inst = Instruction::At(dex_pc_ptr);
  int32_t length = shadow_frame->GetVReg(inst->VRegB_22c(inst_data));
  ObjPtr<mirror::Object> obj = AllocArrayFromCode<false, true>(
      dex::TypeIndex(inst->VRegC_22c()), length, shadow_frame->GetMethod(), self,
      Runtime::Current()->GetHeap()->GetCurrentAllocator());
  if (UNLIKELY(obj == nullptr)) {
      return false;
  }
  shadow_frame->SetVRegReference(inst->VRegA_22c(inst_data), obj);
  return true;
}

extern "C" size_t MterpHandleException(Thread* self, ShadowFrame* shadow_frame)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  DCHECK(self->IsExceptionPending());
  const instrumentation::Instrumentation* const instrumentation =
      Runtime::Current()->GetInstrumentation();
  return MoveToExceptionHandler(self, *shadow_frame, instrumentation);
}

extern "C" void MterpCheckBefore(Thread* self, ShadowFrame* shadow_frame, uint16_t* dex_pc_ptr)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  // Check that we are using the right interpreter.
  if (kIsDebugBuild && self->UseMterp() != CanUseMterp()) {
    // The flag might be currently being updated on all threads. Retry with lock.
    MutexLock tll_mu(self, *Locks::thread_list_lock_);
    DCHECK_EQ(self->UseMterp(), CanUseMterp());
  }
  DCHECK(!Runtime::Current()->IsActiveTransaction());
  const Instruction* inst = Instruction::At(dex_pc_ptr);
  uint16_t inst_data = inst->Fetch16(0);
  if (inst->Opcode(inst_data) == Instruction::MOVE_EXCEPTION) {
    self->AssertPendingException();
  } else {
    self->AssertNoPendingException();
  }
  if (kTraceExecutionEnabled) {
    uint32_t dex_pc = dex_pc_ptr - shadow_frame->GetDexInstructions();
    TraceExecution(*shadow_frame, inst, dex_pc);
  }
  if (kTestExportPC) {
    // Save invalid dex pc to force segfault if improperly used.
    shadow_frame->SetDexPCPtr(reinterpret_cast<uint16_t*>(kExportPCPoison));
  }
}

extern "C" void MterpLogDivideByZeroException(Thread* self, ShadowFrame* shadow_frame)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  UNUSED(self);
  const Instruction* inst = Instruction::At(shadow_frame->GetDexPCPtr());
  uint16_t inst_data = inst->Fetch16(0);
  LOG(INFO) << "DivideByZero: " << inst->Opcode(inst_data);
}

extern "C" void MterpLogArrayIndexException(Thread* self, ShadowFrame* shadow_frame)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  UNUSED(self);
  const Instruction* inst = Instruction::At(shadow_frame->GetDexPCPtr());
  uint16_t inst_data = inst->Fetch16(0);
  LOG(INFO) << "ArrayIndex: " << inst->Opcode(inst_data);
}

extern "C" void MterpLogNegativeArraySizeException(Thread* self, ShadowFrame* shadow_frame)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  UNUSED(self);
  const Instruction* inst = Instruction::At(shadow_frame->GetDexPCPtr());
  uint16_t inst_data = inst->Fetch16(0);
  LOG(INFO) << "NegativeArraySize: " << inst->Opcode(inst_data);
}

extern "C" void MterpLogNoSuchMethodException(Thread* self, ShadowFrame* shadow_frame)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  UNUSED(self);
  const Instruction* inst = Instruction::At(shadow_frame->GetDexPCPtr());
  uint16_t inst_data = inst->Fetch16(0);
  LOG(INFO) << "NoSuchMethod: " << inst->Opcode(inst_data);
}

extern "C" void MterpLogExceptionThrownException(Thread* self, ShadowFrame* shadow_frame)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  UNUSED(self);
  const Instruction* inst = Instruction::At(shadow_frame->GetDexPCPtr());
  uint16_t inst_data = inst->Fetch16(0);
  LOG(INFO) << "ExceptionThrown: " << inst->Opcode(inst_data);
}

extern "C" void MterpLogNullObjectException(Thread* self, ShadowFrame* shadow_frame)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  UNUSED(self);
  const Instruction* inst = Instruction::At(shadow_frame->GetDexPCPtr());
  uint16_t inst_data = inst->Fetch16(0);
  LOG(INFO) << "NullObject: " << inst->Opcode(inst_data);
}

extern "C" void MterpLogFallback(Thread* self, ShadowFrame* shadow_frame)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  UNUSED(self);
  const Instruction* inst = Instruction::At(shadow_frame->GetDexPCPtr());
  uint16_t inst_data = inst->Fetch16(0);
  LOG(INFO) << "Fallback: " << inst->Opcode(inst_data) << ", Suspend Pending?: "
            << self->IsExceptionPending();
}

extern "C" void MterpLogOSR(Thread* self, ShadowFrame* shadow_frame, int32_t offset)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  UNUSED(self);
  const Instruction* inst = Instruction::At(shadow_frame->GetDexPCPtr());
  uint16_t inst_data = inst->Fetch16(0);
  LOG(INFO) << "OSR: " << inst->Opcode(inst_data) << ", offset = " << offset;
}

extern "C" void MterpLogSuspendFallback(Thread* self, ShadowFrame* shadow_frame, uint32_t flags)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  UNUSED(self);
  const Instruction* inst = Instruction::At(shadow_frame->GetDexPCPtr());
  uint16_t inst_data = inst->Fetch16(0);
  if (flags & kCheckpointRequest) {
    LOG(INFO) << "Checkpoint fallback: " << inst->Opcode(inst_data);
  } else if (flags & kSuspendRequest) {
    LOG(INFO) << "Suspend fallback: " << inst->Opcode(inst_data);
  } else if (flags & kEmptyCheckpointRequest) {
    LOG(INFO) << "Empty checkpoint fallback: " << inst->Opcode(inst_data);
  }
}

extern "C" size_t MterpSuspendCheck(Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  self->AllowThreadSuspension();
  return !self->UseMterp();
}

// Execute single field access instruction (get/put, static/instance).
// The template arguments reduce this to fairly small amount of code.
// It requires the target object and field to be already resolved.
template<typename PrimType, FindFieldType kAccessType>
ALWAYS_INLINE void MterpFieldAccess(Instruction* inst,
                                    uint16_t inst_data,
                                    ShadowFrame* shadow_frame,
                                    ObjPtr<mirror::Object> obj,
                                    MemberOffset offset,
                                    bool is_volatile)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  static_assert(std::is_integral<PrimType>::value, "Unexpected primitive type");
  constexpr bool kIsStatic = (kAccessType & FindFieldFlags::StaticBit) != 0;
  constexpr bool kIsPrimitive = (kAccessType & FindFieldFlags::PrimitiveBit) != 0;
  constexpr bool kIsRead = (kAccessType & FindFieldFlags::ReadBit) != 0;

  uint16_t vRegA = kIsStatic ? inst->VRegA_21c(inst_data) : inst->VRegA_22c(inst_data);
  if (kIsPrimitive) {
    if (kIsRead) {
      PrimType value = UNLIKELY(is_volatile)
          ? obj->GetFieldPrimitive<PrimType, /*kIsVolatile=*/ true>(offset)
          : obj->GetFieldPrimitive<PrimType, /*kIsVolatile=*/ false>(offset);
      if (sizeof(PrimType) == sizeof(uint64_t)) {
        shadow_frame->SetVRegLong(vRegA, value);  // Set two consecutive registers.
      } else {
        shadow_frame->SetVReg(vRegA, static_cast<int32_t>(value));  // Sign/zero extend.
      }
    } else {  // Write.
      uint64_t value = (sizeof(PrimType) == sizeof(uint64_t))
          ? shadow_frame->GetVRegLong(vRegA)
          : shadow_frame->GetVReg(vRegA);
      if (UNLIKELY(is_volatile)) {
        obj->SetFieldPrimitive<PrimType, /*kIsVolatile=*/ true>(offset, value);
      } else {
        obj->SetFieldPrimitive<PrimType, /*kIsVolatile=*/ false>(offset, value);
      }
    }
  } else {  // Object.
    if (kIsRead) {
      ObjPtr<mirror::Object> value = UNLIKELY(is_volatile)
          ? obj->GetFieldObjectVolatile<mirror::Object>(offset)
          : obj->GetFieldObject<mirror::Object>(offset);
      shadow_frame->SetVRegReference(vRegA, value);
    } else {  // Write.
      ObjPtr<mirror::Object> value = shadow_frame->GetVRegReference(vRegA);
      if (UNLIKELY(is_volatile)) {
        obj->SetFieldObjectVolatile</*kTransactionActive=*/ false>(offset, value);
      } else {
        obj->SetFieldObject</*kTransactionActive=*/ false>(offset, value);
      }
    }
  }
}

template<typename PrimType, FindFieldType kAccessType>
NO_INLINE bool MterpFieldAccessSlow(Instruction* inst,
                                    uint16_t inst_data,
                                    ShadowFrame* shadow_frame,
                                    Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  constexpr bool kIsStatic = (kAccessType & FindFieldFlags::StaticBit) != 0;
  constexpr bool kIsRead = (kAccessType & FindFieldFlags::ReadBit) != 0;

  // Update the dex pc in shadow frame, just in case anything throws.
  shadow_frame->SetDexPCPtr(reinterpret_cast<uint16_t*>(inst));
  ArtMethod* referrer = shadow_frame->GetMethod();
  uint32_t field_idx = kIsStatic ? inst->VRegB_21c() : inst->VRegC_22c();
  ArtField* field = FindFieldFromCode<kAccessType, /* access_checks= */ false>(
      field_idx, referrer, self, sizeof(PrimType));
  if (UNLIKELY(field == nullptr)) {
    DCHECK(self->IsExceptionPending());
    return false;
  }
  ObjPtr<mirror::Object> obj = kIsStatic
      ? field->GetDeclaringClass().Ptr()
      : shadow_frame->GetVRegReference(inst->VRegB_22c(inst_data));
  if (UNLIKELY(obj == nullptr)) {
    ThrowNullPointerExceptionForFieldAccess(field, kIsRead);
    return false;
  }
  MterpFieldAccess<PrimType, kAccessType>(
      inst, inst_data, shadow_frame, obj, field->GetOffset(), field->IsVolatile());
  return true;
}

// This methods is called from assembly to handle field access instructions.
//
// This method is fairly hot.  It is long, but it has been carefully optimized.
// It contains only fully inlined methods -> no spills -> no prologue/epilogue.
template<typename PrimType, FindFieldType kAccessType>
ALWAYS_INLINE bool MterpFieldAccessFast(Instruction* inst,
                                        uint16_t inst_data,
                                        ShadowFrame* shadow_frame,
                                        Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  constexpr bool kIsStatic = (kAccessType & FindFieldFlags::StaticBit) != 0;

  // Try to find the field in small thread-local cache first.
  InterpreterCache* tls_cache = self->GetInterpreterCache();
  size_t tls_value;
  if (LIKELY(tls_cache->Get(inst, &tls_value))) {
    // The meaning of the cache value is opcode-specific.
    // It is ArtFiled* for static fields and the raw offset for instance fields.
    size_t offset = kIsStatic
        ? reinterpret_cast<ArtField*>(tls_value)->GetOffset().SizeValue()
        : tls_value;
    if (kIsDebugBuild) {
      uint32_t field_idx = kIsStatic ? inst->VRegB_21c() : inst->VRegC_22c();
      ArtField* field = FindFieldFromCode<kAccessType, /* access_checks= */ false>(
          field_idx, shadow_frame->GetMethod(), self, sizeof(PrimType));
      DCHECK_EQ(offset, field->GetOffset().SizeValue());
    }
    ObjPtr<mirror::Object> obj = kIsStatic
        ? reinterpret_cast<ArtField*>(tls_value)->GetDeclaringClass()
        : MakeObjPtr(shadow_frame->GetVRegReference(inst->VRegB_22c(inst_data)));
    if (LIKELY(obj != nullptr)) {
      MterpFieldAccess<PrimType, kAccessType>(
          inst, inst_data, shadow_frame, obj, MemberOffset(offset), /* is_volatile= */ false);
      return true;
    }
  }

  // This effectively inlines the fast path from ArtMethod::GetDexCache.
  ArtMethod* referrer = shadow_frame->GetMethod();
  if (LIKELY(!referrer->IsObsolete())) {
    // Avoid read barriers, since we need only the pointer to the native (non-movable)
    // DexCache field array which we can get even through from-space objects.
    ObjPtr<mirror::Class> klass = referrer->GetDeclaringClass<kWithoutReadBarrier>();
    mirror::DexCache* dex_cache = klass->GetDexCache<kDefaultVerifyFlags, kWithoutReadBarrier>();

    // Try to find the desired field in DexCache.
    uint32_t field_idx = kIsStatic ? inst->VRegB_21c() : inst->VRegC_22c();
    ArtField* field = dex_cache->GetResolvedField(field_idx, kRuntimePointerSize);
    if (LIKELY(field != nullptr)) {
      bool initialized = !kIsStatic || field->GetDeclaringClass()->IsInitialized();
      if (LIKELY(initialized)) {
        DCHECK_EQ(field, (FindFieldFromCode<kAccessType, /* access_checks= */ false>(
            field_idx, referrer, self, sizeof(PrimType))));
        ObjPtr<mirror::Object> obj = kIsStatic
            ? field->GetDeclaringClass().Ptr()
            : shadow_frame->GetVRegReference(inst->VRegB_22c(inst_data));
        if (LIKELY(kIsStatic || obj != nullptr)) {
          // Only non-volatile fields are allowed in the thread-local cache.
          if (LIKELY(!field->IsVolatile())) {
            if (kIsStatic) {
              tls_cache->Set(inst, reinterpret_cast<uintptr_t>(field));
            } else {
              tls_cache->Set(inst, field->GetOffset().SizeValue());
            }
          }
          MterpFieldAccess<PrimType, kAccessType>(
              inst, inst_data, shadow_frame, obj, field->GetOffset(), field->IsVolatile());
          return true;
        }
      }
    }
  }

  // Slow path. Last and with identical arguments so that it becomes single instruction tail call.
  return MterpFieldAccessSlow<PrimType, kAccessType>(inst, inst_data, shadow_frame, self);
}

#define MTERP_FIELD_ACCESSOR(Name, PrimType, AccessType)                                          \
extern "C" bool Name(Instruction* inst, uint16_t inst_data, ShadowFrame* sf, Thread* self)        \
    REQUIRES_SHARED(Locks::mutator_lock_) {                                                       \
  return MterpFieldAccessFast<PrimType, AccessType>(inst, inst_data, sf, self);                   \
}

#define MTERP_FIELD_ACCESSORS_FOR_TYPE(Sufix, PrimType, Kind)                                     \
  MTERP_FIELD_ACCESSOR(MterpIGet##Sufix, PrimType, Instance##Kind##Read)                          \
  MTERP_FIELD_ACCESSOR(MterpIPut##Sufix, PrimType, Instance##Kind##Write)                         \
  MTERP_FIELD_ACCESSOR(MterpSGet##Sufix, PrimType, Static##Kind##Read)                            \
  MTERP_FIELD_ACCESSOR(MterpSPut##Sufix, PrimType, Static##Kind##Write)

MTERP_FIELD_ACCESSORS_FOR_TYPE(I8, int8_t, Primitive)
MTERP_FIELD_ACCESSORS_FOR_TYPE(U8, uint8_t, Primitive)
MTERP_FIELD_ACCESSORS_FOR_TYPE(I16, int16_t, Primitive)
MTERP_FIELD_ACCESSORS_FOR_TYPE(U16, uint16_t, Primitive)
MTERP_FIELD_ACCESSORS_FOR_TYPE(U32, uint32_t, Primitive)
MTERP_FIELD_ACCESSORS_FOR_TYPE(U64, uint64_t, Primitive)
MTERP_FIELD_ACCESSORS_FOR_TYPE(Obj, uint32_t, Object)

// Check that the primitive type for Obj variant above is correct.
// It really must be primitive type for the templates to compile.
// In the case of objects, it is only used to get the field size.
static_assert(kHeapReferenceSize == sizeof(uint32_t), "Unexpected kHeapReferenceSize");

#undef MTERP_FIELD_ACCESSORS_FOR_TYPE
#undef MTERP_FIELD_ACCESSOR

extern "C" mirror::Object* artAGetObjectFromMterp(mirror::Object* arr,
                                                  int32_t index)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  if (UNLIKELY(arr == nullptr)) {
    ThrowNullPointerExceptionFromInterpreter();
    return nullptr;
  }
  mirror::ObjectArray<mirror::Object>* array = arr->AsObjectArray<mirror::Object>();
  if (LIKELY(array->CheckIsValidIndex(index))) {
    return array->GetWithoutChecks(index);
  } else {
    return nullptr;
  }
}

extern "C" mirror::Object* artIGetObjectFromMterp(mirror::Object* obj,
                                                  uint32_t field_offset)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  if (UNLIKELY(obj == nullptr)) {
    ThrowNullPointerExceptionFromInterpreter();
    return nullptr;
  }
  return obj->GetFieldObject<mirror::Object>(MemberOffset(field_offset));
}

/*
 * Create a hotness_countdown based on the current method hotness_count and profiling
 * mode.  In short, determine how many hotness events we hit before reporting back
 * to the full instrumentation via MterpAddHotnessBatch.  Called once on entry to the method,
 * and regenerated following batch updates.
 */
extern "C" ssize_t MterpSetUpHotnessCountdown(ArtMethod* method,
                                              ShadowFrame* shadow_frame,
                                              Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  uint16_t hotness_count = method->GetCounter();
  int32_t countdown_value = jit::kJitHotnessDisabled;
  jit::Jit* jit = Runtime::Current()->GetJit();
  if (jit != nullptr) {
    // We need to add batch size to ensure the threshold gets passed even after rounding.
    constexpr int32_t kBatchSize = jit::kJitSamplesBatchSize;
    int32_t warm_threshold = static_cast<int32_t>(jit->WarmMethodThreshold()) + kBatchSize;
    int32_t hot_threshold = static_cast<int32_t>(jit->HotMethodThreshold()) + kBatchSize;
    int32_t osr_threshold = static_cast<int32_t>(jit->OSRMethodThreshold()) + kBatchSize;
    if (hotness_count < warm_threshold) {
      countdown_value = warm_threshold - hotness_count;
    } else if (hotness_count < hot_threshold) {
      countdown_value = hot_threshold - hotness_count;
    } else if (hotness_count < osr_threshold) {
      countdown_value = osr_threshold - hotness_count;
    } else {
      countdown_value = jit::kJitCheckForOSR;
    }
    if (jit::Jit::ShouldUsePriorityThreadWeight(self)) {
      int32_t priority_thread_weight = jit->PriorityThreadWeight();
      countdown_value = std::min(countdown_value, countdown_value / priority_thread_weight);
    }
  }
  /*
   * The actual hotness threshold may exceed the range of our int16_t countdown value.  This is
   * not a problem, though.  We can just break it down into smaller chunks.
   */
  countdown_value = std::min(countdown_value,
                             static_cast<int32_t>(std::numeric_limits<int16_t>::max()));
  shadow_frame->SetCachedHotnessCountdown(countdown_value);
  shadow_frame->SetHotnessCountdown(countdown_value);
  return countdown_value;
}

/*
 * Report a batch of hotness events to the instrumentation and then return the new
 * countdown value to the next time we should report.
 */
extern "C" ssize_t MterpAddHotnessBatch(ArtMethod* method,
                                        ShadowFrame* shadow_frame,
                                        Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  jit::Jit* jit = Runtime::Current()->GetJit();
  if (jit != nullptr) {
    int16_t count = shadow_frame->GetCachedHotnessCountdown() - shadow_frame->GetHotnessCountdown();
    jit->AddSamples(self, method, count, /*with_backedges=*/ true);
  }
  return MterpSetUpHotnessCountdown(method, shadow_frame, self);
}

extern "C" size_t MterpMaybeDoOnStackReplacement(Thread* self,
                                                 ShadowFrame* shadow_frame,
                                                 int32_t offset)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  int16_t osr_countdown = shadow_frame->GetCachedHotnessCountdown() - 1;
  bool did_osr = false;
  /*
   * To reduce the cost of polling the compiler to determine whether the requested OSR
   * compilation has completed, only check every Nth time.  NOTE: the "osr_countdown <= 0"
   * condition is satisfied either by the decrement below or the initial setting of
   * the cached countdown field to kJitCheckForOSR, which elsewhere is asserted to be -1.
   */
  if (osr_countdown <= 0) {
    ArtMethod* method = shadow_frame->GetMethod();
    JValue* result = shadow_frame->GetResultRegister();
    uint32_t dex_pc = shadow_frame->GetDexPC();
    jit::Jit* jit = Runtime::Current()->GetJit();
    osr_countdown = jit::Jit::kJitRecheckOSRThreshold;
    if (offset <= 0) {
      // Keep updating hotness in case a compilation request was dropped.  Eventually it will retry.
      jit->AddSamples(self, method, osr_countdown, /*with_backedges=*/ true);
    }
    did_osr = jit::Jit::MaybeDoOnStackReplacement(self, method, dex_pc, offset, result);
  }
  shadow_frame->SetCachedHotnessCountdown(osr_countdown);
  return did_osr;
}

}  // namespace interpreter
}  // namespace art
