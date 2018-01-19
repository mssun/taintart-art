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

#ifndef ART_RUNTIME_HIDDEN_API_H_
#define ART_RUNTIME_HIDDEN_API_H_

#include "hidden_api_access_flags.h"
#include "reflection.h"
#include "runtime.h"

namespace art {
namespace hiddenapi {

// Returns true if member with `access flags` should only be accessed from
// boot class path.
inline bool IsMemberHidden(uint32_t access_flags) {
  if (!Runtime::Current()->AreHiddenApiChecksEnabled()) {
    return false;
  }

  switch (HiddenApiAccessFlags::DecodeFromRuntime(access_flags)) {
    case HiddenApiAccessFlags::kWhitelist:
    case HiddenApiAccessFlags::kLightGreylist:
    case HiddenApiAccessFlags::kDarkGreylist:
      return false;
    case HiddenApiAccessFlags::kBlacklist:
      return true;
  }
}

// Returns true if caller `num_frames` up the stack is in boot class path.
inline bool IsCallerInBootClassPath(Thread* self, size_t num_frames)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ObjPtr<mirror::Class> klass = GetCallingClass(self, num_frames);
  if (klass == nullptr) {
    // Unattached native thread. Assume that this is *not* boot class path.
    return false;
  }
  return klass->IsBootStrapClassLoaded();
}

// Returns true if `caller` should not be allowed to access member with `access_flags`.
inline bool ShouldBlockAccessToMember(uint32_t access_flags, mirror::Class* caller)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  return IsMemberHidden(access_flags) &&
         !caller->IsBootStrapClassLoaded();
}

// Returns true if `caller` should not be allowed to access `member`.
template<typename T>
inline bool ShouldBlockAccessToMember(T* member, ArtMethod* caller)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  DCHECK(member != nullptr);
  DCHECK(!caller->IsRuntimeMethod());
  return ShouldBlockAccessToMember(member->GetAccessFlags(), caller->GetDeclaringClass());
}

// Returns true if the caller `num_frames` up the stack should not be allowed
// to access `member`.
template<typename T>
inline bool ShouldBlockAccessToMember(T* member, Thread* self, size_t num_frames)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  DCHECK(member != nullptr);
  return IsMemberHidden(member->GetAccessFlags()) &&
         !IsCallerInBootClassPath(self, num_frames);  // This is expensive. Save it for last.
}

}  // namespace hiddenapi
}  // namespace art

#endif  // ART_RUNTIME_HIDDEN_API_H_
