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

// Returns true if we should warn about non-boot class path accessing member
// with `access_flags`.
inline bool ShouldWarnAboutMember(uint32_t access_flags) {
  if (!Runtime::Current()->AreHiddenApiChecksEnabled()) {
    return false;
  }

  switch (HiddenApiAccessFlags::DecodeFromRuntime(access_flags)) {
    case HiddenApiAccessFlags::kWhitelist:
      return false;
    case HiddenApiAccessFlags::kLightGreylist:
    case HiddenApiAccessFlags::kDarkGreylist:
      return true;
    case HiddenApiAccessFlags::kBlacklist:
      // We should never access a blacklisted member from non-boot class path,
      // but this function is called before we establish the origin of the access.
      // Return false here, we do not want to warn when boot class path accesses
      // a blacklisted member.
      return false;
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

// Issue a warning about field access.
inline void WarnAboutMemberAccess(ArtField* field) REQUIRES_SHARED(Locks::mutator_lock_) {
  Runtime::Current()->SetPendingHiddenApiWarning(true);
  LOG(WARNING) << "Access to hidden field " << field->PrettyField();
}

// Issue a warning about method access.
inline void WarnAboutMemberAccess(ArtMethod* method) REQUIRES_SHARED(Locks::mutator_lock_) {
  Runtime::Current()->SetPendingHiddenApiWarning(true);
  LOG(WARNING) << "Access to hidden method " << method->PrettyMethod();
}

// Set access flags of `member` to be in hidden API whitelist. This can be disabled
// with a Runtime::SetDedupHiddenApiWarnings.
template<typename T>
inline void MaybeWhitelistMember(T* member) REQUIRES_SHARED(Locks::mutator_lock_) {
  if (Runtime::Current()->ShouldDedupeHiddenApiWarnings()) {
    member->SetAccessFlags(HiddenApiAccessFlags::EncodeForRuntime(
        member->GetAccessFlags(), HiddenApiAccessFlags::kWhitelist));
    DCHECK(!ShouldWarnAboutMember(member->GetAccessFlags()));
  }
}

// Check if `caller` should be allowed to access `member` and warn if not.
template<typename T>
inline void MaybeWarnAboutMemberAccess(T* member, ArtMethod* caller)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  DCHECK(member != nullptr);
  DCHECK(!caller->IsRuntimeMethod());
  if (!Runtime::Current()->AreHiddenApiChecksEnabled() ||
      member == nullptr ||
      !ShouldWarnAboutMember(member->GetAccessFlags()) ||
      caller->GetDeclaringClass()->IsBootStrapClassLoaded()) {
    return;
  }

  WarnAboutMember(member);
  MaybeWhitelistMember(member);
}

// Check if the caller `num_frames` up the stack should be allowed to access
// `member` and warn if not.
template<typename T>
inline void MaybeWarnAboutMemberAccess(T* member, Thread* self, size_t num_frames)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  if (!Runtime::Current()->AreHiddenApiChecksEnabled() ||
      member == nullptr ||
      !ShouldWarnAboutMember(member->GetAccessFlags())) {
    return;
  }

  // Walk the stack to find the caller. This is *very* expensive. Save it for last.
  ObjPtr<mirror::Class> klass = GetCallingClass(self, num_frames);
  if (klass == nullptr) {
    // Unattached native thread, assume that this is *not* boot class path
    // and enforce the rules.
  } else if (klass->IsBootStrapClassLoaded()) {
    return;
  }

  WarnAboutMemberAccess(member);
  MaybeWhitelistMember(member);
}

}  // namespace hiddenapi
}  // namespace art

#endif  // ART_RUNTIME_HIDDEN_API_H_
