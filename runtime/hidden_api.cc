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

#include "hidden_api.h"

#include <nativehelper/scoped_local_ref.h>

#include "base/dumpable.h"
#include "thread-current-inl.h"
#include "well_known_classes.h"

namespace art {
namespace hiddenapi {

static inline std::ostream& operator<<(std::ostream& os, AccessMethod value) {
  switch (value) {
    case kNone:
      LOG(FATAL) << "Internal access to hidden API should not be logged";
      UNREACHABLE();
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

// GetMemberAction-related static_asserts.
static_assert(
    EnumsEqual(EnforcementPolicy::kDarkGreyAndBlackList, HiddenApiAccessFlags::kDarkGreylist) &&
    EnumsEqual(EnforcementPolicy::kBlacklistOnly, HiddenApiAccessFlags::kBlacklist),
    "Mismatch between EnforcementPolicy and ApiList enums");
static_assert(
    EnforcementPolicy::kJustWarn < EnforcementPolicy::kDarkGreyAndBlackList &&
    EnforcementPolicy::kDarkGreyAndBlackList < EnforcementPolicy::kBlacklistOnly,
    "EnforcementPolicy values ordering not correct");

namespace detail {

MemberSignature::MemberSignature(ArtField* field) {
  member_type_ = "field";
  signature_parts_ = {
    field->GetDeclaringClass()->GetDescriptor(&tmp_),
    "->",
    field->GetName(),
    ":",
    field->GetTypeDescriptor()
  };
}

MemberSignature::MemberSignature(ArtMethod* method) {
  member_type_ = "method";
  signature_parts_ = {
    method->GetDeclaringClass()->GetDescriptor(&tmp_),
    "->",
    method->GetName(),
    method->GetSignature().ToString()
  };
}

bool MemberSignature::DoesPrefixMatch(const std::string& prefix) const {
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

bool MemberSignature::IsExempted(const std::vector<std::string>& exemptions) {
  for (const std::string& exemption : exemptions) {
    if (DoesPrefixMatch(exemption)) {
      return true;
    }
  }
  return false;
}

void MemberSignature::Dump(std::ostream& os) const {
  for (std::string part : signature_parts_) {
    os << part;
  }
}

void MemberSignature::WarnAboutAccess(AccessMethod access_method,
                                      HiddenApiAccessFlags::ApiList list) {
  LOG(WARNING) << "Accessing hidden " << member_type_ << " " << Dumpable<MemberSignature>(*this)
               << " (" << list << ", " << access_method << ")";
}

template<typename T>
Action GetMemberActionImpl(T* member, Action action, AccessMethod access_method) {
  DCHECK_NE(action, kAllow);

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

  if (access_method != kNone) {
    // Print a log message with information about this class member access.
    // We do this regardless of whether we block the access or not.
    member_signature.WarnAboutAccess(access_method,
        HiddenApiAccessFlags::DecodeFromRuntime(member->GetAccessFlags()));
  }

  if (action == kDeny) {
    // Block access
    return action;
  }

  // Allow access to this member but print a warning.
  DCHECK(action == kAllowButWarn || action == kAllowButWarnAndToast);

  if (access_method != kNone) {
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
  }

  return action;
}

// Need to instantiate this.
template Action GetMemberActionImpl<ArtField>(ArtField* member,
                                              Action action,
                                              AccessMethod access_method);
template Action GetMemberActionImpl<ArtMethod>(ArtMethod* member,
                                               Action action,
                                               AccessMethod access_method);
}  // namespace detail

template<typename T>
void NotifyHiddenApiListener(T* member) {
  Runtime* runtime = Runtime::Current();
  if (!runtime->IsAotCompiler()) {
    ScopedObjectAccessUnchecked soa(Thread::Current());

    ScopedLocalRef<jobject> consumer_object(soa.Env(),
        soa.Env()->GetStaticObjectField(
            WellKnownClasses::dalvik_system_VMRuntime,
            WellKnownClasses::dalvik_system_VMRuntime_nonSdkApiUsageConsumer));
    // If the consumer is non-null, we call back to it to let it know that we
    // have encountered an API that's in one of our lists.
    if (consumer_object != nullptr) {
      detail::MemberSignature member_signature(member);
      std::ostringstream member_signature_str;
      member_signature.Dump(member_signature_str);

      ScopedLocalRef<jobject> signature_str(
          soa.Env(),
          soa.Env()->NewStringUTF(member_signature_str.str().c_str()));

      // Call through to Consumer.accept(String memberSignature);
      soa.Env()->CallVoidMethod(consumer_object.get(),
                                WellKnownClasses::java_util_function_Consumer_accept,
                                signature_str.get());
    }
  }
}

template void NotifyHiddenApiListener<ArtMethod>(ArtMethod* member);
template void NotifyHiddenApiListener<ArtField>(ArtField* member);

}  // namespace hiddenapi
}  // namespace art
