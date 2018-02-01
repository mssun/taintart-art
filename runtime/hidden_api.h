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

enum Action {
  kAllow,
  kAllowButWarn,
  kDeny
};

inline Action GetMemberAction(uint32_t access_flags) {
  switch (HiddenApiAccessFlags::DecodeFromRuntime(access_flags)) {
    case HiddenApiAccessFlags::kWhitelist:
      return kAllow;
    case HiddenApiAccessFlags::kLightGreylist:
    case HiddenApiAccessFlags::kDarkGreylist:
      return kAllowButWarn;
    case HiddenApiAccessFlags::kBlacklist:
      return kDeny;
  }
}

// Issue a warning about field access.
inline void WarnAboutMemberAccess(ArtField* field) REQUIRES_SHARED(Locks::mutator_lock_) {
  std::string tmp;
  LOG(WARNING) << "Accessing hidden field "
               << field->GetDeclaringClass()->GetDescriptor(&tmp) << "->"
               << field->GetName() << ":" << field->GetTypeDescriptor();
}

// Issue a warning about method access.
inline void WarnAboutMemberAccess(ArtMethod* method) REQUIRES_SHARED(Locks::mutator_lock_) {
  std::string tmp;
  LOG(WARNING) << "Accessing hidden method "
               << method->GetDeclaringClass()->GetDescriptor(&tmp) << "->"
               << method->GetName() << method->GetSignature().ToString();
}

// Returns true if access to `member` should be denied to the caller of the
// reflective query. The decision is based on whether the caller is in boot
// class path or not. Because different users of this function determine this
// in a different way, `fn_caller_in_boot(self)` is called and should return
// true if the caller is in boot class path.
// This function might print warnings into the log if the member is greylisted.
template<typename T>
inline bool ShouldBlockAccessToMember(T* member,
                                      Thread* self,
                                      std::function<bool(Thread*)> fn_caller_in_boot)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  DCHECK(member != nullptr);

  if (!Runtime::Current()->AreHiddenApiChecksEnabled()) {
    // Exit early. Nothing to enforce.
    return false;
  }

  Action action = GetMemberAction(member->GetAccessFlags());
  if (action == kAllow) {
    // Nothing to do.
    return false;
  }

  // Member is hidden. Walk the stack to find the caller.
  // This can be *very* expensive. Save it for last.
  if (fn_caller_in_boot(self)) {
    // Caller in boot class path. Exit.
    return false;
  }

  // Member is hidden and we are not in the boot class path. Act accordingly.
  if (action == kAllowButWarn) {
    // Allow access to this member but print a warning. Depending on a runtime
    // flag, we might move the member into whitelist and skip the warning the
    // next time the member is used.
    Runtime::Current()->SetPendingHiddenApiWarning(true);
    if (Runtime::Current()->ShouldDedupeHiddenApiWarnings()) {
      member->SetAccessFlags(HiddenApiAccessFlags::EncodeForRuntime(
          member->GetAccessFlags(), HiddenApiAccessFlags::kWhitelist));
    }
    WarnAboutMemberAccess(member);
    return false;
  } else {
    DCHECK_EQ(action, hiddenapi::kDeny);
    return true;
  }
}

// Returns true if access to member with `access_flags` should be denied to `caller`.
// This function should be called on statically linked uses of hidden API.
inline bool ShouldBlockAccessToMember(uint32_t access_flags, mirror::Class* caller)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  if (!Runtime::Current()->AreHiddenApiChecksEnabled()) {
    // Exit early. Nothing to enforce.
    return false;
  }

  // Only continue if we want to deny access. Warnings are *not* printed.
  if (GetMemberAction(access_flags) != kDeny) {
    return false;
  }

  // Member is hidden. Check if the caller is in boot class path.
  if (caller == nullptr) {
    // The caller is unknown. We assume that this is *not* boot class path.
    return true;
  }

  return !caller->IsBootStrapClassLoaded();
}

}  // namespace hiddenapi
}  // namespace art

#endif  // ART_RUNTIME_HIDDEN_API_H_
