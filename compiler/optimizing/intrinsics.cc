/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include "intrinsics.h"

#include "art_field-inl.h"
#include "art_method-inl.h"
#include "base/utils.h"
#include "class_linker.h"
#include "dex/invoke_type.h"
#include "driver/compiler_options.h"
#include "gc/space/image_space.h"
#include "image-inl.h"
#include "intrinsic_objects.h"
#include "nodes.h"
#include "obj_ptr-inl.h"
#include "scoped_thread_state_change-inl.h"
#include "thread-current-inl.h"

namespace art {

// Check that intrinsic enum values fit within space set aside in ArtMethod modifier flags.
#define CHECK_INTRINSICS_ENUM_VALUES(Name, IsStatic, NeedsEnvironmentOrCache, SideEffects, Exceptions, ...) \
  static_assert( \
      static_cast<uint32_t>(Intrinsics::k ## Name) <= (kAccIntrinsicBits >> CTZ(kAccIntrinsicBits)), \
      "Instrinsics enumeration space overflow.");
#include "intrinsics_list.h"
  INTRINSICS_LIST(CHECK_INTRINSICS_ENUM_VALUES)
#undef INTRINSICS_LIST
#undef CHECK_INTRINSICS_ENUM_VALUES

// Function that returns whether an intrinsic is static/direct or virtual.
static inline InvokeType GetIntrinsicInvokeType(Intrinsics i) {
  switch (i) {
    case Intrinsics::kNone:
      return kInterface;  // Non-sensical for intrinsic.
#define OPTIMIZING_INTRINSICS(Name, IsStatic, NeedsEnvironmentOrCache, SideEffects, Exceptions, ...) \
    case Intrinsics::k ## Name: \
      return IsStatic;
#include "intrinsics_list.h"
      INTRINSICS_LIST(OPTIMIZING_INTRINSICS)
#undef INTRINSICS_LIST
#undef OPTIMIZING_INTRINSICS
  }
  return kInterface;
}

// Function that returns whether an intrinsic needs an environment or not.
static inline IntrinsicNeedsEnvironmentOrCache NeedsEnvironmentOrCache(Intrinsics i) {
  switch (i) {
    case Intrinsics::kNone:
      return kNeedsEnvironmentOrCache;  // Non-sensical for intrinsic.
#define OPTIMIZING_INTRINSICS(Name, IsStatic, NeedsEnvironmentOrCache, SideEffects, Exceptions, ...) \
    case Intrinsics::k ## Name: \
      return NeedsEnvironmentOrCache;
#include "intrinsics_list.h"
      INTRINSICS_LIST(OPTIMIZING_INTRINSICS)
#undef INTRINSICS_LIST
#undef OPTIMIZING_INTRINSICS
  }
  return kNeedsEnvironmentOrCache;
}

// Function that returns whether an intrinsic has side effects.
static inline IntrinsicSideEffects GetSideEffects(Intrinsics i) {
  switch (i) {
    case Intrinsics::kNone:
      return kAllSideEffects;
#define OPTIMIZING_INTRINSICS(Name, IsStatic, NeedsEnvironmentOrCache, SideEffects, Exceptions, ...) \
    case Intrinsics::k ## Name: \
      return SideEffects;
#include "intrinsics_list.h"
      INTRINSICS_LIST(OPTIMIZING_INTRINSICS)
#undef INTRINSICS_LIST
#undef OPTIMIZING_INTRINSICS
  }
  return kAllSideEffects;
}

// Function that returns whether an intrinsic can throw exceptions.
static inline IntrinsicExceptions GetExceptions(Intrinsics i) {
  switch (i) {
    case Intrinsics::kNone:
      return kCanThrow;
#define OPTIMIZING_INTRINSICS(Name, IsStatic, NeedsEnvironmentOrCache, SideEffects, Exceptions, ...) \
    case Intrinsics::k ## Name: \
      return Exceptions;
#include "intrinsics_list.h"
      INTRINSICS_LIST(OPTIMIZING_INTRINSICS)
#undef INTRINSICS_LIST
#undef OPTIMIZING_INTRINSICS
  }
  return kCanThrow;
}

static bool CheckInvokeType(Intrinsics intrinsic, HInvoke* invoke)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  // Whenever the intrinsic is marked as static, report an error if we find an InvokeVirtual.
  //
  // Whenever the intrinsic is marked as direct and we find an InvokeVirtual, a devirtualization
  // failure occured. We might be in a situation where we have inlined a method that calls an
  // intrinsic, but that method is in a different dex file on which we do not have a
  // verified_method that would have helped the compiler driver sharpen the call. In that case,
  // make sure that the intrinsic is actually for some final method (or in a final class), as
  // otherwise the intrinsics setup is broken.
  //
  // For the last direction, we have intrinsics for virtual functions that will perform a check
  // inline. If the precise type is known, however, the instruction will be sharpened to an
  // InvokeStaticOrDirect.
  InvokeType intrinsic_type = GetIntrinsicInvokeType(intrinsic);
  InvokeType invoke_type = invoke->GetInvokeType();

  switch (intrinsic_type) {
    case kStatic:
      return (invoke_type == kStatic);

    case kDirect:
      if (invoke_type == kDirect) {
        return true;
      }
      if (invoke_type == kVirtual) {
        ArtMethod* art_method = invoke->GetResolvedMethod();
        return (art_method->IsFinal() || art_method->GetDeclaringClass()->IsFinal());
      }
      return false;

    case kVirtual:
      // Call might be devirtualized.
      return (invoke_type == kVirtual || invoke_type == kDirect || invoke_type == kInterface);

    case kSuper:
    case kInterface:
    case kPolymorphic:
    case kCustom:
      return false;
  }
  LOG(FATAL) << "Unknown intrinsic invoke type: " << intrinsic_type;
  UNREACHABLE();
}

bool IntrinsicsRecognizer::Recognize(HInvoke* invoke,
                                     ArtMethod* art_method,
                                     /*out*/ bool* wrong_invoke_type) {
  if (art_method == nullptr) {
    art_method = invoke->GetResolvedMethod();
  }
  *wrong_invoke_type = false;
  if (art_method == nullptr || !art_method->IsIntrinsic()) {
    return false;
  }

  // TODO: b/65872996 The intent is that polymorphic signature methods should
  // be compiler intrinsics. At present, they are only interpreter intrinsics.
  if (art_method->IsPolymorphicSignature()) {
    return false;
  }

  Intrinsics intrinsic = static_cast<Intrinsics>(art_method->GetIntrinsic());
  if (CheckInvokeType(intrinsic, invoke) == false) {
    *wrong_invoke_type = true;
    return false;
  }

  invoke->SetIntrinsic(intrinsic,
                       NeedsEnvironmentOrCache(intrinsic),
                       GetSideEffects(intrinsic),
                       GetExceptions(intrinsic));
  return true;
}

bool IntrinsicsRecognizer::Run() {
  bool didRecognize = false;
  ScopedObjectAccess soa(Thread::Current());
  for (HBasicBlock* block : graph_->GetReversePostOrder()) {
    for (HInstructionIterator inst_it(block->GetInstructions()); !inst_it.Done();
         inst_it.Advance()) {
      HInstruction* inst = inst_it.Current();
      if (inst->IsInvoke()) {
        bool wrong_invoke_type = false;
        if (Recognize(inst->AsInvoke(), /* art_method */ nullptr, &wrong_invoke_type)) {
          didRecognize = true;
          MaybeRecordStat(stats_, MethodCompilationStat::kIntrinsicRecognized);
        } else if (wrong_invoke_type) {
          LOG(WARNING)
              << "Found an intrinsic with unexpected invoke type: "
              << inst->AsInvoke()->GetResolvedMethod()->PrettyMethod() << " "
              << inst->DebugName();
        }
      }
    }
  }
  return didRecognize;
}

std::ostream& operator<<(std::ostream& os, const Intrinsics& intrinsic) {
  switch (intrinsic) {
    case Intrinsics::kNone:
      os << "None";
      break;
#define OPTIMIZING_INTRINSICS(Name, IsStatic, NeedsEnvironmentOrCache, SideEffects, Exceptions, ...) \
    case Intrinsics::k ## Name: \
      os << # Name; \
      break;
#include "intrinsics_list.h"
      INTRINSICS_LIST(OPTIMIZING_INTRINSICS)
#undef STATIC_INTRINSICS_LIST
#undef VIRTUAL_INTRINSICS_LIST
#undef OPTIMIZING_INTRINSICS
  }
  return os;
}

static const char kIntegerCacheDescriptor[] = "Ljava/lang/Integer$IntegerCache;";
static const char kIntegerDescriptor[] = "Ljava/lang/Integer;";
static const char kIntegerArrayDescriptor[] = "[Ljava/lang/Integer;";
static const char kLowFieldName[] = "low";
static const char kHighFieldName[] = "high";
static const char kValueFieldName[] = "value";

static ObjPtr<mirror::ObjectArray<mirror::Object>> GetBootImageLiveObjects()
    REQUIRES_SHARED(Locks::mutator_lock_) {
  gc::Heap* heap = Runtime::Current()->GetHeap();
  const std::vector<gc::space::ImageSpace*>& boot_image_spaces = heap->GetBootImageSpaces();
  DCHECK(!boot_image_spaces.empty());
  const ImageHeader& main_header = boot_image_spaces[0]->GetImageHeader();
  ObjPtr<mirror::ObjectArray<mirror::Object>> boot_image_live_objects =
      ObjPtr<mirror::ObjectArray<mirror::Object>>::DownCast(
          main_header.GetImageRoot<kWithoutReadBarrier>(ImageHeader::kBootImageLiveObjects));
  DCHECK(boot_image_live_objects != nullptr);
  DCHECK(heap->ObjectIsInBootImageSpace(boot_image_live_objects));
  return boot_image_live_objects;
}

static ObjPtr<mirror::Class> LookupInitializedClass(Thread* self,
                                                    ClassLinker* class_linker,
                                                    const char* descriptor)
        REQUIRES_SHARED(Locks::mutator_lock_) {
  ObjPtr<mirror::Class> klass =
      class_linker->LookupClass(self, descriptor, /* class_loader */ nullptr);
  DCHECK(klass != nullptr);
  DCHECK(klass->IsInitialized());
  return klass;
}

static ObjPtr<mirror::ObjectArray<mirror::Object>> GetIntegerCacheArray(
    ObjPtr<mirror::Class> cache_class) REQUIRES_SHARED(Locks::mutator_lock_) {
  ArtField* cache_field = cache_class->FindDeclaredStaticField("cache", kIntegerArrayDescriptor);
  DCHECK(cache_field != nullptr);
  return ObjPtr<mirror::ObjectArray<mirror::Object>>::DownCast(cache_field->GetObject(cache_class));
}

static int32_t GetIntegerCacheField(ObjPtr<mirror::Class> cache_class, const char* field_name)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ArtField* field = cache_class->FindDeclaredStaticField(field_name, "I");
  DCHECK(field != nullptr);
  return field->GetInt(cache_class);
}

