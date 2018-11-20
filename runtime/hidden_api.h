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

#include "art_field.h"
#include "art_method.h"
#include "base/hiddenapi_flags.h"
#include "base/mutex.h"
#include "intrinsics_enum.h"
#include "mirror/class-inl.h"
#include "reflection.h"
#include "runtime.h"

namespace art {
namespace hiddenapi {

// Hidden API enforcement policy
// This must be kept in sync with ApplicationInfo.ApiEnforcementPolicy in
// frameworks/base/core/java/android/content/pm/ApplicationInfo.java
enum class EnforcementPolicy {
  kDisabled             = 0,
  kJustWarn             = 1,  // keep checks enabled, but allow everything (enables logging)
  kEnabled              = 2,  // ban dark grey & blacklist
  kMax = kEnabled,
};

inline EnforcementPolicy EnforcementPolicyFromInt(int api_policy_int) {
  DCHECK_GE(api_policy_int, 0);
  DCHECK_LE(api_policy_int, static_cast<int>(EnforcementPolicy::kMax));
  return static_cast<EnforcementPolicy>(api_policy_int);
}

enum class AccessMethod {
  kNone,  // internal test that does not correspond to an actual access by app
  kReflection,
  kJNI,
  kLinking,
};

struct AccessContext {
 public:
  explicit AccessContext(bool is_trusted) : is_trusted_(is_trusted) {}

  explicit AccessContext(ObjPtr<mirror::Class> klass) : is_trusted_(GetIsTrusted(klass)) {}

  AccessContext(ObjPtr<mirror::ClassLoader> class_loader, ObjPtr<mirror::DexCache> dex_cache)
      : is_trusted_(GetIsTrusted(class_loader, dex_cache)) {}

  bool IsTrusted() const { return is_trusted_; }

 private:
  static bool GetIsTrusted(ObjPtr<mirror::ClassLoader> class_loader,
                           ObjPtr<mirror::DexCache> dex_cache)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    // Trust if the caller is in is boot class loader.
    if (class_loader.IsNull()) {
      return true;
    }

    // Trust if caller is in a platform dex file.
    if (!dex_cache.IsNull()) {
      const DexFile* dex_file = dex_cache->GetDexFile();
      if (dex_file != nullptr && dex_file->IsPlatformDexFile()) {
        return true;
      }
    }

    return false;
  }

  static bool GetIsTrusted(ObjPtr<mirror::Class> klass) REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK(!klass.IsNull());

    if (klass->ShouldSkipHiddenApiChecks() && Runtime::Current()->IsJavaDebuggable()) {
      // Class is known, it is marked trusted and we are in debuggable mode.
      return true;
    }

    // Check other aspects of the context.
    return GetIsTrusted(klass->GetClassLoader(), klass->GetDexCache());
  }

  bool is_trusted_;
};

class ScopedHiddenApiEnforcementPolicySetting {
 public:
  explicit ScopedHiddenApiEnforcementPolicySetting(EnforcementPolicy new_policy)
      : initial_policy_(Runtime::Current()->GetHiddenApiEnforcementPolicy()) {
    Runtime::Current()->SetHiddenApiEnforcementPolicy(new_policy);
  }

  ~ScopedHiddenApiEnforcementPolicySetting() {
    Runtime::Current()->SetHiddenApiEnforcementPolicy(initial_policy_);
  }

 private:
  const EnforcementPolicy initial_policy_;
  DISALLOW_COPY_AND_ASSIGN(ScopedHiddenApiEnforcementPolicySetting);
};

// Implementation details. DO NOT ACCESS DIRECTLY.
namespace detail {

// Class to encapsulate the signature of a member (ArtField or ArtMethod). This
// is used as a helper when matching prefixes, and when logging the signature.
class MemberSignature {
 private:
  enum MemberType {
    kField,
    kMethod,
  };

  std::string class_name_;
  std::string member_name_;
  std::string type_signature_;
  std::string tmp_;
  MemberType type_;

  inline std::vector<const char*> GetSignatureParts() const;

 public:
  explicit MemberSignature(ArtField* field) REQUIRES_SHARED(Locks::mutator_lock_);
  explicit MemberSignature(ArtMethod* method) REQUIRES_SHARED(Locks::mutator_lock_);
  explicit MemberSignature(const ClassAccessor::Field& field);
  explicit MemberSignature(const ClassAccessor::Method& method);

  void Dump(std::ostream& os) const;

  bool Equals(const MemberSignature& other);
  bool MemberNameAndTypeMatch(const MemberSignature& other);

