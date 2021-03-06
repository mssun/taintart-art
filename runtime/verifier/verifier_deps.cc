/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include "verifier_deps.h"

#include <cstring>
#include <sstream>

#include "art_field-inl.h"
#include "art_method-inl.h"
#include "base/indenter.h"
#include "base/leb128.h"
#include "base/mutex-inl.h"
#include "compiler_callbacks.h"
#include "dex/class_accessor-inl.h"
#include "dex/dex_file-inl.h"
#include "mirror/class-inl.h"
#include "mirror/class_loader.h"
#include "oat_file.h"
#include "obj_ptr-inl.h"
#include "runtime.h"

namespace art {
namespace verifier {

VerifierDeps::VerifierDeps(const std::vector<const DexFile*>& dex_files, bool output_only)
    : output_only_(output_only) {
  for (const DexFile* dex_file : dex_files) {
    DCHECK(GetDexFileDeps(*dex_file) == nullptr);
    std::unique_ptr<DexFileDeps> deps(new DexFileDeps(dex_file->NumClassDefs()));
    dex_deps_.emplace(dex_file, std::move(deps));
  }
}

VerifierDeps::VerifierDeps(const std::vector<const DexFile*>& dex_files)
    : VerifierDeps(dex_files, /*output_only=*/ true) {}

// Perform logical OR on two bit vectors and assign back to LHS, i.e. `to_update |= other`.
// Size of the two vectors must be equal.
// Size of `other` must be equal to size of `to_update`.
static inline void BitVectorOr(std::vector<bool>& to_update, const std::vector<bool>& other) {
  DCHECK_EQ(to_update.size(), other.size());
  std::transform(other.begin(),
                 other.end(),
                 to_update.begin(),
                 to_update.begin(),
                 std::logical_or<bool>());
}

void VerifierDeps::MergeWith(std::unique_ptr<VerifierDeps> other,
                             const std::vector<const DexFile*>& dex_files) {
  DCHECK(other != nullptr);
  DCHECK_EQ(dex_deps_.size(), other->dex_deps_.size());
  for (const DexFile* dex_file : dex_files) {
    DexFileDeps* my_deps = GetDexFileDeps(*dex_file);
    DexFileDeps& other_deps = *other->GetDexFileDeps(*dex_file);
    // We currently collect extra strings only on the main `VerifierDeps`,
    // which should be the one passed as `this` in this method.
    DCHECK(other_deps.strings_.empty());
    my_deps->assignable_types_.merge(other_deps.assignable_types_);
    my_deps->unassignable_types_.merge(other_deps.unassignable_types_);
    my_deps->classes_.merge(other_deps.classes_);
    my_deps->fields_.merge(other_deps.fields_);
    my_deps->methods_.merge(other_deps.methods_);
    BitVectorOr(my_deps->verified_classes_, other_deps.verified_classes_);
    BitVectorOr(my_deps->redefined_classes_, other_deps.redefined_classes_);
  }
}

VerifierDeps::DexFileDeps* VerifierDeps::GetDexFileDeps(const DexFile& dex_file) {
  auto it = dex_deps_.find(&dex_file);
  return (it == dex_deps_.end()) ? nullptr : it->second.get();
}

const VerifierDeps::DexFileDeps* VerifierDeps::GetDexFileDeps(const DexFile& dex_file) const {
  auto it = dex_deps_.find(&dex_file);
  return (it == dex_deps_.end()) ? nullptr : it->second.get();
}

// Access flags that impact vdex verification.
static constexpr uint32_t kAccVdexAccessFlags =
    kAccPublic | kAccPrivate | kAccProtected | kAccStatic | kAccInterface;

template <typename Ptr>
uint16_t VerifierDeps::GetAccessFlags(Ptr element) {
  static_assert(kAccJavaFlagsMask == 0xFFFF, "Unexpected value of a constant");
  if (element == nullptr) {
    return VerifierDeps::kUnresolvedMarker;
  } else {
    uint16_t access_flags = Low16Bits(element->GetAccessFlags()) & kAccVdexAccessFlags;
    CHECK_NE(access_flags, VerifierDeps::kUnresolvedMarker);
    return access_flags;
  }
}

dex::StringIndex VerifierDeps::GetClassDescriptorStringId(const DexFile& dex_file,
                                                          ObjPtr<mirror::Class> klass) {
  DCHECK(klass != nullptr);
  ObjPtr<mirror::DexCache> dex_cache = klass->GetDexCache();
  // Array and proxy classes do not have a dex cache.
  if (!klass->IsArrayClass() && !klass->IsProxyClass()) {
    DCHECK(dex_cache != nullptr) << klass->PrettyClass();
    if (dex_cache->GetDexFile() == &dex_file) {
      // FindStringId is slow, try to go through the class def if we have one.
      const dex::ClassDef* class_def = klass->GetClassDef();
      DCHECK(class_def != nullptr) << klass->PrettyClass();
      const dex::TypeId& type_id = dex_file.GetTypeId(class_def->class_idx_);
      if (kIsDebugBuild) {
        std::string temp;
        CHECK_EQ(GetIdFromString(dex_file, klass->GetDescriptor(&temp)), type_id.descriptor_idx_);
      }
      return type_id.descriptor_idx_;
    }
  }
  std::string temp;
  return GetIdFromString(dex_file, klass->GetDescriptor(&temp));
}

// Try to find the string descriptor of the class. type_idx is a best guess of a matching string id.
static dex::StringIndex TryGetClassDescriptorStringId(const DexFile& dex_file,
                                                      dex::TypeIndex type_idx,
                                                      ObjPtr<mirror::Class> klass)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  if (!klass->IsArrayClass()) {
    const dex::TypeId& type_id = dex_file.GetTypeId(type_idx);
    const DexFile& klass_dex = klass->GetDexFile();
    const dex::TypeId& klass_type_id = klass_dex.GetTypeId(klass->GetClassDef()->class_idx_);
    if (strcmp(dex_file.GetTypeDescriptor(type_id),
               klass_dex.GetTypeDescriptor(klass_type_id)) == 0) {
      return type_id.descriptor_idx_;
    }
  }
  return dex::StringIndex::Invalid();
}

dex::StringIndex VerifierDeps::GetMethodDeclaringClassStringId(const DexFile& dex_file,
                                                               uint32_t dex_method_index,
                                                               ArtMethod* method) {
  static_assert(kAccJavaFlagsMask == 0xFFFF, "Unexpected value of a constant");
  if (method == nullptr) {
    return dex::StringIndex(VerifierDeps::kUnresolvedMarker);
  }
  const dex::StringIndex string_id = TryGetClassDescriptorStringId(
      dex_file,
      dex_file.GetMethodId(dex_method_index).class_idx_,
      method->GetDeclaringClass());
  if (string_id.IsValid()) {
    // Got lucky using the original dex file, return based on the input dex file.
    DCHECK_EQ(GetClassDescriptorStringId(dex_file, method->GetDeclaringClass()), string_id);
    return string_id;
  }
  return GetClassDescriptorStringId(dex_file, method->GetDeclaringClass());
}

dex::StringIndex VerifierDeps::GetFieldDeclaringClassStringId(const DexFile& dex_file,
                                                              uint32_t dex_field_idx,
                                                              ArtField* field) {
  static_assert(kAccJavaFlagsMask == 0xFFFF, "Unexpected value of a constant");
  if (field == nullptr) {
    return dex::StringIndex(VerifierDeps::kUnresolvedMarker);
  }
  const dex::StringIndex string_id = TryGetClassDescriptorStringId(
      dex_file,
      dex_file.GetFieldId(dex_field_idx).class_idx_,
      field->GetDeclaringClass());
  if (string_id.IsValid()) {
    // Got lucky using the original dex file, return based on the input dex file.
    DCHECK_EQ(GetClassDescriptorStringId(dex_file, field->GetDeclaringClass()), string_id);
    return string_id;
  }
  return GetClassDescriptorStringId(dex_file, field->GetDeclaringClass());
}

static inline VerifierDeps* GetMainVerifierDeps() {
  // The main VerifierDeps is the one set in the compiler callbacks, which at the
  // end of verification will have all the per-thread VerifierDeps merged into it.
  CompilerCallbacks* callbacks = Runtime::Current()->GetCompilerCallbacks();
  if (callbacks == nullptr) {
    return nullptr;
  }
  return callbacks->GetVerifierDeps();
}

static inline VerifierDeps* GetThreadLocalVerifierDeps() {
  // During AOT, each thread has its own VerifierDeps, to avoid lock contention. At the end
  // of full verification, these VerifierDeps will be merged into the main one.
  if (!Runtime::Current()->IsAotCompiler()) {
    return nullptr;
  }
  return Thread::Current()->GetVerifierDeps();
}

static bool FindExistingStringId(const std::vector<std::string>& strings,
                                 const std::string& str,
                                 uint32_t* found_id) {
  uint32_t num_extra_ids = strings.size();
  for (size_t i = 0; i < num_extra_ids; ++i) {
    if (strings[i] == str) {
      *found_id = i;
      return true;
    }
  }
  return false;
}

dex::StringIndex VerifierDeps::GetIdFromString(const DexFile& dex_file, const std::string& str) {
  const dex::StringId* string_id = dex_file.FindStringId(str.c_str());
  if (string_id != nullptr) {
    // String is in the DEX file. Return its ID.
    return dex_file.GetIndexForStringId(*string_id);
  }

  // String is not in the DEX file. Assign a new ID to it which is higher than
  // the number of strings in the DEX file.

  // We use the main `VerifierDeps` for adding new strings to simplify
  // synchronization/merging of these entries between threads.
  VerifierDeps* singleton = GetMainVerifierDeps();
  DexFileDeps* deps = singleton->GetDexFileDeps(dex_file);
  DCHECK(deps != nullptr);

  uint32_t num_ids_in_dex = dex_file.NumStringIds();
  uint32_t found_id;

  {
    ReaderMutexLock mu(Thread::Current(), *Locks::verifier_deps_lock_);
    if (FindExistingStringId(deps->strings_, str, &found_id)) {
      return dex::StringIndex(num_ids_in_dex + found_id);
    }
  }
  {
    WriterMutexLock mu(Thread::Current(), *Locks::verifier_deps_lock_);
    if (FindExistingStringId(deps->strings_, str, &found_id)) {
      return dex::StringIndex(num_ids_in_dex + found_id);
    }
    deps->strings_.push_back(str);
    dex::StringIndex new_id(num_ids_in_dex + deps->strings_.size() - 1);
    CHECK_GE(new_id.index_, num_ids_in_dex);  // check for overflows
    DCHECK_EQ(str, singleton->GetStringFromId(dex_file, new_id));
    return new_id;
  }
}

std::string VerifierDeps::GetStringFromId(const DexFile& dex_file, dex::StringIndex string_id)
    const {
  uint32_t num_ids_in_dex = dex_file.NumStringIds();
  if (string_id.index_ < num_ids_in_dex) {
    return std::string(dex_file.StringDataByIdx(string_id));
  } else {
    const DexFileDeps* deps = GetDexFileDeps(dex_file);
    DCHECK(deps != nullptr);
    string_id.index_ -= num_ids_in_dex;
    CHECK_LT(string_id.index_, deps->strings_.size());
    return deps->strings_[string_id.index_];
  }
}

bool VerifierDeps::IsInClassPath(ObjPtr<mirror::Class> klass) const {
  DCHECK(klass != nullptr);

  // For array types, we return whether the non-array component type
  // is in the classpath.
  while (klass->IsArrayClass()) {
    klass = klass->GetComponentType();
  }

  if (klass->IsPrimitive()) {
    return true;
  }

  ObjPtr<mirror::DexCache> dex_cache = klass->GetDexCache();
  DCHECK(dex_cache != nullptr);
  const DexFile* dex_file = dex_cache->GetDexFile();
  DCHECK(dex_file != nullptr);

  // Test if the `dex_deps_` contains an entry for `dex_file`. If not, the dex
  // file was not registered as being compiled and we assume `klass` is in the
  // classpath.
  return (GetDexFileDeps(*dex_file) == nullptr);
}

void VerifierDeps::AddClassResolution(const DexFile& dex_file,
                                      dex::TypeIndex type_idx,
                                      ObjPtr<mirror::Class> klass) {
  DexFileDeps* dex_deps = GetDexFileDeps(dex_file);
  if (dex_deps == nullptr) {
    // This invocation is from verification of a dex file which is not being compiled.
    return;
  }

  if (klass != nullptr && !IsInClassPath(klass)) {
    // Class resolved into one of the DEX files which are being compiled.
    // This is not a classpath dependency.
    return;
  }

  dex_deps->classes_.emplace(ClassResolution(type_idx, GetAccessFlags(klass)));
}

void VerifierDeps::AddFieldResolution(const DexFile& dex_file,
                                      uint32_t field_idx,
                                      ArtField* field) {
  DexFileDeps* dex_deps = GetDexFileDeps(dex_file);
  if (dex_deps == nullptr) {
    // This invocation is from verification of a dex file which is not being compiled.
    return;
  }

  if (field != nullptr && !IsInClassPath(field->GetDeclaringClass())) {
    // Field resolved into one of the DEX files which are being compiled.
    // This is not a classpath dependency.
    return;
  }

  dex_deps->fields_.emplace(FieldResolution(field_idx,
                                            GetAccessFlags(field),
                                            GetFieldDeclaringClassStringId(dex_file,
                                                                           field_idx,
                                                                           field)));
}

void VerifierDeps::AddMethodResolution(const DexFile& dex_file,
                                       uint32_t method_idx,
                                       ArtMethod* method) {
  DexFileDeps* dex_deps = GetDexFileDeps(dex_file);
  if (dex_deps == nullptr) {
    // This invocation is from verification of a dex file which is not being compiled.
    return;
  }

  if (method != nullptr && !IsInClassPath(method->GetDeclaringClass())) {
    // Method resolved into one of the DEX files which are being compiled.
    // This is not a classpath dependency.
    return;
  }

  MethodResolution method_tuple(method_idx,
                                GetAccessFlags(method),
                                GetMethodDeclaringClassStringId(dex_file, method_idx, method));
  dex_deps->methods_.insert(method_tuple);
}

ObjPtr<mirror::Class> VerifierDeps::FindOneClassPathBoundaryForInterface(
    ObjPtr<mirror::Class> destination,
    ObjPtr<mirror::Class> source) const {
  DCHECK(destination->IsInterface());
  DCHECK(IsInClassPath(destination));
  Thread* thread = Thread::Current();
  ObjPtr<mirror::Class> current = source;
  // Record the classes that are at the boundary between the compiled DEX files and
  // the classpath. We will check those classes later to find one class that inherits
  // `destination`.
  std::vector<ObjPtr<mirror::Class>> boundaries;
  // If the destination is a direct interface of a class defined in the DEX files being
  // compiled, no need to record it.
  while (!IsInClassPath(current)) {
    for (size_t i = 0; i < current->NumDirectInterfaces(); ++i) {
      ObjPtr<mirror::Class> direct = mirror::Class::GetDirectInterface(thread, current, i);
      if (direct == destination) {
        return nullptr;
      } else if (IsInClassPath(direct)) {
        boundaries.push_back(direct);
      }
    }
    current = current->GetSuperClass();
  }
  DCHECK(current != nullptr);
  boundaries.push_back(current);

  // Check if we have an interface defined in the DEX files being compiled, direclty
  // inheriting `destination`.
  int32_t iftable_count = source->GetIfTableCount();
  ObjPtr<mirror::IfTable> iftable = source->GetIfTable();
  for (int32_t i = 0; i < iftable_count; ++i) {
    ObjPtr<mirror::Class> itf = iftable->GetInterface(i);
    if (!IsInClassPath(itf)) {
      for (size_t j = 0; j < itf->NumDirectInterfaces(); ++j) {
        ObjPtr<mirror::Class> direct = mirror::Class::GetDirectInterface(thread, itf, j);
        if (direct == destination) {
          return nullptr;
        } else if (IsInClassPath(direct)) {
          boundaries.push_back(direct);
        }
      }
    }
  }

  // Find a boundary making `source` inherit from `destination`. We must find one.
  for (const ObjPtr<mirror::Class>& boundary : boundaries) {
    if (destination->IsAssignableFrom(boundary)) {
      return boundary;
    }
  }
  LOG(FATAL) << "Should have found a classpath boundary";
  UNREACHABLE();
}

void VerifierDeps::AddAssignability(const DexFile& dex_file,
                                    ObjPtr<mirror::Class> destination,
                                    ObjPtr<mirror::Class> source,
                                    bool is_strict,
                                    bool is_assignable) {
  // Test that the method is only called on reference types.
  // Note that concurrent verification of `destination` and `source` may have
  // set their status to erroneous. However, the tests performed below rely
  // merely on no issues with linking (valid access flags, superclass and
  // implemented interfaces). If the class at any point reached the IsResolved
  // status, the requirement holds. This is guaranteed by RegTypeCache::ResolveClass.
  DCHECK(destination != nullptr);
  DCHECK(source != nullptr);

  if (destination->IsPrimitive() || source->IsPrimitive()) {
    // Primitive types are trivially non-assignable to anything else.
    // We do not need to record trivial assignability, as it will
    // not change across releases.
    return;
  }

  if (source->IsObjectClass() && !is_assignable) {
    // j.l.Object is trivially non-assignable to other types, don't
    // record it.
    return;
  }

  if (destination == source ||
      destination->IsObjectClass() ||
      (!is_strict && destination->IsInterface())) {
    // Cases when `destination` is trivially assignable from `source`.
    DCHECK(is_assignable);
    return;
  }

  if (destination->IsArrayClass() && source->IsArrayClass()) {
    // Both types are arrays. Break down to component types and add recursively.
    // This helps filter out destinations from compiled DEX files (see below)
    // and deduplicate entries with the same canonical component type.
    ObjPtr<mirror::Class> destination_component = destination->GetComponentType();
    ObjPtr<mirror::Class> source_component = source->GetComponentType();

    // Only perform the optimization if both types are resolved which guarantees
    // that they linked successfully, as required at the top of this method.
    if (destination_component->IsResolved() && source_component->IsResolved()) {
      AddAssignability(dex_file,
                       destination_component,
                       source_component,
                       /* is_strict= */ true,
                       is_assignable);
      return;
    }
  } else {
    // We only do this check for non-array types, as arrays might have erroneous
    // component types which makes the IsAssignableFrom check unreliable.
    DCHECK_EQ(is_assignable, destination->IsAssignableFrom(source));
  }

  DexFileDeps* dex_deps = GetDexFileDeps(dex_file);
  if (dex_deps == nullptr) {
    // This invocation is from verification of a DEX file which is not being compiled.
    return;
  }

  if (!IsInClassPath(destination) && !IsInClassPath(source)) {
    // Both `destination` and `source` are defined in the compiled DEX files.
    // No need to record a dependency.
    return;
  }

  if (!IsInClassPath(source)) {
    if (!destination->IsInterface() && !source->IsInterface()) {
      // Find the super class at the classpath boundary. Only that class
      // can change the assignability.
      do {
        source = source->GetSuperClass();
      } while (!IsInClassPath(source));

      // If that class is the actual destination, no need to record it.
      if (source == destination) {
        return;
      }
    } else if (is_assignable) {
      source = FindOneClassPathBoundaryForInterface(destination, source);
      if (source == nullptr) {
        // There was no classpath boundary, no need to record.
        return;
      }
      DCHECK(IsInClassPath(source));
    }
  }


  // Get string IDs for both descriptors and store in the appropriate set.
  dex::StringIndex destination_id = GetClassDescriptorStringId(dex_file, destination);
  dex::StringIndex source_id = GetClassDescriptorStringId(dex_file, source);

  if (is_assignable) {
    dex_deps->assignable_types_.emplace(TypeAssignability(destination_id, source_id));
  } else {
    dex_deps->unassignable_types_.emplace(TypeAssignability(destination_id, source_id));
  }
}

void VerifierDeps::MaybeRecordClassRedefinition(const DexFile& dex_file,
                                                const dex::ClassDef& class_def) {
  VerifierDeps* thread_deps = GetThreadLocalVerifierDeps();
  if (thread_deps != nullptr) {
    DexFileDeps* dex_deps = thread_deps->GetDexFileDeps(dex_file);
    DCHECK_EQ(dex_deps->redefined_classes_.size(), dex_file.NumClassDefs());
    dex_deps->redefined_classes_[dex_file.GetIndexForClassDef(class_def)] = true;
  }
}

void VerifierDeps::MaybeRecordVerificationStatus(const DexFile& dex_file,
                                                 const dex::ClassDef& class_def,
                                                 FailureKind failure_kind) {
  // The `verified_classes_` bit vector is initialized to `false`.
  // Only continue if we are about to write `true`.
  if (failure_kind == FailureKind::kNoFailure) {
    VerifierDeps* thread_deps = GetThreadLocalVerifierDeps();
    if (thread_deps != nullptr) {
      thread_deps->RecordClassVerified(dex_file, class_def);
    }
  }
}

void VerifierDeps::RecordClassVerified(const DexFile& dex_file, const dex::ClassDef& class_def) {
  DexFileDeps* dex_deps = GetDexFileDeps(dex_file);
  DCHECK_EQ(dex_deps->verified_classes_.size(), dex_file.NumClassDefs());
  dex_deps->verified_classes_[dex_file.GetIndexForClassDef(class_def)] = true;
}

void VerifierDeps::MaybeRecordClassResolution(const DexFile& dex_file,
                                              dex::TypeIndex type_idx,
                                              ObjPtr<mirror::Class> klass) {
  VerifierDeps* thread_deps = GetThreadLocalVerifierDeps();
  if (thread_deps != nullptr) {
    thread_deps->AddClassResolution(dex_file, type_idx, klass);
  }
}

void VerifierDeps::MaybeRecordFieldResolution(const DexFile& dex_file,
                                              uint32_t field_idx,
                                              ArtField* field) {
  VerifierDeps* thread_deps = GetThreadLocalVerifierDeps();
  if (thread_deps != nullptr) {
    thread_deps->AddFieldResolution(dex_file, field_idx, field);
  }
}

void VerifierDeps::MaybeRecordMethodResolution(const DexFile& dex_file,
                                               uint32_t method_idx,
                                               ArtMethod* method) {
  VerifierDeps* thread_deps = GetThreadLocalVerifierDeps();
  if (thread_deps != nullptr) {
    thread_deps->AddMethodResolution(dex_file, method_idx, method);
  }
}

void VerifierDeps::MaybeRecordAssignability(const DexFile& dex_file,
                                            ObjPtr<mirror::Class> destination,
                                            ObjPtr<mirror::Class> source,
                                            bool is_strict,
                                            bool is_assignable) {
  VerifierDeps* thread_deps = GetThreadLocalVerifierDeps();
  if (thread_deps != nullptr) {
    thread_deps->AddAssignability(dex_file, destination, source, is_strict, is_assignable);
  }
}

namespace {

static inline uint32_t DecodeUint32WithOverflowCheck(const uint8_t** in, const uint8_t* end) {
  CHECK_LT(*in, end);
  return DecodeUnsignedLeb128(in);
}

template<typename T> inline uint32_t Encode(T in);

template<> inline uint32_t Encode<uint16_t>(uint16_t in) {
  return in;
}
template<> inline uint32_t Encode<uint32_t>(uint32_t in) {
  return in;
}
template<> inline uint32_t Encode<dex::TypeIndex>(dex::TypeIndex in) {
  return in.index_;
}
template<> inline uint32_t Encode<dex::StringIndex>(dex::StringIndex in) {
  return in.index_;
}

template<typename T> inline T Decode(uint32_t in);

template<> inline uint16_t Decode<uint16_t>(uint32_t in) {
  return dchecked_integral_cast<uint16_t>(in);
}
template<> inline uint32_t Decode<uint32_t>(uint32_t in) {
  return in;
}
template<> inline dex::TypeIndex Decode<dex::TypeIndex>(uint32_t in) {
  return dex::TypeIndex(in);
}
template<> inline dex::StringIndex Decode<dex::StringIndex>(uint32_t in) {
  return dex::StringIndex(in);
}

template<typename T1, typename T2>
static inline void EncodeTuple(std::vector<uint8_t>* out, const std::tuple<T1, T2>& t) {
  EncodeUnsignedLeb128(out, Encode(std::get<0>(t)));
  EncodeUnsignedLeb128(out, Encode(std::get<1>(t)));
}

template<typename T1, typename T2>
static inline void DecodeTuple(const uint8_t** in, const uint8_t* end, std::tuple<T1, T2>* t) {
  T1 v1 = Decode<T1>(DecodeUint32WithOverflowCheck(in, end));
  T2 v2 = Decode<T2>(DecodeUint32WithOverflowCheck(in, end));
  *t = std::make_tuple(v1, v2);
}

template<typename T1, typename T2, typename T3>
static inline void EncodeTuple(std::vector<uint8_t>* out, const std::tuple<T1, T2, T3>& t) {
  EncodeUnsignedLeb128(out, Encode(std::get<0>(t)));
  EncodeUnsignedLeb128(out, Encode(std::get<1>(t)));
  EncodeUnsignedLeb128(out, Encode(std::get<2>(t)));
}

template<typename T1, typename T2, typename T3>
static inline void DecodeTuple(const uint8_t** in, const uint8_t* end, std::tuple<T1, T2, T3>* t) {
  T1 v1 = Decode<T1>(DecodeUint32WithOverflowCheck(in, end));
  T2 v2 = Decode<T2>(DecodeUint32WithOverflowCheck(in, end));
  T3 v3 = Decode<T3>(DecodeUint32WithOverflowCheck(in, end));
  *t = std::make_tuple(v1, v2, v3);
}

template<typename T>
static inline void EncodeSet(std::vector<uint8_t>* out, const std::set<T>& set) {
  EncodeUnsignedLeb128(out, set.size());
  for (const T& entry : set) {
    EncodeTuple(out, entry);
  }
}

template<typename T>
static inline void DecodeSet(const uint8_t** in, const uint8_t* end, std::set<T>* set) {
  DCHECK(set->empty());
  size_t num_entries = DecodeUint32WithOverflowCheck(in, end);
  for (size_t i = 0; i < num_entries; ++i) {
    T tuple;
    DecodeTuple(in, end, &tuple);
    set->emplace(tuple);
  }
}

static inline void EncodeUint16SparseBitVector(std::vector<uint8_t>* out,
                                               const std::vector<bool>& vector,
                                               bool sparse_value) {
  DCHECK(IsUint<16>(vector.size()));
  EncodeUnsignedLeb128(out, std::count(vector.begin(), vector.end(), sparse_value));
  for (uint16_t idx = 0; idx < vector.size(); ++idx) {
    if (vector[idx] == sparse_value) {
      EncodeUnsignedLeb128(out, Encode(idx));
    }
  }
}

static inline void DecodeUint16SparseBitVector(const uint8_t** in,
                                               const uint8_t* end,
                                               std::vector<bool>* vector,
                                               bool sparse_value) {
  DCHECK(IsUint<16>(vector->size()));
  std::fill(vector->begin(), vector->end(), !sparse_value);
  size_t num_entries = DecodeUint32WithOverflowCheck(in, end);
  for (size_t i = 0; i < num_entries; ++i) {
    uint16_t idx = Decode<uint16_t>(DecodeUint32WithOverflowCheck(in, end));
    DCHECK_LT(idx, vector->size());
    (*vector)[idx] = sparse_value;
  }
}

static inline void EncodeStringVector(std::vector<uint8_t>* out,
                                      const std::vector<std::string>& strings) {
  EncodeUnsignedLeb128(out, strings.size());
  for (const std::string& str : strings) {
    const uint8_t* data = reinterpret_cast<const uint8_t*>(str.c_str());
    size_t length = str.length() + 1;
    out->insert(out->end(), data, data + length);
    DCHECK_EQ(0u, out->back());
  }
}

static inline void DecodeStringVector(const uint8_t** in,
                                      const uint8_t* end,
                                      std::vector<std::string>* strings) {
  DCHECK(strings->empty());
  size_t num_strings = DecodeUint32WithOverflowCheck(in, end);
  strings->reserve(num_strings);
  for (size_t i = 0; i < num_strings; ++i) {
    CHECK_LT(*in, end);
    const char* string_start = reinterpret_cast<const char*>(*in);
    strings->emplace_back(std::string(string_start));
    *in += strings->back().length() + 1;
  }
}

static inline std::string ToHex(uint32_t value) {
  std::stringstream ss;
  ss << std::hex << value << std::dec;
  return ss.str();
}

}  // namespace

void VerifierDeps::Encode(const std::vector<const DexFile*>& dex_files,
                          std::vector<uint8_t>* buffer) const {
  for (const DexFile* dex_file : dex_files) {
    const DexFileDeps& deps = *GetDexFileDeps(*dex_file);
    EncodeStringVector(buffer, deps.strings_);
    EncodeSet(buffer, deps.assignable_types_);
    EncodeSet(buffer, deps.unassignable_types_);
    EncodeSet(buffer, deps.classes_);
    EncodeSet(buffer, deps.fields_);
    EncodeSet(buffer, deps.methods_);
    EncodeUint16SparseBitVector(buffer, deps.verified_classes_, /* sparse_value= */ false);
    EncodeUint16SparseBitVector(buffer, deps.redefined_classes_, /* sparse_value= */ true);
  }
}

void VerifierDeps::DecodeDexFileDeps(DexFileDeps& deps,
                                     const uint8_t** data_start,
                                     const uint8_t* data_end) {
  DecodeStringVector(data_start, data_end, &deps.strings_);
  DecodeSet(data_start, data_end, &deps.assignable_types_);
  DecodeSet(data_start, data_end, &deps.unassignable_types_);
  DecodeSet(data_start, data_end, &deps.classes_);
  DecodeSet(data_start, data_end, &deps.fields_);
  DecodeSet(data_start, data_end, &deps.methods_);
  DecodeUint16SparseBitVector(data_start,
                              data_end,
                              &deps.verified_classes_,
                              /* sparse_value= */ false);
  DecodeUint16SparseBitVector(data_start,
                              data_end,
                              &deps.redefined_classes_,
                              /* sparse_value= */ true);
}

VerifierDeps::VerifierDeps(const std::vector<const DexFile*>& dex_files,
                           ArrayRef<const uint8_t> data)
    : VerifierDeps(dex_files, /*output_only=*/ false) {
  if (data.empty()) {
    // Return eagerly, as the first thing we expect from VerifierDeps data is
    // the number of created strings, even if there is no dependency.
    // Currently, only the boot image does not have any VerifierDeps data.
    return;
  }
  const uint8_t* data_start = data.data();
  const uint8_t* data_end = data_start + data.size();
  for (const DexFile* dex_file : dex_files) {
    DexFileDeps* deps = GetDexFileDeps(*dex_file);
    DecodeDexFileDeps(*deps, &data_start, data_end);
  }
  CHECK_LE(data_start, data_end);
}

std::vector<std::vector<bool>> VerifierDeps::ParseVerifiedClasses(
    const std::vector<const DexFile*>& dex_files,
    ArrayRef<const uint8_t> data) {
  DCHECK(!data.empty());
  DCHECK(!dex_files.empty());

  std::vector<std::vector<bool>> verified_classes_per_dex;
  verified_classes_per_dex.reserve(dex_files.size());

  const uint8_t* data_start = data.data();
  const uint8_t* data_end = data_start + data.size();
  for (const DexFile* dex_file : dex_files) {
    DexFileDeps deps(dex_file->NumClassDefs());
    DecodeDexFileDeps(deps, &data_start, data_end);
    verified_classes_per_dex.push_back(std::move(deps.verified_classes_));
  }
  return verified_classes_per_dex;
}

bool VerifierDeps::Equals(const VerifierDeps& rhs) const {
  if (dex_deps_.size() != rhs.dex_deps_.size()) {
    return false;
  }

  auto lhs_it = dex_deps_.begin();
  auto rhs_it = rhs.dex_deps_.begin();

  for (; (lhs_it != dex_deps_.end()) && (rhs_it != rhs.dex_deps_.end()); lhs_it++, rhs_it++) {
    const DexFile* lhs_dex_file = lhs_it->first;
    const DexFile* rhs_dex_file = rhs_it->first;
    if (lhs_dex_file != rhs_dex_file) {
      return false;
    }

    DexFileDeps* lhs_deps = lhs_it->second.get();
    DexFileDeps* rhs_deps = rhs_it->second.get();
    if (!lhs_deps->Equals(*rhs_deps)) {
      return false;
    }
  }

  DCHECK((lhs_it == dex_deps_.end()) && (rhs_it == rhs.dex_deps_.end()));
  return true;
}

bool VerifierDeps::DexFileDeps::Equals(const VerifierDeps::DexFileDeps& rhs) const {
  return (strings_ == rhs.strings_) &&
         (assignable_types_ == rhs.assignable_types_) &&
         (unassignable_types_ == rhs.unassignable_types_) &&
         (classes_ == rhs.classes_) &&
         (fields_ == rhs.fields_) &&
         (methods_ == rhs.methods_) &&
         (verified_classes_ == rhs.verified_classes_);
}

void VerifierDeps::Dump(VariableIndentationOutputStream* vios) const {
  for (const auto& dep : dex_deps_) {
    const DexFile& dex_file = *dep.first;
    vios->Stream()
        << "Dependencies of "
        << dex_file.GetLocation()
        << ":\n";

    ScopedIndentation indent(vios);

    for (const std::string& str : dep.second->strings_) {
      vios->Stream() << "Extra string: " << str << "\n";
    }

    for (const TypeAssignability& entry : dep.second->assignable_types_) {
      vios->Stream()
        << GetStringFromId(dex_file, entry.GetSource())
        << " must be assignable to "
        << GetStringFromId(dex_file, entry.GetDestination())
        << "\n";
    }

    for (const TypeAssignability& entry : dep.second->unassignable_types_) {
      vios->Stream()
        << GetStringFromId(dex_file, entry.GetSource())
        << " must not be assignable to "
        << GetStringFromId(dex_file, entry.GetDestination())
        << "\n";
    }

    for (const ClassResolution& entry : dep.second->classes_) {
      vios->Stream()
          << dex_file.StringByTypeIdx(entry.GetDexTypeIndex())
          << (entry.IsResolved() ? " must be resolved " : "must not be resolved ")
          << " with access flags " << std::hex << entry.GetAccessFlags() << std::dec
          << "\n";
    }

    for (const FieldResolution& entry : dep.second->fields_) {
      const dex::FieldId& field_id = dex_file.GetFieldId(entry.GetDexFieldIndex());
      vios->Stream()
          << dex_file.GetFieldDeclaringClassDescriptor(field_id) << "->"
          << dex_file.GetFieldName(field_id) << ":"
          << dex_file.GetFieldTypeDescriptor(field_id)
          << " is expected to be ";
      if (!entry.IsResolved()) {
        vios->Stream() << "unresolved\n";
      } else {
        vios->Stream()
          << "in class "
          << GetStringFromId(dex_file, entry.GetDeclaringClassIndex())
          << ", and have the access flags " << std::hex << entry.GetAccessFlags() << std::dec
          << "\n";
      }
    }

    for (const MethodResolution& method : dep.second->methods_) {
      const dex::MethodId& method_id = dex_file.GetMethodId(method.GetDexMethodIndex());
      vios->Stream()
          << dex_file.GetMethodDeclaringClassDescriptor(method_id) << "->"
          << dex_file.GetMethodName(method_id)
          << dex_file.GetMethodSignature(method_id).ToString()
          << " is expected to be ";
      if (!method.IsResolved()) {
        vios->Stream() << "unresolved\n";
      } else {
        vios->Stream()
          << "in class "
          << GetStringFromId(dex_file, method.GetDeclaringClassIndex())
          << ", have the access flags " << std::hex << method.GetAccessFlags() << std::dec
          << "\n";
      }
    }

    for (size_t idx = 0; idx < dep.second->verified_classes_.size(); idx++) {
      if (!dep.second->verified_classes_[idx]) {
        vios->Stream()
            << dex_file.GetClassDescriptor(dex_file.GetClassDef(idx))
            << " will be verified at runtime\n";
      }
    }
  }
}

bool VerifierDeps::ValidateDependencies(Thread* self,
                                        Handle<mirror::ClassLoader> class_loader,
                                        const std::vector<const DexFile*>& classpath,
                                        /* out */ std::string* error_msg) const {
  for (const auto& entry : dex_deps_) {
    if (!VerifyDexFile(class_loader, *entry.first, *entry.second, classpath, self, error_msg)) {
      return false;
    }
  }
  return true;
}

// TODO: share that helper with other parts of the compiler that have
// the same lookup pattern.
static ObjPtr<mirror::Class> FindClassAndClearException(ClassLinker* class_linker,
                                                        Thread* self,
                                                        const std::string& name,
                                                        Handle<mirror::ClassLoader> class_loader)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ObjPtr<mirror::Class> result = class_linker->FindClass(self, name.c_str(), class_loader);
  if (result == nullptr) {
    DCHECK(self->IsExceptionPending());
    self->ClearException();
  }
  return result;
}

