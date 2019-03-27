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

#include "var_handles.h"

#include "common_throws.h"
#include "dex/dex_instruction.h"
#include "handle.h"
#include "method_handles-inl.h"
#include "mirror/method_type-inl.h"
#include "mirror/var_handle.h"

namespace art {

namespace {

bool VarHandleInvokeAccessorWithConversions(Thread* self,
                                            ShadowFrame& shadow_frame,
                                            Handle<mirror::VarHandle> var_handle,
                                            Handle<mirror::MethodType> callsite_type,
                                            const mirror::VarHandle::AccessMode access_mode,
                                            const InstructionOperands* const operands,
                                            JValue* result)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  StackHandleScope<1> hs(self);
  Handle<mirror::MethodType> accessor_type(hs.NewHandle(
      var_handle->GetMethodTypeForAccessMode(self, access_mode)));
  const size_t num_vregs = accessor_type->NumberOfVRegs();
  const int num_params = accessor_type->GetPTypes()->GetLength();
  ShadowFrameAllocaUniquePtr accessor_frame =
      CREATE_SHADOW_FRAME(num_vregs, nullptr, shadow_frame.GetMethod(), shadow_frame.GetDexPC());
  ShadowFrameGetter getter(shadow_frame, operands);
  static const uint32_t kFirstDestinationReg = 0;
  ShadowFrameSetter setter(accessor_frame.get(), kFirstDestinationReg);
  if (!PerformConversions(self, callsite_type, accessor_type, &getter, &setter, num_params)) {
    return false;
  }
  RangeInstructionOperands accessor_operands(kFirstDestinationReg,
                                             kFirstDestinationReg + num_vregs);
  if (!var_handle->Access(access_mode, accessor_frame.get(), &accessor_operands, result)) {
    return false;
  }
  return ConvertReturnValue(callsite_type, accessor_type, result);
}

}  // namespace

bool VarHandleInvokeAccessor(Thread* self,
                             ShadowFrame& shadow_frame,
                             Handle<mirror::VarHandle> var_handle,
                             Handle<mirror::MethodType> callsite_type,
                             const mirror::VarHandle::AccessMode access_mode,
                             const InstructionOperands* const operands,
                             JValue* result) {
  if (var_handle.IsNull()) {
    ThrowNullPointerExceptionFromDexPC();
    return false;
  }

  if (!var_handle->IsAccessModeSupported(access_mode)) {
    ThrowUnsupportedOperationException();
    return false;
  }

  mirror::VarHandle::MatchKind match_kind =
      var_handle->GetMethodTypeMatchForAccessMode(access_mode, callsite_type.Get());
  if (LIKELY(match_kind == mirror::VarHandle::MatchKind::kExact)) {
    return var_handle->Access(access_mode, &shadow_frame, operands, result);
  } else if (match_kind == mirror::VarHandle::MatchKind::kWithConversions) {
    return VarHandleInvokeAccessorWithConversions(self,
                                                  shadow_frame,
                                                  var_handle,
                                                  callsite_type,
                                                  access_mode,
                                                  operands,
                                                  result);
  } else {
    DCHECK_EQ(match_kind, mirror::VarHandle::MatchKind::kNone);
    ThrowWrongMethodTypeException(var_handle->PrettyDescriptorForAccessMode(access_mode),
                                  callsite_type->PrettyDescriptor());
    return false;
  }
}

}  // namespace art
