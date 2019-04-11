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

#include "art_field-inl.h"
#include "art_method-inl.h"
#include "base/dumpable.h"
#include "base/file_utils.h"
#include "class_root.h"
#include "dex/class_accessor-inl.h"
#include "dex/dex_file_loader.h"
#include "mirror/class_ext.h"
#include "oat_file.h"
#include "scoped_thread_state_change.h"
#include "thread-inl.h"
#include "well_known_classes.h"

namespace art {
namespace hiddenapi {

// Set to true if we should always print a warning in logcat for all hidden API accesses, not just
// dark grey and black. This can be set to true for developer preview / beta builds, but should be
// false for public release builds.
// Note that when flipping this flag, you must also update the expectations of test 674-hiddenapi
// as it affects whether or not we warn for light grey APIs that have been added to the exemptions
// list.
static constexpr bool kLogAllAccesses = false;

static inline std::ostream& operator<<(std::ostream& os, AccessMethod value) {
  switch (value) {
    case AccessMethod::kNone:
      LOG(FATAL) << "Internal access to hidden API should not be logged";
      UNREACHABLE();
    case AccessMethod::kReflection:
      os << "reflection";
      break;
    case AccessMethod::kJNI:
      os << "JNI";
      break;
    case AccessMethod::kLinking:
      os << "linking";
      break;
  }
  return os;
}

static inline std::ostream& operator<<(std::ostream& os, const AccessContext& value)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  if (!value.GetClass().IsNull()) {
    std::string tmp;
    os << value.GetClass()->GetDescriptor(&tmp);
  } else if (value.GetDexFile() != nullptr) {
    os << value.GetDexFile()->GetLocation();
  } else {
    os << "<unknown_caller>";
  }
  return os;
}

static Domain DetermineDomainFromLocation(const std::string& dex_location,
                                          ObjPtr<mirror::ClassLoader> class_loader) {
  // If running with APEX, check `path` against known APEX locations.
  // These checks will be skipped on target buildbots where ANDROID_RUNTIME_ROOT
  // is set to "/system".
  if (RuntimeModuleRootDistinctFromAndroidRoot()) {
    if (LocationIsOnRuntimeModule(dex_location.c_str()) ||
        LocationIsOnConscryptModule(dex_location.c_str())) {
      return Domain::kCorePlatform;
    }

    if (LocationIsOnApex(dex_location.c_str())) {
      return Domain::kPlatform;
    }
  }

  if (LocationIsOnSystemFramework(dex_location.c_str())) {
    return Domain::kPlatform;
  }

  if (class_loader.IsNull()) {
    LOG(WARNING) << "DexFile " << dex_location
        << " is in boot class path but is not in a known location";
    return Domain::kPlatform;
  }

  return Domain::kApplication;
}

void InitializeDexFileDomain(const DexFile& dex_file, ObjPtr<mirror::ClassLoader> class_loader) {
  Domain dex_domain = DetermineDomainFromLocation(dex_file.GetLocation(), class_loader);

  // Assign the domain unless a more permissive domain has already been assigned.
  // This may happen when DexFile is initialized as trusted.
  if (IsDomainMoreTrustedThan(dex_domain, dex_file.GetHiddenapiDomain())) {
    dex_file.SetHiddenapiDomain(dex_domain);
  }
}

namespace detail {

// Do not change the values of items in this enum, as they are written to the
// event log for offline analysis. Any changes will interfere with that analysis.
enum AccessContextFlags {
  // Accessed member is a field if this bit is set, else a method
  kMemberIsField = 1 << 0,
  // Indicates if access was denied to the member, instead of just printing a warning.
  kAccessDenied  = 1 << 1,
};

MemberSignature::MemberSignature(ArtField* field) {
  class_name_ = field->GetDeclaringClass()->GetDescriptor(&tmp_);
  member_name_ = field->GetName();
  type_signature_ = field->GetTypeDescriptor();
  type_ = kField;
}

MemberSignature::MemberSignature(ArtMethod* method) {
  DCHECK(method == method->GetInterfaceMethodIfProxy(kRuntimePointerSize))
      << "Caller should have replaced proxy method with interface method";
  class_name_ = method->GetDeclaringClass()->GetDescriptor(&tmp_);
  member_name_ = method->GetName();
  type_signature_ = method->GetSignature().ToString();
  type_ = kMethod;
}

MemberSignature::MemberSignature(const ClassAccessor::Field& field) {
  const DexFile& dex_file = field.GetDexFile();
  const dex::FieldId& field_id = dex_file.GetFieldId(field.GetIndex());
  class_name_ = dex_file.GetFieldDeclaringClassDescriptor(field_id);
  member_name_ = dex_file.GetFieldName(field_id);
  type_signature_ = dex_file.GetFieldTypeDescriptor(field_id);
  type_ = kField;
}

MemberSignature::MemberSignature(const ClassAccessor::Method& method) {
  const DexFile& dex_file = method.GetDexFile();
  const dex::MethodId& method_id = dex_file.GetMethodId(method.GetIndex());
  class_name_ = dex_file.GetMethodDeclaringClassDescriptor(method_id);
  member_name_ = dex_file.GetMethodName(method_id);
  type_signature_ = dex_file.GetMethodSignature(method_id).ToString();
  type_ = kMethod;
}

inline std::vector<const char*> MemberSignature::GetSignatureParts() const {
  if (type_ == kField) {
    return { class_name_.c_str(), "->", member_name_.c_str(), ":", type_signature_.c_str() };
  } else {
    DCHECK_EQ(type_, kMethod);
    return { class_name_.c_str(), "->", member_name_.c_str(), type_signature_.c_str() };
  }
}

bool MemberSignature::DoesPrefixMatch(const std::string& prefix) const {
  size_t pos = 0;
  for (const char* part : GetSignatureParts()) {
    size_t count = std::min(prefix.length() - pos, strlen(part));
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
  for (const char* part : GetSignatureParts()) {
    os << part;
  }
}

void MemberSignature::WarnAboutAccess(AccessMethod access_method,
                                      hiddenapi::ApiList list,
                                      bool access_denied) {
  LOG(WARNING) << "Accessing hidden " << (type_ == kField ? "field " : "method ")
               << Dumpable<MemberSignature>(*this) << " (" << list << ", " << access_method
               << (access_denied ? ", denied)" : ", allowed)");
}

bool MemberSignature::Equals(const MemberSignature& other) {
  return type_ == other.type_ &&
         class_name_ == other.class_name_ &&
         member_name_ == other.member_name_ &&
         type_signature_ == other.type_signature_;
}

bool MemberSignature::MemberNameAndTypeMatch(const MemberSignature& other) {
  return member_name_ == other.member_name_ && type_signature_ == other.type_signature_;
}

void MemberSignature::LogAccessToEventLog(uint32_t sampled_value,
                                          AccessMethod access_method,
                                          bool access_denied) {
#ifdef ART_TARGET_ANDROID
  if (access_method == AccessMethod::kLinking || access_method == AccessMethod::kNone) {
    // Linking warnings come from static analysis/compilation of the bytecode
    // and can contain false positives (i.e. code that is never run). We choose
    // not to log these in the event log.
    // None does not correspond to actual access, so should also be ignored.
    return;
  }
  Runtime* runtime = Runtime::Current();
  if (runtime->IsAotCompiler()) {
    return;
  }
  JNIEnvExt* env = Thread::Current()->GetJniEnv();
  const std::string& package_name = Runtime::Current()->GetProcessPackageName();
  ScopedLocalRef<jstring> package_str(env, env->NewStringUTF(package_name.c_str()));
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    LOG(ERROR) << "Unable to allocate string for package name which called hidden api";
  }
  std::ostringstream signature_str;
  Dump(signature_str);
  ScopedLocalRef<jstring> signature_jstr(env,
      env->NewStringUTF(signature_str.str().c_str()));
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    LOG(ERROR) << "Unable to allocate string for hidden api method signature";
  }
  env->CallStaticVoidMethod(WellKnownClasses::dalvik_system_VMRuntime,
      WellKnownClasses::dalvik_system_VMRuntime_hiddenApiUsed,
      sampled_value,
      package_str.get(),
      signature_jstr.get(),
      static_cast<jint>(access_method),
      access_denied);
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    LOG(ERROR) << "Unable to report hidden api usage";
  }
#else
  UNUSED(sampled_value);
  UNUSED(access_method);
  UNUSED(access_denied);
#endif
}