bool VerifierDeps::VerifyAssignability(Handle<mirror::ClassLoader> class_loader,
                                       const DexFile& dex_file,
                                       const std::set<TypeAssignability>& assignables,
                                       bool expected_assignability,
                                       Thread* self,
                                       /* out */ std::string* error_msg) const {
  StackHandleScope<2> hs(self);
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  MutableHandle<mirror::Class> source(hs.NewHandle<mirror::Class>(nullptr));
  MutableHandle<mirror::Class> destination(hs.NewHandle<mirror::Class>(nullptr));

  for (const auto& entry : assignables) {
    const std::string& destination_desc = GetStringFromId(dex_file, entry.GetDestination());
    destination.Assign(
        FindClassAndClearException(class_linker, self, destination_desc.c_str(), class_loader));
    const std::string& source_desc = GetStringFromId(dex_file, entry.GetSource());
    source.Assign(
        FindClassAndClearException(class_linker, self, source_desc.c_str(), class_loader));

    if (destination == nullptr) {
      *error_msg = "Could not resolve class " + destination_desc;
      return false;
    }

    if (source == nullptr) {
      *error_msg = "Could not resolve class " + source_desc;
      return false;
    }

    DCHECK(destination->IsResolved() && source->IsResolved());
    if (destination->IsAssignableFrom(source.Get()) != expected_assignability) {
      *error_msg = "Class " + destination_desc + (expected_assignability ? " not " : " ") +
          "assignable from " + source_desc;
      return false;
    }
  }
  return true;
}

