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

#include "art_field-inl.h"
#include "art_method-inl.h"
#include "base/mutex.h"
#include "dex/hidden_api_access_flags.h"
#include "mirror/class-inl.h"
#include "reflection.h"
#include "runtime.h"

namespace art {
namespace hiddenapi {

// Hidden API enforcement policy
// This must be kept in sync with ApplicationInfo.ApiEnforcementPolicy in
// frameworks/base/core/java/android/content/pm/ApplicationInfo.java
enum class EnforcementPolicy {
  kNoChecks             = 0,
  kAllLists             = 1,  // ban anything but whitelist
  kDarkGreyAndBlackList = 2,  // ban dark grey & blacklist
  kBlacklistOnly        = 3,  // ban blacklist violations only
  kMax = kBlacklistOnly,
};

inline EnforcementPolicy EnforcementPolicyFromInt(int api_policy_int) {
  DCHECK_GE(api_policy_int, 0);
  DCHECK_LE(api_policy_int, static_cast<int>(EnforcementPolicy::kMax));
  return static_cast<EnforcementPolicy>(api_policy_int);
}

enum Action {
  kAllow,
  kAllowButWarn,
  kAllowButWarnAndToast,
  kDeny
};

enum AccessMethod {
  kReflection,
  kJNI,
  kLinking,
};

inline Action GetActionFromAccessFlags(uint32_t access_flags) {
  EnforcementPolicy policy = Runtime::Current()->GetHiddenApiEnforcementPolicy();
  if (policy == EnforcementPolicy::kNoChecks) {
    // Exit early. Nothing to enforce.
    return kAllow;
  }

  HiddenApiAccessFlags::ApiList api_list = HiddenApiAccessFlags::DecodeFromRuntime(access_flags);
  if (api_list == HiddenApiAccessFlags::kWhitelist) {
    return kAllow;
  }
  // The logic below relies on equality of values in the enums EnforcementPolicy and
  // HiddenApiAccessFlags::ApiList, and their ordering. Assertions are in hidden_api.cc.
  if (static_cast<int>(policy) > static_cast<int>(api_list)) {
    return api_list == HiddenApiAccessFlags::kDarkGreylist
        ? kAllowButWarnAndToast
        : kAllowButWarn;
  } else {
    return kDeny;
  }
}

// Implementation details. DO NOT ACCESS DIRECTLY.
namespace detail {

// Class to encapsulate the signature of a member (ArtField or ArtMethod). This
// is used as a helper when matching prefixes, and when logging the signature.
class MemberSignature {
 private:
  std::string member_type_;
  std::vector<std::string> signature_parts_;
  std::string tmp_;

 public:
  explicit MemberSignature(ArtField* field) REQUIRES_SHARED(Locks::mutator_lock_);
  explicit MemberSignature(ArtMethod* method) REQUIRES_SHARED(Locks::mutator_lock_);

  void Dump(std::ostream& os) const;

  // Performs prefix match on this member. Since the full member signature is
  // composed of several parts, we match each part in turn (rather than
  // building the entire thing in memory and performing a simple prefix match)
  bool DoesPrefixMatch(const std::string& prefix) const;

  bool IsExempted(const std::vector<std::string>& exemptions);

  void WarnAboutAccess(AccessMethod access_method, HiddenApiAccessFlags::ApiList list);
};

template<typename T>
Action GetMemberActionImpl(T* member, Action action, AccessMethod access_method)
    REQUIRES_SHARED(Locks::mutator_lock_);

// Returns true if the caller is either loaded by the boot strap class loader or comes from
// a dex file located in ${ANDROID_ROOT}/framework/.
ALWAYS_INLINE
inline bool IsCallerInPlatformDex(ObjPtr<mirror::ClassLoader> caller_class_loader,
                                  ObjPtr<mirror::DexCache> caller_dex_cache)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  if (caller_class_loader.IsNull()) {
    return true;
  } else if (caller_dex_cache.IsNull()) {
    return false;
  } else {
    const DexFile* caller_dex_file = caller_dex_cache->GetDexFile();
    return caller_dex_file != nullptr && caller_dex_file->IsPlatformDexFile();
  }
}

}  // namespace detail

// Returns true if access to `member` should be denied to the caller of the
// reflective query. The decision is based on whether the caller is in the
// platform or not. Because different users of this function determine this
// in a different way, `fn_caller_in_platform(self)` is called and should
// return true if the caller is located in the platform.
// This function might print warnings into the log if the member is hidden.
template<typename T>
inline Action GetMemberAction(T* member,
                              Thread* self,
                              std::function<bool(Thread*)> fn_caller_in_platform,
                              AccessMethod access_method)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  DCHECK(member != nullptr);

  Action action = GetActionFromAccessFlags(member->GetAccessFlags());
  if (action == kAllow) {
    // Nothing to do.
    return action;
  }

  // Member is hidden. Invoke `fn_caller_in_platform` and find the origin of the access.
  // This can be *very* expensive. Save it for last.
  if (fn_caller_in_platform(self)) {
    // Caller in the platform. Exit.
    return kAllow;
  }

  // Member is hidden and caller is not in the platform.
  return detail::GetMemberActionImpl(member, action, access_method);
}

inline bool IsCallerInPlatformDex(ObjPtr<mirror::Class> caller)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  return !caller.IsNull() &&
      detail::IsCallerInPlatformDex(caller->GetClassLoader(), caller->GetDexCache());
}

// Returns true if access to `member` should be denied to a caller loaded with
// `caller_class_loader`.
// This function might print warnings into the log if the member is hidden.
template<typename T>
inline Action GetMemberAction(T* member,
                              ObjPtr<mirror::ClassLoader> caller_class_loader,
                              ObjPtr<mirror::DexCache> caller_dex_cache,
                              AccessMethod access_method)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  bool caller_in_platform = detail::IsCallerInPlatformDex(caller_class_loader, caller_dex_cache);
  return GetMemberAction(member,
                         /* thread */ nullptr,
                         [caller_in_platform] (Thread*) { return caller_in_platform; },
                         access_method);
}

// Calls back into managed code to notify VMRuntime.nonSdkApiUsageConsumer that
// |member| was accessed. This is usually called when an API is on the black,
// dark grey or light grey lists. Given that the callback can execute arbitrary
// code, a call to this method can result in thread suspension.
template<typename T> void NotifyHiddenApiListener(T* member)
    REQUIRES_SHARED(Locks::mutator_lock_);


}  // namespace hiddenapi
}  // namespace art

#endif  // ART_RUNTIME_HIDDEN_API_H_