static bool CheckIntegerCache(Thread* self,
                              ClassLinker* class_linker,
                              ObjPtr<mirror::ObjectArray<mirror::Object>> boot_image_live_objects,
                              ObjPtr<mirror::ObjectArray<mirror::Object>> boot_image_cache)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  DCHECK(boot_image_cache != nullptr);

  // Since we have a cache in the boot image, both java.lang.Integer and
  // java.lang.Integer$IntegerCache must be initialized in the boot image.
  ObjPtr<mirror::Class> cache_class =
      LookupInitializedClass(self, class_linker, kIntegerCacheDescriptor);
  ObjPtr<mirror::Class> integer_class =
      LookupInitializedClass(self, class_linker, kIntegerDescriptor);

  // Check that the current cache is the same as the `boot_image_cache`.
  ObjPtr<mirror::ObjectArray<mirror::Object>> current_cache = GetIntegerCacheArray(cache_class);
  if (current_cache != boot_image_cache) {
    return false;  // Messed up IntegerCache.cache.
  }

  // Check that the range matches the boot image cache length.
  int32_t low = GetIntegerCacheField(cache_class, kLowFieldName);
  int32_t high = GetIntegerCacheField(cache_class, kHighFieldName);
  if (boot_image_cache->GetLength() != high - low + 1) {
    return false;  // Messed up IntegerCache.low or IntegerCache.high.
  }

  // Check that the elements match the boot image intrinsic objects and check their values as well.
  ArtField* value_field = integer_class->FindDeclaredInstanceField(kValueFieldName, "I");
  DCHECK(value_field != nullptr);
  for (int32_t i = 0, len = boot_image_cache->GetLength(); i != len; ++i) {
    ObjPtr<mirror::Object> boot_image_object =
        IntrinsicObjects::GetIntegerValueOfObject(boot_image_live_objects, i);
    DCHECK(Runtime::Current()->GetHeap()->ObjectIsInBootImageSpace(boot_image_object));
    // No need for read barrier for comparison with a boot image object.
    ObjPtr<mirror::Object> current_object =
        boot_image_cache->GetWithoutChecks<kVerifyNone, kWithoutReadBarrier>(i);
    if (boot_image_object != current_object) {
      return false;  // Messed up IntegerCache.cache[i]
    }
    if (value_field->GetInt(boot_image_object) != low + i) {
      return false;  // Messed up IntegerCache.cache[i].value.
    }
  }

  return true;
}