bool VerifierDeps::VerifyClasses(Handle<mirror::ClassLoader> class_loader,
                                 const DexFile& dex_file,
                                 const std::set<ClassResolution>& classes,
                                 Thread* self,
                                 /* out */ std::string* error_msg) const {
  StackHandleScope<1> hs(self);
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  MutableHandle<mirror::Class> cls(hs.NewHandle<mirror::Class>(nullptr));
  for (const auto& entry : classes) {
    std::string descriptor = dex_file.StringByTypeIdx(entry.GetDexTypeIndex());
    cls.Assign(FindClassAndClearException(class_linker, self, descriptor, class_loader));

    if (entry.IsResolved()) {
      if (cls == nullptr) {
        *error_msg = "Could not resolve class " + descriptor;
        return false;
      } else if (entry.GetAccessFlags() != GetAccessFlags(cls.Get())) {
        *error_msg = "Unexpected access flags on class " + descriptor
            + " (expected=" + ToHex(entry.GetAccessFlags())
            + ", actual=" + ToHex(GetAccessFlags(cls.Get())) + ")";
        return false;
      }
    } else if (cls != nullptr) {
      *error_msg = "Unexpected successful resolution of class " + descriptor;
      return false;
    }
  }
  return true;
}

static std::string GetFieldDescription(const DexFile& dex_file, uint32_t index) {
  const dex::FieldId& field_id = dex_file.GetFieldId(index);
  return std::string(dex_file.GetFieldDeclaringClassDescriptor(field_id))
      + "->"
      + dex_file.GetFieldName(field_id)
      + ":"
      + dex_file.GetFieldTypeDescriptor(field_id);
}