  // Performs prefix match on this member. Since the full member signature is
  // composed of several parts, we match each part in turn (rather than
  // building the entire thing in memory and performing a simple prefix match)
  bool DoesPrefixMatch(const std::string& prefix) const;

  bool IsExempted(const std::vector<std::string>& exemptions);

  void WarnAboutAccess(AccessMethod access_method, ApiList list);

  void LogAccessToEventLog(AccessMethod access_method, bool access_denied);

  // Calls back into managed code to notify VMRuntime.nonSdkApiUsageConsumer that
  // |member| was accessed. This is usually called when an API is on the black,
  // dark grey or light grey lists. Given that the callback can execute arbitrary
  // code, a call to this method can result in thread suspension.
  void NotifyHiddenApiListener(AccessMethod access_method);
};

// Locates hiddenapi flags for `field` in the corresponding dex file.
// NB: This is an O(N) operation, linear with the number of members in the class def.
template<typename T>
uint32_t GetDexFlags(T* member) REQUIRES_SHARED(Locks::mutator_lock_);

template<typename T>
bool ShouldDenyAccessToMemberImpl(T* member, ApiList api_list, AccessMethod access_method)
    REQUIRES_SHARED(Locks::mutator_lock_);

}  // namespace detail

// Returns access flags for the runtime representation of a class member (ArtField/ArtMember).
ALWAYS_INLINE inline uint32_t CreateRuntimeFlags(const ClassAccessor::BaseItem& member) {
  uint32_t runtime_flags = 0u;

  uint32_t dex_flags = member.GetHiddenapiFlags();
  DCHECK(AreValidDexFlags(dex_flags));

  ApiList api_list = ApiList::FromDexFlags(dex_flags);
  if (api_list == ApiList::Whitelist()) {
    runtime_flags |= kAccPublicApi;
  }

  DCHECK_EQ(runtime_flags & kAccHiddenapiBits, runtime_flags)
      << "Runtime flags not in reserved access flags bits";
  return runtime_flags;
}

// Extracts hiddenapi runtime flags from access flags of ArtField.
ALWAYS_INLINE inline uint32_t GetRuntimeFlags(ArtField* field)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  return field->GetAccessFlags() & kAccHiddenapiBits;
}