void IntrinsicVisitor::ComputeIntegerValueOfLocations(HInvoke* invoke,
                                                      CodeGenerator* codegen,
                                                      Location return_location,
                                                      Location first_argument_location) {
  // The intrinsic will call if it needs to allocate a j.l.Integer.
  LocationSummary::CallKind call_kind = LocationSummary::kCallOnMainOnly;
  const CompilerOptions& compiler_options = codegen->GetCompilerOptions();
  if (compiler_options.IsBootImage()) {
    // Piggyback on the method load kind to determine whether we can use PC-relative addressing.
    // This should cover both the testing config (non-PIC boot image) and codegens that reject
    // PC-relative load kinds and fall back to the runtime call.
    if (!invoke->AsInvokeStaticOrDirect()->HasPcRelativeMethodLoadKind()) {
      return;
    }
    if (!compiler_options.IsImageClass(kIntegerCacheDescriptor) ||
        !compiler_options.IsImageClass(kIntegerDescriptor)) {
      return;
    }
    ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
    Thread* self = Thread::Current();
    ScopedObjectAccess soa(self);
    ObjPtr<mirror::Class> cache_class = class_linker->LookupClass(
        self, kIntegerCacheDescriptor, /* class_loader */ nullptr);
    DCHECK(cache_class != nullptr);
    if (UNLIKELY(!cache_class->IsInitialized())) {
      LOG(WARNING) << "Image class " << cache_class->PrettyDescriptor() << " is uninitialized.";
      return;
    }
    ObjPtr<mirror::Class> integer_class =
        class_linker->LookupClass(self, kIntegerDescriptor, /* class_loader */ nullptr);
    DCHECK(integer_class != nullptr);
    if (UNLIKELY(!integer_class->IsInitialized())) {
      LOG(WARNING) << "Image class " << integer_class->PrettyDescriptor() << " is uninitialized.";
      return;
    }
    int32_t low = GetIntegerCacheField(cache_class, kLowFieldName);
    int32_t high = GetIntegerCacheField(cache_class, kHighFieldName);
    if (kIsDebugBuild) {
      ObjPtr<mirror::ObjectArray<mirror::Object>> current_cache = GetIntegerCacheArray(cache_class);
      CHECK(current_cache != nullptr);
      CHECK_EQ(current_cache->GetLength(), high - low + 1);
      ArtField* value_field = integer_class->FindDeclaredInstanceField(kValueFieldName, "I");
      CHECK(value_field != nullptr);
      for (int32_t i = 0, len = current_cache->GetLength(); i != len; ++i) {
        ObjPtr<mirror::Object> current_object = current_cache->GetWithoutChecks(i);
        CHECK(current_object != nullptr);
        CHECK_EQ(value_field->GetInt(current_object), low + i);
      }
    }
    if (invoke->InputAt(0)->IsIntConstant()) {
      int32_t value = invoke->InputAt(0)->AsIntConstant()->GetValue();
      if (static_cast<uint32_t>(value) - static_cast<uint32_t>(low) <
          static_cast<uint32_t>(high - low + 1)) {
        // No call, we shall use direct pointer to the Integer object.
        call_kind = LocationSummary::kNoCall;
      }
    }
  } else {
    Runtime* runtime = Runtime::Current();
    if (runtime->GetHeap()->GetBootImageSpaces().empty()) {
      return;  // Running without boot image, cannot use required boot image objects.
    }
    Thread* self = Thread::Current();
    ScopedObjectAccess soa(self);
    ObjPtr<mirror::ObjectArray<mirror::Object>> boot_image_live_objects = GetBootImageLiveObjects();
    ObjPtr<mirror::ObjectArray<mirror::Object>> cache =
        IntrinsicObjects::GetIntegerValueOfCache(boot_image_live_objects);
    if (cache == nullptr) {
      return;  // No cache in the boot image.
    }
    if (runtime->UseJitCompilation()) {
      if (!CheckIntegerCache(self, runtime->GetClassLinker(), boot_image_live_objects, cache)) {
        return;  // The cache was somehow messed up, probably by using reflection.
      }
    } else {
      DCHECK(runtime->IsAotCompiler());
      DCHECK(CheckIntegerCache(self, runtime->GetClassLinker(), boot_image_live_objects, cache));
      if (invoke->InputAt(0)->IsIntConstant()) {
        int32_t value = invoke->InputAt(0)->AsIntConstant()->GetValue();
        // Retrieve the `value` from the lowest cached Integer.
        ObjPtr<mirror::Object> low_integer =
            IntrinsicObjects::GetIntegerValueOfObject(boot_image_live_objects, 0u);
        ObjPtr<mirror::Class> integer_class =
            low_integer->GetClass<kVerifyNone, kWithoutReadBarrier>();
        ArtField* value_field = integer_class->FindDeclaredInstanceField(kValueFieldName, "I");
        DCHECK(value_field != nullptr);
        int32_t low = value_field->GetInt(low_integer);
        if (static_cast<uint32_t>(value) - static_cast<uint32_t>(low) <
            static_cast<uint32_t>(cache->GetLength())) {
          // No call, we shall use direct pointer to the Integer object. Note that we cannot
          // do this for JIT as the "low" can change through reflection before emitting the code.
          call_kind = LocationSummary::kNoCall;
        }
      }
    }
  }

  ArenaAllocator* allocator = invoke->GetBlock()->GetGraph()->GetAllocator();
  LocationSummary* locations = new (allocator) LocationSummary(invoke, call_kind, kIntrinsified);
  if (call_kind == LocationSummary::kCallOnMainOnly) {
    locations->SetInAt(0, Location::RegisterOrConstant(invoke->InputAt(0)));
    locations->AddTemp(first_argument_location);
    locations->SetOut(return_location);
  } else {
    locations->SetInAt(0, Location::ConstantLocation(invoke->InputAt(0)->AsConstant()));
    locations->SetOut(Location::RequiresRegister());
  }
}