bool VerifierDeps::VerifyFields(Handle<mirror::ClassLoader> class_loader,
                                const DexFile& dex_file,
                                const std::set<FieldResolution>& fields,
                                Thread* self,
                                /* out */ std::string* error_msg) const {
  // Check recorded fields are resolved the same way, have the same recorded class,
  // and have the same recorded flags.
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  for (const auto& entry : fields) {
    const dex::FieldId& field_id = dex_file.GetFieldId(entry.GetDexFieldIndex());
    std::string_view name(dex_file.StringDataByIdx(field_id.name_idx_));
    std::string_view type(
        dex_file.StringDataByIdx(dex_file.GetTypeId(field_id.type_idx_).descriptor_idx_));
    // Only use field_id.class_idx_ when the entry is unresolved, which is rare.
    // Otherwise, we might end up resolving an application class, which is expensive.
    std::string expected_decl_klass = entry.IsResolved()
        ? GetStringFromId(dex_file, entry.GetDeclaringClassIndex())
        : dex_file.StringByTypeIdx(field_id.class_idx_);
    ObjPtr<mirror::Class> cls = FindClassAndClearException(
        class_linker, self, expected_decl_klass.c_str(), class_loader);
    if (cls == nullptr) {
      *error_msg = "Could not resolve class " + expected_decl_klass;
      return false;
    }
    DCHECK(cls->IsResolved());

    ArtField* field = mirror::Class::FindField(self, cls, name, type);
    if (entry.IsResolved()) {
      std::string temp;
      if (field == nullptr) {
        *error_msg = "Could not resolve field " +
            GetFieldDescription(dex_file, entry.GetDexFieldIndex());
        return false;
      } else if (expected_decl_klass != field->GetDeclaringClass()->GetDescriptor(&temp)) {
        *error_msg = "Unexpected declaring class for field resolution "
            + GetFieldDescription(dex_file, entry.GetDexFieldIndex())
            + " (expected=" + expected_decl_klass
            + ", actual=" + field->GetDeclaringClass()->GetDescriptor(&temp) + ")";
        return false;
      } else if (entry.GetAccessFlags() != GetAccessFlags(field)) {
        *error_msg = "Unexpected access flags for resolved field "
            + GetFieldDescription(dex_file, entry.GetDexFieldIndex())
            + " (expected=" + ToHex(entry.GetAccessFlags())
            + ", actual=" + ToHex(GetAccessFlags(field)) + ")";
        return false;
      }
    } else if (field != nullptr) {
      *error_msg = "Unexpected successful resolution of field "
          + GetFieldDescription(dex_file, entry.GetDexFieldIndex());
      return false;
    }
  }
  return true;
}