// Extracts hiddenapi runtime flags from access flags of ArtMethod.
// Uses hardcoded values for intrinsics.
ALWAYS_INLINE inline uint32_t GetRuntimeFlags(ArtMethod* method)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  if (UNLIKELY(method->IsIntrinsic())) {
    switch (static_cast<Intrinsics>(method->GetIntrinsic())) {
      case Intrinsics::kSystemArrayCopyChar:
      case Intrinsics::kStringGetCharsNoCheck:
      case Intrinsics::kReferenceGetReferent:
      case Intrinsics::kMemoryPeekByte:
      case Intrinsics::kMemoryPokeByte:
      case Intrinsics::kUnsafeCASInt:
      case Intrinsics::kUnsafeCASLong:
      case Intrinsics::kUnsafeCASObject:
      case Intrinsics::kUnsafeGet:
      case Intrinsics::kUnsafeGetAndAddInt:
      case Intrinsics::kUnsafeGetAndAddLong:
      case Intrinsics::kUnsafeGetAndSetInt:
      case Intrinsics::kUnsafeGetAndSetLong:
      case Intrinsics::kUnsafeGetAndSetObject:
      case Intrinsics::kUnsafeGetLong:
      case Intrinsics::kUnsafeGetLongVolatile:
      case Intrinsics::kUnsafeGetObject:
      case Intrinsics::kUnsafeGetObjectVolatile:
      case Intrinsics::kUnsafeGetVolatile:
      case Intrinsics::kUnsafePut:
      case Intrinsics::kUnsafePutLong:
      case Intrinsics::kUnsafePutLongOrdered:
      case Intrinsics::kUnsafePutLongVolatile:
      case Intrinsics::kUnsafePutObject:
      case Intrinsics::kUnsafePutObjectOrdered:
      case Intrinsics::kUnsafePutObjectVolatile:
      case Intrinsics::kUnsafePutOrdered:
      case Intrinsics::kUnsafePutVolatile:
      case Intrinsics::kUnsafeLoadFence:
      case Intrinsics::kUnsafeStoreFence:
      case Intrinsics::kUnsafeFullFence:
      case Intrinsics::kCRC32Update:
      case Intrinsics::kCRC32UpdateBytes:
      case Intrinsics::kStringNewStringFromBytes:
      case Intrinsics::kStringNewStringFromChars:
      case Intrinsics::kStringNewStringFromString:
      case Intrinsics::kMemoryPeekIntNative:
      case Intrinsics::kMemoryPeekLongNative:
      case Intrinsics::kMemoryPeekShortNative:
      case Intrinsics::kMemoryPokeIntNative:
      case Intrinsics::kMemoryPokeLongNative:
      case Intrinsics::kMemoryPokeShortNative:
      case Intrinsics::kVarHandleFullFence:
      case Intrinsics::kVarHandleAcquireFence:
      case Intrinsics::kVarHandleReleaseFence:
      case Intrinsics::kVarHandleLoadLoadFence:
      case Intrinsics::kVarHandleStoreStoreFence:
      case Intrinsics::kVarHandleCompareAndExchange:
      case Intrinsics::kVarHandleCompareAndExchangeAcquire:
      case Intrinsics::kVarHandleCompareAndExchangeRelease:
      case Intrinsics::kVarHandleCompareAndSet:
      case Intrinsics::kVarHandleGet:
      case Intrinsics::kVarHandleGetAcquire:
      case Intrinsics::kVarHandleGetAndAdd:
      case Intrinsics::kVarHandleGetAndAddAcquire:
      case Intrinsics::kVarHandleGetAndAddRelease:
      case Intrinsics::kVarHandleGetAndBitwiseAnd:
      case Intrinsics::kVarHandleGetAndBitwiseAndAcquire:
      case Intrinsics::kVarHandleGetAndBitwiseAndRelease:
      case Intrinsics::kVarHandleGetAndBitwiseOr:
      case Intrinsics::kVarHandleGetAndBitwiseOrAcquire:
      case Intrinsics::kVarHandleGetAndBitwiseOrRelease:
      case Intrinsics::kVarHandleGetAndBitwiseXor:
      case Intrinsics::kVarHandleGetAndBitwiseXorAcquire:
      case Intrinsics::kVarHandleGetAndBitwiseXorRelease:
      case Intrinsics::kVarHandleGetAndSet:
      case Intrinsics::kVarHandleGetAndSetAcquire:
      case Intrinsics::kVarHandleGetAndSetRelease:
      case Intrinsics::kVarHandleGetOpaque:
      case Intrinsics::kVarHandleGetVolatile:
      case Intrinsics::kVarHandleSet:
      case Intrinsics::kVarHandleSetOpaque:
      case Intrinsics::kVarHandleSetRelease:
      case Intrinsics::kVarHandleSetVolatile:
      case Intrinsics::kVarHandleWeakCompareAndSet:
      case Intrinsics::kVarHandleWeakCompareAndSetAcquire:
      case Intrinsics::kVarHandleWeakCompareAndSetPlain:
      case Intrinsics::kVarHandleWeakCompareAndSetRelease:
        return 0u;
      default:
        // Remaining intrinsics are public API. We DCHECK that in SetIntrinsic().
        return kAccPublicApi;
    }
  } else {
    return method->GetAccessFlags() & kAccHiddenapiBits;
  }
}

// Returns true if access to `member` should be denied in the given context.
// The decision is based on whether the caller is in a trusted context or not.
// Because determining the access context can be expensive, a lambda function
// "fn_get_access_context" is lazily invoked after other criteria have been
// considered.
// This function might print warnings into the log if the member is hidden.
template<typename T>
inline bool ShouldDenyAccessToMember(T* member,
                                     std::function<AccessContext()> fn_get_access_context,
                                     AccessMethod access_method)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  DCHECK(member != nullptr);

  // Exit early if member is public API. This flag is also set for non-boot class
  // path fields/methods.
  if ((GetRuntimeFlags(member) & kAccPublicApi) != 0) {
    return false;
  }

  // Check if caller is exempted from access checks.
  // This can be *very* expensive. Save it for last.
  if (fn_get_access_context().IsTrusted()) {
    return false;
  }

  // Decode hidden API access flags from the dex file.
  // This is an O(N) operation scaling with the number of fields/methods
  // in the class. Only do this on slow path and only do it once.
  ApiList api_list = ApiList::FromDexFlags(detail::GetDexFlags(member));

  // Member is hidden and caller is not exempted. Enter slow path.
  return detail::ShouldDenyAccessToMemberImpl(member, api_list, access_method);
}

// Helper method for callers where access context can be determined beforehand.
// Wraps AccessContext in a lambda and passes it to the real ShouldDenyAccessToMember.
template<typename T>
inline bool ShouldDenyAccessToMember(T* member,
                                     AccessContext access_context,
                                     AccessMethod access_method)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  return ShouldDenyAccessToMember(
      member,
      [&] () REQUIRES_SHARED(Locks::mutator_lock_) { return access_context; },
      access_method);
}

}  // namespace hiddenapi
}  // namespace art

#endif  // ART_RUNTIME_HIDDEN_API_H_