void MemberSignature::NotifyHiddenApiListener(AccessMethod access_method) {
  if (access_method != AccessMethod::kReflection && access_method != AccessMethod::kJNI) {
    // We can only up-call into Java during reflection and JNI down-calls.
    return;
  }

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
      std::ostringstream member_signature_str;
      Dump(member_signature_str);

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

static ALWAYS_INLINE bool CanUpdateRuntimeFlags(ArtField*) {
  return true;
}

static ALWAYS_INLINE bool CanUpdateRuntimeFlags(ArtMethod* method) {
  return !method->IsIntrinsic();
}

template<typename T>
static ALWAYS_INLINE void MaybeUpdateAccessFlags(Runtime* runtime, T* member, uint32_t flag)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  // Update the access flags unless:
  // (a) `member` is an intrinsic
  // (b) this is AOT compiler, as we do not want the updated access flags in the boot/app image
  // (c) deduping warnings has been explicitly switched off.
  if (CanUpdateRuntimeFlags(member) &&
      !runtime->IsAotCompiler() &&
      runtime->ShouldDedupeHiddenApiWarnings()) {
    member->SetAccessFlags(member->GetAccessFlags() | flag);
  }
}

static ALWAYS_INLINE uint32_t GetMemberDexIndex(ArtField* field) {
  return field->GetDexFieldIndex();
}

