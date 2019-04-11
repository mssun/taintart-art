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
#include "base/locks.h"
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

// Hidden API access method
// Thist must be kept in sync with VMRuntime.HiddenApiUsageLogger.ACCESS_METHOD_*
enum class AccessMethod {
  kNone = 0,  // internal test that does not correspond to an actual access by app
  kReflection = 1,
  kJNI = 2,
  kLinking = 3,
};

// Represents the API domain of a caller/callee.
class AccessContext {
 public:
  // Initialize to either the fully-trusted or fully-untrusted domain.
  explicit AccessContext(bool is_trusted)
      : klass_(nullptr),
        dex_file_(nullptr),
        domain_(ComputeDomain(is_trusted)) {}

  // Initialize from class loader and dex file (via dex cache).
  AccessContext(ObjPtr<mirror::ClassLoader> class_loader, ObjPtr<mirror::DexCache> dex_cache)
      REQUIRES_SHARED(Locks::mutator_lock_)
      : klass_(nullptr),
        dex_file_(GetDexFileFromDexCache(dex_cache)),
        domain_(ComputeDomain(class_loader, dex_file_)) {}

  // Initialize from class loader and dex file (only used by tests).
  AccessContext(ObjPtr<mirror::ClassLoader> class_loader, const DexFile* dex_file)
      : klass_(nullptr),
        dex_file_(dex_file),
        domain_(ComputeDomain(class_loader, dex_file_)) {}

  // Initialize from Class.
  explicit AccessContext(ObjPtr<mirror::Class> klass)
      REQUIRES_SHARED(Locks::mutator_lock_)
      : klass_(klass),
        dex_file_(GetDexFileFromDexCache(klass->GetDexCache())),
        domain_(ComputeDomain(klass, dex_file_)) {}

  ObjPtr<mirror::Class> GetClass() const { return klass_; }
  const DexFile* GetDexFile() const { return dex_file_; }
  Domain GetDomain() const { return domain_; }
  bool IsApplicationDomain() const { return domain_ == Domain::kApplication; }

  // Returns true if this domain is always allowed to access the domain of `callee`.
  bool CanAlwaysAccess(const AccessContext& callee) const {
    return IsDomainMoreTrustedThan(domain_, callee.domain_);
  }

 private:
  static const DexFile* GetDexFileFromDexCache(ObjPtr<mirror::DexCache> dex_cache)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    return dex_cache.IsNull() ? nullptr : dex_cache->GetDexFile();
  }

  static Domain ComputeDomain(bool is_trusted) {
    return is_trusted ? Domain::kCorePlatform : Domain::kApplication;
  }

  static Domain ComputeDomain(ObjPtr<mirror::ClassLoader> class_loader, const DexFile* dex_file) {
    if (dex_file == nullptr) {
      return ComputeDomain(/* is_trusted= */ class_loader.IsNull());
    }

    return dex_file->GetHiddenapiDomain();
  }

  static Domain ComputeDomain(ObjPtr<mirror::Class> klass, const DexFile* dex_file)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    // Check other aspects of the context.
    Domain domain = ComputeDomain(klass->GetClassLoader(), dex_file);

    if (domain == Domain::kApplication &&
        klass->ShouldSkipHiddenApiChecks() &&
        Runtime::Current()->IsJavaDebuggable()) {
      // Class is known, it is marked trusted and we are in debuggable mode.
      domain = ComputeDomain(/* is_trusted= */ true);
    }

    return domain;
  }

  // Pointer to declaring class of the caller/callee (null if not provided).
  // This is not safe across GC but we're only using this class for passing
  // information about the caller to the access check logic and never retain
  // the AccessContext instance beyond that.
  const ObjPtr<mirror::Class> klass_;

  // DexFile of the caller/callee (null if not provided).
  const DexFile* const dex_file_;

  // Computed domain of the caller/callee.
  const Domain domain_;
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

  void WarnAboutAccess(AccessMethod access_method, ApiList list, bool access_denied);

  void LogAccessToEventLog(uint32_t sampled_value, AccessMethod access_method, bool access_denied);

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

// Handler of detected core platform API violations. Returns true if access to
// `member` should be denied.
template<typename T>
bool HandleCorePlatformApiViolation(T* member,
                                    const AccessContext& caller_context,
                                    AccessMethod access_method,
                                    EnforcementPolicy policy)
    REQUIRES_SHARED(Locks::mutator_lock_);

template<typename T>
bool ShouldDenyAccessToMemberImpl(T* member, ApiList api_list, AccessMethod access_method)
    REQUIRES_SHARED(Locks::mutator_lock_);

inline ArtField* GetInterfaceMemberIfProxy(ArtField* field) { return field; }

inline ArtMethod* GetInterfaceMemberIfProxy(ArtMethod* method)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  return method->GetInterfaceMethodIfProxy(kRuntimePointerSize);
}

// Returns access flags for the runtime representation of a class member (ArtField/ArtMember).
ALWAYS_INLINE inline uint32_t CreateRuntimeFlags_Impl(uint32_t dex_flags) {
  uint32_t runtime_flags = 0u;

  ApiList api_list(dex_flags);
  DCHECK(api_list.IsValid());

  if (api_list.Contains(ApiList::Whitelist())) {
    runtime_flags |= kAccPublicApi;
  } else {
    // Only add domain-specific flags for non-public API members.
    // This simplifies hardcoded values for intrinsics.
    if (api_list.Contains(ApiList::CorePlatformApi())) {
      runtime_flags |= kAccCorePlatformApi;
    }
  }

  DCHECK_EQ(runtime_flags & kAccHiddenapiBits, runtime_flags)
      << "Runtime flags not in reserved access flags bits";
  return runtime_flags;
}

}  // namespace detail