static std::string GetMethodDescription(const DexFile& dex_file, uint32_t index) {
  const dex::MethodId& method_id = dex_file.GetMethodId(index);
  return std::string(dex_file.GetMethodDeclaringClassDescriptor(method_id))
      + "->"
      + dex_file.GetMethodName(method_id)
      + dex_file.GetMethodSignature(method_id).ToString();
}

bool VerifierDeps::VerifyMethods(Handle<mirror::ClassLoader> class_loader,
                                 const DexFile& dex_file,
                                 const std::set<MethodResolution>& methods,
                                 Thread* self,
                                 /* out */ std::string* error_msg) const {
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  PointerSize pointer_size = class_linker->GetImagePointerSize();

  for (const auto& entry : methods) {
    const dex::MethodId& method_id = dex_file.GetMethodId(entry.GetDexMethodIndex());

    const char* name = dex_file.GetMethodName(method_id);
    const Signature signature = dex_file.GetMethodSignature(method_id);
    // Only use method_id.class_idx_ when the entry is unresolved, which is rare.
    // Otherwise, we might end up resolving an application class, which is expensive.
    std::string expected_decl_klass = entry.IsResolved()
        ? GetStringFromId(dex_file, entry.GetDeclaringClassIndex())
        : dex_file.StringByTypeIdx(method_id.class_idx_);

    ObjPtr<mirror::Class> cls = FindClassAndClearException(
        class_linker, self, expected_decl_klass.c_str(), class_loader);
    if (cls == nullptr) {
      *error_msg = "Could not resolve class " + expected_decl_klass;
      return false;
    }
    DCHECK(cls->IsResolved());
    ArtMethod* method = nullptr;
    if (cls->IsInterface()) {
      method = cls->FindInterfaceMethod(name, signature, pointer_size);
    } else {
      method = cls->FindClassMethod(name, signature, pointer_size);
    }

    if (entry.IsResolved()) {
      std::string temp;
      if (method == nullptr) {
        *error_msg = "Could not resolve method "
            + GetMethodDescription(dex_file, entry.GetDexMethodIndex());
        return false;
      } else if (expected_decl_klass != method->GetDeclaringClass()->GetDescriptor(&temp)) {
        *error_msg = "Unexpected declaring class for method resolution "
            + GetMethodDescription(dex_file, entry.GetDexMethodIndex())
            + " (expected=" + expected_decl_klass
            + ", actual=" + method->GetDeclaringClass()->GetDescriptor(&temp) + ")";
        return false;
      } else if (entry.GetAccessFlags() != GetAccessFlags(method)) {
        *error_msg = "Unexpected access flags for resolved method resolution "
            + GetMethodDescription(dex_file, entry.GetDexMethodIndex())
            + " (expected=" + ToHex(entry.GetAccessFlags())
            + ", actual=" + ToHex(GetAccessFlags(method)) + ")";
        return false;
      }
    } else if (method != nullptr) {
      *error_msg = "Unexpected successful resolution of method "
          + GetMethodDescription(dex_file, entry.GetDexMethodIndex());
      return false;
    }
  }
  return true;
}