static ALWAYS_INLINE uint32_t GetMemberDexIndex(ArtMethod* method)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  // Use the non-obsolete method to avoid DexFile mismatch between
  // the method index and the declaring class.
  return method->GetNonObsoleteMethod()->GetDexMethodIndex();
}

static void VisitMembers(const DexFile& dex_file,
                         const dex::ClassDef& class_def,
                         const std::function<void(const ClassAccessor::Field&)>& fn_visit) {
  ClassAccessor accessor(dex_file, class_def, /* parse_hiddenapi_class_data= */ true);
  accessor.VisitFields(fn_visit, fn_visit);
}

static void VisitMembers(const DexFile& dex_file,
                         const dex::ClassDef& class_def,
                         const std::function<void(const ClassAccessor::Method&)>& fn_visit) {
  ClassAccessor accessor(dex_file, class_def, /* parse_hiddenapi_class_data= */ true);
  accessor.VisitMethods(fn_visit, fn_visit);
}

template<typename T>
uint32_t GetDexFlags(T* member) REQUIRES_SHARED(Locks::mutator_lock_) {
  static_assert(std::is_same<T, ArtField>::value || std::is_same<T, ArtMethod>::value);
  constexpr bool kMemberIsField = std::is_same<T, ArtField>::value;
  using AccessorType = typename std::conditional<std::is_same<T, ArtField>::value,
      ClassAccessor::Field, ClassAccessor::Method>::type;

  ObjPtr<mirror::Class> declaring_class = member->GetDeclaringClass();
  DCHECK(!declaring_class.IsNull()) << "Attempting to access a runtime method";

  ApiList flags;
  DCHECK(!flags.IsValid());

  // Check if the declaring class has ClassExt allocated. If it does, check if
  // the pre-JVMTI redefine dex file has been set to determine if the declaring
  // class has been JVMTI-redefined.
  ObjPtr<mirror::ClassExt> ext(declaring_class->GetExtData());
  const DexFile* original_dex = ext.IsNull() ? nullptr : ext->GetPreRedefineDexFile();
  if (LIKELY(original_dex == nullptr)) {
    // Class is not redefined. Find the class def, iterate over its members and
    // find the entry corresponding to this `member`.
    const dex::ClassDef* class_def = declaring_class->GetClassDef();
    if (class_def == nullptr) {
      // ClassDef is not set for proxy classes. Only their fields can ever be inspected.
      DCHECK(declaring_class->IsProxyClass())
          << "Only proxy classes are expected not to have a class def";
      DCHECK(kMemberIsField)
          << "Interface methods should be inspected instead of proxy class methods";
      flags = ApiList::Greylist();
    } else {
      uint32_t member_index = GetMemberDexIndex(member);
      auto fn_visit = [&](const AccessorType& dex_member) {
        if (dex_member.GetIndex() == member_index) {
          flags = ApiList(dex_member.GetHiddenapiFlags());
        }
      };
      VisitMembers(declaring_class->GetDexFile(), *class_def, fn_visit);
    }
  } else {
    // Class was redefined using JVMTI. We have a pointer to the original dex file
    // and the class def index of this class in that dex file, but the field/method
    // indices are lost. Iterate over all members of the class def and find the one
    // corresponding to this `member` by name and type string comparison.
    // This is obviously very slow, but it is only used when non-exempt code tries
    // to access a hidden member of a JVMTI-redefined class.
    uint16_t class_def_idx = ext->GetPreRedefineClassDefIndex();
    DCHECK_NE(class_def_idx, DexFile::kDexNoIndex16);
    const dex::ClassDef& original_class_def = original_dex->GetClassDef(class_def_idx);
    MemberSignature member_signature(member);
    auto fn_visit = [&](const AccessorType& dex_member) {
      MemberSignature cur_signature(dex_member);
      if (member_signature.MemberNameAndTypeMatch(cur_signature)) {
        DCHECK(member_signature.Equals(cur_signature));
        flags = ApiList(dex_member.GetHiddenapiFlags());
      }
    };
    VisitMembers(*original_dex, original_class_def, fn_visit);
  }

  CHECK(flags.IsValid()) << "Could not find hiddenapi flags for "
      << Dumpable<MemberSignature>(MemberSignature(member));
  return flags.GetDexFlags();
}

