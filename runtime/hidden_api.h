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
#include "base/dumpable.h"
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

inline std::ostream& operator<<(std::ostream& os, AccessMethod value) {
  switch (value) {
    case kReflection:
      os << "reflection";
      break;
    case kJNI:
      os << "JNI";
      break;
    case kLinking:
      os << "linking";
      break;
  }
  return os;
}

static constexpr bool EnumsEqual(EnforcementPolicy policy, HiddenApiAccessFlags::ApiList apiList) {
  return static_cast<int>(policy) == static_cast<int>(apiList);
}

inline Action GetMemberAction(uint32_t access_flags) {
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
  // HiddenApiAccessFlags::ApiList, and their ordering. Assert that this is as expected.
  static_assert(
      EnumsEqual(EnforcementPolicy::kAllLists, HiddenApiAccessFlags::kLightGreylist) &&
      EnumsEqual(EnforcementPolicy::kDarkGreyAndBlackList, HiddenApiAccessFlags::kDarkGreylist) &&
      EnumsEqual(EnforcementPolicy::kBlacklistOnly, HiddenApiAccessFlags::kBlacklist),
      "Mismatch between EnforcementPolicy and ApiList enums");
  static_assert(
      EnforcementPolicy::kAllLists < EnforcementPolicy::kDarkGreyAndBlackList &&
      EnforcementPolicy::kDarkGreyAndBlackList < EnforcementPolicy::kBlacklistOnly,
      "EnforcementPolicy values ordering not correct");
  if (static_cast<int>(policy) > static_cast<int>(api_list)) {
    return api_list == HiddenApiAccessFlags::kDarkGreylist
        ? kAllowButWarnAndToast
        : kAllowButWarn;
  } else {
    return kDeny;
  }
}


// Class to encapsulate the signature of a member (ArtField or ArtMethod). This
// is used as a helper when matching prefixes, and when logging the signature.
class MemberSignature {
 private:
  std::string member_type_;
  std::vector<std::string> signature_parts_;
  std::string tmp_;

 public:
  explicit MemberSignature(ArtField* field) REQUIRES_SHARED(Locks::mutator_lock_) {
    member_type_ = "field";
    signature_parts_ = {
      field->GetDeclaringClass()->GetDescriptor(&tmp_),
      "->",
      field->GetName(),
      ":",
      field->GetTypeDescriptor()
    };
  }

  explicit MemberSignature(ArtMethod* method) REQUIRES_SHARED(Locks::mutator_lock_) {
    member_type_ = "method";
    signature_parts_ = {
      method->GetDeclaringClass()->GetDescriptor(&tmp_),
      "->",
      method->GetName(),
      method->GetSignature().ToString()
    };
  }

  const std::vector<std::string>& Parts() const {
    return signature_parts_;
  }

  void Dump(std::ostream& os) const {
    for (std::string part : signature_parts_) {
      os << part;
    }
  }
  // Performs prefix match on this member. Since the full member signature is
  // composed of several parts, we match each part in turn (rather than
  // building the entire thing in memory and performing a simple prefix match)
  bool DoesPrefixMatch(const std::string& prefix) const {
    size_t pos = 0;
    for (const std::string& part : signature_parts_) {
      size_t count = std::min(prefix.length() - pos, part.length());
      if (prefix.compare(pos, count, part, 0, count) == 0) {
        pos += count;
      } else {
        return false;
      }
    }
    // We have a complete match if all parts match (we exit the loop without
    // returning) AND we've matched the whole prefix.
    return pos == prefix.length();
  }

  bool IsExempted(const std::vector<std::string>& exemptions) {
    for (const std::string& exemption : exemptions) {
      if (DoesPrefixMatch(exemption)) {
        return true;
      }
    }
    return false;
  }

  void WarnAboutAccess(AccessMethod access_method, HiddenApiAccessFlags::ApiList list) {
    LOG(WARNING) << "Accessing hidden " << member_type_ << " " << Dumpable<MemberSignature>(*this)
                 << " (" << list << ", " << access_method << ")";
  }
};

// Returns true if access to `member` should be denied to the caller of the
// reflective query. The decision is based on whether the caller is in boot
// class path or not. Because different users of this function determine this
// in a different way, `fn_caller_in_boot(self)` is called and should return
// true if the caller is in boot class path.
// This function might print warnings into the log if the member is hidden.
template<typename T>
inline bool ShouldBlockAccessToMember(T* member,
                                      Thread* self,
                                      std::function<bool(Thread*)> fn_caller_in_boot,
                                      AccessMethod access_method)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  DCHECK(member != nullptr);

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

  // Member is hidden and we are not in the boot class path.

  // Get the signature, we need it later.
  MemberSignature member_signature(member);

  Runtime* runtime = Runtime::Current();

  if (action == kDeny) {
    // If we were about to deny, check for an exemption first.
    // Exempted APIs are treated as light grey list.
    if (member_signature.IsExempted(runtime->GetHiddenApiExemptions())) {
      action = kAllowButWarn;
      // Avoid re-examining the exemption list next time.
      // Note this results in the warning below showing "light greylist", which
      // seems like what one would expect. Exemptions effectively add new members to
      // the light greylist.
      member->SetAccessFlags(HiddenApiAccessFlags::EncodeForRuntime(
              member->GetAccessFlags(), HiddenApiAccessFlags::kLightGreylist));
    }
  }

  // Print a log message with information about this class member access.
  // We do this regardless of whether we block the access or not.
  member_signature.WarnAboutAccess(access_method,
      HiddenApiAccessFlags::DecodeFromRuntime(member->GetAccessFlags()));

  if (action == kDeny) {
    // Block access
    return true;
  }

  // Allow access to this member but print a warning.
  DCHECK(action == kAllowButWarn || action == kAllowButWarnAndToast);

  // Depending on a runtime flag, we might move the member into whitelist and
  // skip the warning the next time the member is accessed.
  if (runtime->ShouldDedupeHiddenApiWarnings()) {
    member->SetAccessFlags(HiddenApiAccessFlags::EncodeForRuntime(
        member->GetAccessFlags(), HiddenApiAccessFlags::kWhitelist));
  }

  // If this action requires a UI warning, set the appropriate flag.
  if (action == kAllowButWarnAndToast || runtime->ShouldAlwaysSetHiddenApiWarningFlag()) {
    runtime->SetPendingHiddenApiWarning(true);
  }

  return false;
}

// Returns true if access to `member` should be denied to a caller loaded with
// `caller_class_loader`.
// This function might print warnings into the log if the member is hidden.
template<typename T>
inline bool ShouldBlockAccessToMember(T* member,
                                      ObjPtr<mirror::ClassLoader> caller_class_loader,
                                      AccessMethod access_method)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  bool caller_in_boot = (caller_class_loader.IsNull());
  return ShouldBlockAccessToMember(member,
                                   /* thread */ nullptr,
                                   [caller_in_boot] (Thread*) { return caller_in_boot; },
                                   access_method);
}

}  // namespace hiddenapi
}  // namespace art

#endif  // ART_RUNTIME_HIDDEN_API_H_