bool VerifierDeps::IsInDexFiles(const char* descriptor,
                                size_t hash,
                                const std::vector<const DexFile*>& dex_files,
                                /* out */ const DexFile** out_dex_file) const {
  for (const DexFile* dex_file : dex_files) {
    if (OatDexFile::FindClassDef(*dex_file, descriptor, hash) != nullptr) {
      *out_dex_file = dex_file;
      return true;
    }
  }
  return false;
}

bool VerifierDeps::VerifyInternalClasses(const DexFile& dex_file,
                                         const std::vector<const DexFile*>& classpath,
                                         const std::vector<bool>& verified_classes,
                                         const std::vector<bool>& redefined_classes,
                                         /* out */ std::string* error_msg) const {
  const std::vector<const DexFile*>& boot_classpath =
      Runtime::Current()->GetClassLinker()->GetBootClassPath();

  for (ClassAccessor accessor : dex_file.GetClasses()) {
    const char* descriptor = accessor.GetDescriptor();

    const uint16_t class_def_index = accessor.GetClassDefIndex();
    if (redefined_classes[class_def_index]) {
      if (verified_classes[class_def_index]) {
        *error_msg = std::string("Class ") + descriptor + " marked both verified and redefined";
        return false;
      }

      // Class was not verified under these dependencies. No need to check it further.
      continue;
    }

    // Check that the class resolved into the same dex file. Otherwise there is
    // a different class with the same descriptor somewhere in one of the parent
    // class loaders.
    const size_t hash = ComputeModifiedUtf8Hash(descriptor);
    const DexFile* cp_dex_file = nullptr;
    if (IsInDexFiles(descriptor, hash, boot_classpath, &cp_dex_file) ||
        IsInDexFiles(descriptor, hash, classpath, &cp_dex_file)) {
      *error_msg = std::string("Class ") + descriptor
          + " redefines a class in the classpath "
          + "(dexFile expected=" + dex_file.GetLocation()
          + ", actual=" + cp_dex_file->GetLocation() + ")";
      return false;
    }
  }

  return true;
}

bool VerifierDeps::VerifyDexFile(Handle<mirror::ClassLoader> class_loader,
                                 const DexFile& dex_file,
                                 const DexFileDeps& deps,
                                 const std::vector<const DexFile*>& classpath,
                                 Thread* self,
                                 /* out */ std::string* error_msg) const {
  return VerifyInternalClasses(dex_file,
                               classpath,
                               deps.verified_classes_,
                               deps.redefined_classes_,
                               error_msg) &&
         VerifyAssignability(class_loader,
                             dex_file,
                             deps.assignable_types_,
                             /* expected_assignability= */ true,
                             self,
                             error_msg) &&
         VerifyAssignability(class_loader,
                             dex_file,
                             deps.unassignable_types_,
                             /* expected_assignability= */ false,
                             self,
                             error_msg) &&
         VerifyClasses(class_loader, dex_file, deps.classes_, self, error_msg) &&
         VerifyFields(class_loader, dex_file, deps.fields_, self, error_msg) &&
         VerifyMethods(class_loader, dex_file, deps.methods_, self, error_msg);
}

}  // namespace verifier
}  // namespace art