// Returns access flags for the runtime representation of a class member (ArtField/ArtMember).
ALWAYS_INLINE inline uint32_t CreateRuntimeFlags(const ClassAccessor::BaseItem& member) {
  return detail::CreateRuntimeFlags_Impl(member.GetHiddenapiFlags());
}

// Returns access flags for the runtime representation of a class member (ArtField/ArtMember).
template<typename T>
ALWAYS_INLINE inline uint32_t CreateRuntimeFlags(T* member) REQUIRES_SHARED(Locks::mutator_lock_) {
  return detail::CreateRuntimeFlags_Impl(detail::GetDexFlags(member));
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
      case Intrinsics::kCRC32UpdateByteBuffer:
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
      case Intrinsics::kUnsafeGetLong:
        return kAccCorePlatformApi;
      default:
        // Remaining intrinsics are public API. We DCHECK that in SetIntrinsic().
        return kAccPublicApi;
    }
  } else {
    return method->GetAccessFlags() & kAccHiddenapiBits;
  }
}

// Called by class linker when a new dex file has been registered. Assigns
// the AccessContext domain to the newly-registered dex file based on its
// location and class loader.
void InitializeDexFileDomain(const DexFile& dex_file, ObjPtr<mirror::ClassLoader> class_loader);

// Returns true if access to `member` should be denied in the given context.
// The decision is based on whether the caller is in a trusted context or not.
// Because determining the access context can be expensive, a lambda function
// "fn_get_access_context" is lazily invoked after other criteria have been
// considered.
// This function might print warnings into the log if the member is hidden.
template<typename T>
inline bool ShouldDenyAccessToMember(T* member,
                                     const std::function<AccessContext()>& fn_get_access_context,
                                     AccessMethod access_method)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  DCHECK(member != nullptr);

  // Get the runtime flags encoded in member's access flags.
  // Note: this works for proxy methods because they inherit access flags from their
  // respective interface methods.
  const uint32_t runtime_flags = GetRuntimeFlags(member);

  // Exit early if member is public API. This flag is also set for non-boot class
  // path fields/methods.
  if ((runtime_flags & kAccPublicApi) != 0) {
    return false;
  }

  // Determine which domain the caller and callee belong to.
  // This can be *very* expensive. This is why ShouldDenyAccessToMember
  // should not be called on every individual access.
  const AccessContext caller_context = fn_get_access_context();
  const AccessContext callee_context(member->GetDeclaringClass());

  // Non-boot classpath callers should have exited early.
  DCHECK(!callee_context.IsApplicationDomain());

  // Check if the caller is always allowed to access members in the callee context.
  if (caller_context.CanAlwaysAccess(callee_context)) {
    return false;
  }

  // Check if this is platform accessing core platform. We may warn if `member` is
  // not part of core platform API.
  switch (caller_context.GetDomain()) {
    case Domain::kApplication: {
      DCHECK(!callee_context.IsApplicationDomain());

      // Exit early if access checks are completely disabled.
      EnforcementPolicy policy = Runtime::Current()->GetHiddenApiEnforcementPolicy();
      if (policy == EnforcementPolicy::kDisabled) {
        return false;
      }

      // If this is a proxy method, look at the interface method instead.
      member = detail::GetInterfaceMemberIfProxy(member);

      // Decode hidden API access flags from the dex file.
      // This is an O(N) operation scaling with the number of fields/methods
      // in the class. Only do this on slow path and only do it once.
      ApiList api_list(detail::GetDexFlags(member));
      DCHECK(api_list.IsValid());

      // Member is hidden and caller is not exempted. Enter slow path.
      return detail::ShouldDenyAccessToMemberImpl(member, api_list, access_method);
    }

    case Domain::kPlatform: {
      DCHECK(callee_context.GetDomain() == Domain::kCorePlatform);

      // Member is part of core platform API. Accessing it is allowed.
      if ((runtime_flags & kAccCorePlatformApi) != 0) {
        return false;
      }

      // Allow access if access checks are disabled.
      EnforcementPolicy policy = Runtime::Current()->GetCorePlatformApiEnforcementPolicy();
      if (policy == EnforcementPolicy::kDisabled) {
        return false;
      }

      // If this is a proxy method, look at the interface method instead.
      member = detail::GetInterfaceMemberIfProxy(member);

      // Access checks are not disabled, report the violation.
      // This may also add kAccCorePlatformApi to the access flags of `member`
      // so as to not warn again on next access.
      return detail::HandleCorePlatformApiViolation(member,
                                                    caller_context,
                                                    access_method,
                                                    policy);
    }

    case Domain::kCorePlatform: {
      LOG(FATAL) << "CorePlatform domain should be allowed to access all domains";
      UNREACHABLE();
    }
  }
}

// Helper method for callers where access context can be determined beforehand.
// Wraps AccessContext in a lambda and passes it to the real ShouldDenyAccessToMember.
template<typename T>
inline bool ShouldDenyAccessToMember(T* member,
                                     const AccessContext& access_context,
                                     AccessMethod access_method)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  return ShouldDenyAccessToMember(member, [&]() { return access_context; }, access_method);
}

}  // namespace hiddenapi
}  // namespace art

#endif  // ART_RUNTIME_HIDDEN_API_H_