template<typename T>
bool HandleCorePlatformApiViolation(T* member,
                                    const AccessContext& caller_context,
                                    AccessMethod access_method,
                                    EnforcementPolicy policy) {
  DCHECK(policy != EnforcementPolicy::kDisabled)
      << "Should never enter this function when access checks are completely disabled";

  if (access_method != AccessMethod::kNone) {
    LOG(WARNING) << "Core platform API violation: "
        << Dumpable<MemberSignature>(MemberSignature(member))
        << " from " << caller_context << " using " << access_method;

    // If policy is set to just warn, add kAccCorePlatformApi to access flags of
    // `member` to avoid reporting the violation again next time.
    if (policy == EnforcementPolicy::kJustWarn) {
      MaybeUpdateAccessFlags(Runtime::Current(), member, kAccCorePlatformApi);
    }
  }

  // Deny access if enforcement is enabled.
  return policy == EnforcementPolicy::kEnabled;
}

template<typename T>
bool ShouldDenyAccessToMemberImpl(T* member, ApiList api_list, AccessMethod access_method) {
  DCHECK(member != nullptr);
  Runtime* runtime = Runtime::Current();

  EnforcementPolicy policy = runtime->GetHiddenApiEnforcementPolicy();
  DCHECK(policy != EnforcementPolicy::kDisabled)
      << "Should never enter this function when access checks are completely disabled";

  const bool deny_access =
      (policy == EnforcementPolicy::kEnabled) &&
      IsSdkVersionSetAndMoreThan(runtime->GetTargetSdkVersion(),
                                 api_list.GetMaxAllowedSdkVersion());

  MemberSignature member_signature(member);

  // Check for an exemption first. Exempted APIs are treated as white list.
  if (member_signature.IsExempted(runtime->GetHiddenApiExemptions())) {
    // Avoid re-examining the exemption list next time.
    // Note this results in no warning for the member, which seems like what one would expect.
    // Exemptions effectively adds new members to the whitelist.
    MaybeUpdateAccessFlags(runtime, member, kAccPublicApi);
    return false;
  }

  if (access_method != AccessMethod::kNone) {
    // Print a log message with information about this class member access.
    // We do this if we're about to deny access, or the app is debuggable.
    if (kLogAllAccesses || deny_access || runtime->IsJavaDebuggable()) {
      member_signature.WarnAboutAccess(access_method, api_list, deny_access);
    }

    // If there is a StrictMode listener, notify it about this violation.
    member_signature.NotifyHiddenApiListener(access_method);

    // If event log sampling is enabled, report this violation.
    if (kIsTargetBuild && !kIsTargetLinux) {
      uint32_t eventLogSampleRate = runtime->GetHiddenApiEventLogSampleRate();
      // Assert that RAND_MAX is big enough, to ensure sampling below works as expected.
      static_assert(RAND_MAX >= 0xffff, "RAND_MAX too small");
      if (eventLogSampleRate != 0) {
        const uint32_t sampled_value = static_cast<uint32_t>(std::rand()) & 0xffff;
        if (sampled_value < eventLogSampleRate) {
          member_signature.LogAccessToEventLog(sampled_value, access_method, deny_access);
        }
      }
    }

    // If this access was not denied, move the member into whitelist and skip
    // the warning the next time the member is accessed.
    if (!deny_access) {
      MaybeUpdateAccessFlags(runtime, member, kAccPublicApi);
    }
  }

  return deny_access;
}

// Need to instantiate these.
template uint32_t GetDexFlags<ArtField>(ArtField* member);
template uint32_t GetDexFlags<ArtMethod>(ArtMethod* member);
template bool HandleCorePlatformApiViolation(ArtField* member,
                                             const AccessContext& caller_context,
                                             AccessMethod access_method,
                                             EnforcementPolicy policy);
template bool HandleCorePlatformApiViolation(ArtMethod* member,
                                             const AccessContext& caller_context,
                                             AccessMethod access_method,
                                             EnforcementPolicy policy);
template bool ShouldDenyAccessToMemberImpl<ArtField>(ArtField* member,
                                                     ApiList api_list,
                                                     AccessMethod access_method);
template bool ShouldDenyAccessToMemberImpl<ArtMethod>(ArtMethod* member,
                                                      ApiList api_list,
                                                      AccessMethod access_method);
}  // namespace detail

}  // namespace hiddenapi
}  // namespace art