static int32_t GetIntegerCacheLowFromIntegerCache(Thread* self, ClassLinker* class_linker)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ObjPtr<mirror::Class> cache_class =
      LookupInitializedClass(self, class_linker, kIntegerCacheDescriptor);
  return GetIntegerCacheField(cache_class, kLowFieldName);
}

static uint32_t CalculateBootImageOffset(ObjPtr<mirror::Object> object)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  gc::Heap* heap = Runtime::Current()->GetHeap();
  DCHECK(heap->ObjectIsInBootImageSpace(object));
  return reinterpret_cast<const uint8_t*>(object.Ptr()) - heap->GetBootImageSpaces()[0]->Begin();
}

inline IntrinsicVisitor::IntegerValueOfInfo::IntegerValueOfInfo()
    : value_offset(0),
      low(0),
      length(0u),
      integer_boot_image_offset(kInvalidReference),
      value_boot_image_reference(kInvalidReference) {}

IntrinsicVisitor::IntegerValueOfInfo IntrinsicVisitor::ComputeIntegerValueOfInfo(
    HInvoke* invoke, const CompilerOptions& compiler_options) {
  // Note that we could cache all of the data looked up here. but there's no good
  // location for it. We don't want to add it to WellKnownClasses, to avoid creating global
  // jni values. Adding it as state to the compiler singleton seems like wrong
  // separation of concerns.
  // The need for this data should be pretty rare though.

  // Note that at this point we can no longer abort the code generation. Therefore,
  // we need to provide data that shall not lead to a crash even if the fields were
  // modified through reflection since ComputeIntegerValueOfLocations() when JITting.

  Runtime* runtime = Runtime::Current();
  ClassLinker* class_linker = runtime->GetClassLinker();
  Thread* self = Thread::Current();
  ScopedObjectAccess soa(self);

  IntegerValueOfInfo info;
  if (compiler_options.IsBootImage()) {
    ObjPtr<mirror::Class> integer_class =
        LookupInitializedClass(self, class_linker, kIntegerDescriptor);
    ArtField* value_field = integer_class->FindDeclaredInstanceField(kValueFieldName, "I");
    DCHECK(value_field != nullptr);
    info.value_offset = value_field->GetOffset().Uint32Value();
    ObjPtr<mirror::Class> cache_class =
        LookupInitializedClass(self, class_linker, kIntegerCacheDescriptor);
    info.low = GetIntegerCacheField(cache_class, kLowFieldName);
    int32_t high = GetIntegerCacheField(cache_class, kHighFieldName);
    info.length = dchecked_integral_cast<uint32_t>(high - info.low + 1);

    info.integer_boot_image_offset = IntegerValueOfInfo::kInvalidReference;
    if (invoke->InputAt(0)->IsIntConstant()) {
      int32_t input_value = invoke->InputAt(0)->AsIntConstant()->GetValue();
      uint32_t index = static_cast<uint32_t>(input_value) - static_cast<uint32_t>(info.low);
      if (index < static_cast<uint32_t>(info.length)) {
        info.value_boot_image_reference = IntrinsicObjects::EncodePatch(
            IntrinsicObjects::PatchType::kIntegerValueOfObject, index);
      } else {
        // Not in the cache.
        info.value_boot_image_reference = IntegerValueOfInfo::kInvalidReference;
      }
    } else {
      info.array_data_boot_image_reference =
          IntrinsicObjects::EncodePatch(IntrinsicObjects::PatchType::kIntegerValueOfArray);
    }
  } else {
    ObjPtr<mirror::ObjectArray<mirror::Object>> boot_image_live_objects = GetBootImageLiveObjects();
    ObjPtr<mirror::Object> low_integer =
        IntrinsicObjects::GetIntegerValueOfObject(boot_image_live_objects, 0u);
    ObjPtr<mirror::Class> integer_class = low_integer->GetClass<kVerifyNone, kWithoutReadBarrier>();
    ArtField* value_field = integer_class->FindDeclaredInstanceField(kValueFieldName, "I");
    DCHECK(value_field != nullptr);
    info.value_offset = value_field->GetOffset().Uint32Value();
    if (runtime->UseJitCompilation()) {
      // Use the current `IntegerCache.low` for JIT to avoid truly surprising behavior if the
      // code messes up the `value` field in the lowest cached Integer using reflection.
      info.low = GetIntegerCacheLowFromIntegerCache(self, class_linker);
    } else {
      // For app AOT, the `low_integer->value` should be the same as `IntegerCache.low`.
      info.low = value_field->GetInt(low_integer);
      DCHECK_EQ(info.low, GetIntegerCacheLowFromIntegerCache(self, class_linker));
    }
    // Do not look at `IntegerCache.high`, use the immutable length of the cache array instead.
    info.length = dchecked_integral_cast<uint32_t>(
        IntrinsicObjects::GetIntegerValueOfCache(boot_image_live_objects)->GetLength());

    info.integer_boot_image_offset = CalculateBootImageOffset(integer_class);
    if (invoke->InputAt(0)->IsIntConstant()) {
      int32_t input_value = invoke->InputAt(0)->AsIntConstant()->GetValue();
      uint32_t index = static_cast<uint32_t>(input_value) - static_cast<uint32_t>(info.low);
      if (index < static_cast<uint32_t>(info.length)) {
        ObjPtr<mirror::Object> integer =
            IntrinsicObjects::GetIntegerValueOfObject(boot_image_live_objects, index);
        info.value_boot_image_reference = CalculateBootImageOffset(integer);
      } else {
        // Not in the cache.
        info.value_boot_image_reference = IntegerValueOfInfo::kInvalidReference;
      }
    } else {
      info.array_data_boot_image_reference =
          CalculateBootImageOffset(boot_image_live_objects) +
          IntrinsicObjects::GetIntegerValueOfArrayDataOffset(boot_image_live_objects).Uint32Value();
    }
  }

  return info;
}

}  // namespace art
