/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include "var_handle.h"

#include <string>
#include <vector>

#include "art_field-inl.h"
#include "class-alloc-inl.h"
#include "class-inl.h"
#include "class_linker-inl.h"
#include "class_loader.h"
#include "class_root.h"
#include "common_runtime_test.h"
#include "handle_scope-inl.h"
#include "jvalue-inl.h"
#include "method_type.h"
#include "object_array-alloc-inl.h"
#include "object_array-inl.h"
#include "reflection.h"
#include "scoped_thread_state_change-inl.h"

namespace art {
namespace mirror {

// Tests for mirror::VarHandle and it's descendents.
class VarHandleTest : public CommonRuntimeTest {
 public:
  static ObjPtr<FieldVarHandle> CreateFieldVarHandle(Thread* const self,
                                                     ArtField* art_field,
                                                     int32_t access_modes_bit_mask)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Roles::uninterruptible_) {
    StackHandleScope<4> hs(self);
    Handle<FieldVarHandle> fvh = hs.NewHandle(
        ObjPtr<FieldVarHandle>::DownCast(GetClassRoot<FieldVarHandle>()->AllocObject(self)));
    Handle<Class> var_type = hs.NewHandle(art_field->ResolveType());

    if (art_field->IsStatic()) {
      InitializeVarHandle(fvh.Get(), var_type, access_modes_bit_mask);
    } else {
      Handle<Class> declaring_type = hs.NewHandle(art_field->GetDeclaringClass());
      InitializeVarHandle(fvh.Get(),
                          var_type,
                          declaring_type,
                          access_modes_bit_mask);
    }
    uintptr_t opaque_field = reinterpret_cast<uintptr_t>(art_field);
    fvh->SetField64<false>(FieldVarHandle::ArtFieldOffset(), opaque_field);
    return fvh.Get();
  }

  static ObjPtr<ArrayElementVarHandle> CreateArrayElementVarHandle(Thread* const self,
                                                                   Handle<Class> array_class,
                                                                   int32_t access_modes_bit_mask)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Roles::uninterruptible_) {
    StackHandleScope<3> hs(self);
    Handle<ArrayElementVarHandle> vh = hs.NewHandle(
        ObjPtr<ArrayElementVarHandle>::DownCast(
            GetClassRoot<ArrayElementVarHandle>()->AllocObject(self)));

    // Initialize super class fields
    ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
    Handle<Class> var_type = hs.NewHandle(array_class->GetComponentType());
    Handle<Class> index_type = hs.NewHandle(class_linker->FindPrimitiveClass('I'));
    InitializeVarHandle(vh.Get(), var_type, array_class, index_type, access_modes_bit_mask);
    return vh.Get();
  }

  static ObjPtr<ByteArrayViewVarHandle> CreateByteArrayViewVarHandle(
      Thread* const self,
      Handle<Class> view_array_class,
      bool native_byte_order,
      int32_t access_modes_bit_mask)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Roles::uninterruptible_) {
    StackHandleScope<4> hs(self);
    Handle<ByteArrayViewVarHandle> bvh = hs.NewHandle(
        ObjPtr<ByteArrayViewVarHandle>::DownCast(
            GetClassRoot<ByteArrayViewVarHandle>()->AllocObject(self)));

    // Initialize super class fields
    ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
    Handle<Class> var_type = hs.NewHandle(view_array_class->GetComponentType());
    Handle<Class> index_type = hs.NewHandle(class_linker->FindPrimitiveClass('I'));
    Handle<Class> byte_array_class(hs.NewHandle(GetClassRoot<mirror::ByteArray>()));
    InitializeVarHandle(bvh.Get(), var_type, byte_array_class, index_type, access_modes_bit_mask);
    bvh->SetFieldBoolean<false>(ByteArrayViewVarHandle::NativeByteOrderOffset(), native_byte_order);
    return bvh.Get();
  }

  static ObjPtr<ByteBufferViewVarHandle> CreateByteBufferViewVarHandle(
      Thread* const self,
      Handle<Class> view_array_class,
      bool native_byte_order,
      int32_t access_modes_bit_mask)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Roles::uninterruptible_) {
    StackHandleScope<5> hs(self);
    Handle<ByteBufferViewVarHandle> bvh = hs.NewHandle(
        ObjPtr<ByteBufferViewVarHandle>::DownCast(
            GetClassRoot<ByteArrayViewVarHandle>()->AllocObject(self)));
    // Initialize super class fields
    ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
    Handle<Class> var_type = hs.NewHandle(view_array_class->GetComponentType());
    Handle<Class> index_type = hs.NewHandle(class_linker->FindPrimitiveClass('I'));
    Handle<ClassLoader> boot_class_loader;
    Handle<Class> byte_buffer_class = hs.NewHandle(
        class_linker->FindSystemClass(self, "Ljava/nio/ByteBuffer;"));
    InitializeVarHandle(bvh.Get(), var_type, byte_buffer_class, index_type, access_modes_bit_mask);
    bvh->SetFieldBoolean<false>(ByteBufferViewVarHandle::NativeByteOrderOffset(),
                                native_byte_order);
    return bvh.Get();
  }

  static int32_t AccessModesBitMask(VarHandle::AccessMode mode) {
    return 1 << static_cast<int32_t>(mode);
  }

  template<typename... Args>
  static int32_t AccessModesBitMask(VarHandle::AccessMode first, Args... args) {
    return AccessModesBitMask(first) | AccessModesBitMask(args...);
  }

 private:
  static void InitializeVarHandle(ObjPtr<VarHandle> vh,
                                  Handle<Class> var_type,
                                  int32_t access_modes_bit_mask)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    vh->SetFieldObject<false>(VarHandle::VarTypeOffset(), var_type.Get());
    vh->SetField32<false>(VarHandle::AccessModesBitMaskOffset(), access_modes_bit_mask);
  }

  static void InitializeVarHandle(ObjPtr<VarHandle> vh,
                                  Handle<Class> var_type,
                                  Handle<Class> coordinate_type0,
                                  int32_t access_modes_bit_mask)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    InitializeVarHandle(vh, var_type, access_modes_bit_mask);
    vh->SetFieldObject<false>(VarHandle::CoordinateType0Offset(), coordinate_type0.Get());
  }

  static void InitializeVarHandle(ObjPtr<VarHandle> vh,
                                  Handle<Class> var_type,
                                  Handle<Class> coordinate_type0,
                                  Handle<Class> coordinate_type1,
                                  int32_t access_modes_bit_mask)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    InitializeVarHandle(vh, var_type, access_modes_bit_mask);
    vh->SetFieldObject<false>(VarHandle::CoordinateType0Offset(), coordinate_type0.Get());
    vh->SetFieldObject<false>(VarHandle::CoordinateType1Offset(), coordinate_type1.Get());
  }
};

// Convenience method for constructing MethodType instances from
// well-formed method descriptors.
static ObjPtr<MethodType> MethodTypeOf(const std::string& method_descriptor) {
  std::vector<std::string> descriptors;

  auto it = method_descriptor.cbegin();
  if (*it++ != '(') {
    LOG(FATAL) << "Bad descriptor: " << method_descriptor;
  }

  bool returnValueSeen = false;
  const char* prefix = "";
  for (; it != method_descriptor.cend() && !returnValueSeen; ++it) {
    switch (*it) {
      case ')':
        descriptors.push_back(std::string(++it, method_descriptor.cend()));
        returnValueSeen = true;
        break;
      case '[':
        prefix = "[";
        break;
      case 'Z':
      case 'B':
      case 'C':
      case 'S':
      case 'I':
      case 'J':
      case 'F':
      case 'D':
        descriptors.push_back(prefix + std::string(it, it + 1));
        prefix = "";
        break;
      case 'L': {
        auto last = it + 1;
        while (*last != ';') {
          ++last;
        }
        descriptors.push_back(prefix + std::string(it, last + 1));
        prefix = "";
        it = last;
        break;
      }
      default:
        LOG(FATAL) << "Bad descriptor: " << method_descriptor;
    }
  }

  Runtime* const runtime = Runtime::Current();
  ClassLinker* const class_linker = runtime->GetClassLinker();
  Thread* const self = Thread::Current();

  ScopedObjectAccess soa(self);
  StackHandleScope<3> hs(self);
  int ptypes_count = static_cast<int>(descriptors.size()) - 1;
  ObjPtr<mirror::Class> array_of_class = GetClassRoot<mirror::ObjectArray<mirror::Class>>();
  Handle<ObjectArray<Class>> ptypes = hs.NewHandle(
      ObjectArray<Class>::Alloc(Thread::Current(), array_of_class, ptypes_count));
  Handle<mirror::ClassLoader> boot_class_loader = hs.NewHandle<mirror::ClassLoader>(nullptr);
  for (int i = 0; i < ptypes_count; ++i) {
    ptypes->Set(i, class_linker->FindClass(self, descriptors[i].c_str(), boot_class_loader));
  }
  Handle<Class> rtype =
      hs.NewHandle(class_linker->FindClass(self, descriptors.back().c_str(), boot_class_loader));
  return MethodType::Create(self, rtype, ptypes);
}

static bool AccessModeMatch(ObjPtr<VarHandle> vh,
                            VarHandle::AccessMode access_mode,
                            ObjPtr<MethodType> method_type,
                            VarHandle::MatchKind expected_match)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  return vh->GetMethodTypeMatchForAccessMode(access_mode, method_type) == expected_match;
}

template <typename VH>
static bool AccessModeExactMatch(Handle<VH> vh,
                                 VarHandle::AccessMode access_mode,
                                 const char* descriptor)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ObjPtr<MethodType> method_type = MethodTypeOf(descriptor);
  return AccessModeMatch(vh.Get(),
                         access_mode,
                         method_type,
                         VarHandle::MatchKind::kExact);
}

template <typename VH>
static bool AccessModeWithConversionsMatch(Handle<VH> vh,
                                          VarHandle::AccessMode access_mode,
                                          const char* descriptor)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ObjPtr<MethodType> method_type = MethodTypeOf(descriptor);
  return AccessModeMatch(vh.Get(),
                         access_mode,
                         method_type,
                         VarHandle::MatchKind::kWithConversions);
}

template <typename VH>
static bool AccessModeNoMatch(Handle<VH> vh,
                              VarHandle::AccessMode access_mode,
                              const char* descriptor)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ObjPtr<MethodType> method_type = MethodTypeOf(descriptor);
  return AccessModeMatch(vh.Get(),
                         access_mode,
                         method_type,
                         VarHandle::MatchKind::kNone);
}

TEST_F(VarHandleTest, InstanceFieldVarHandle) {
  Thread * const self = Thread::Current();
  ScopedObjectAccess soa(self);

  ObjPtr<Object> i = BoxPrimitive(Primitive::kPrimInt, JValue::FromPrimitive<int32_t>(37));
  ArtField* value = mirror::Class::FindField(self, i->GetClass(), "value", "I");
  int32_t mask = AccessModesBitMask(VarHandle::AccessMode::kGet,
                                    VarHandle::AccessMode::kGetAndSet,
                                    VarHandle::AccessMode::kGetAndBitwiseXor);
  StackHandleScope<6> hs(self);
  Handle<mirror::FieldVarHandle> fvh(hs.NewHandle(CreateFieldVarHandle(self, value, mask)));
  EXPECT_FALSE(fvh.IsNull());
  EXPECT_EQ(value, fvh->GetField());

  // Check access modes
  EXPECT_TRUE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGet));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kSet));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetVolatile));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kSetVolatile));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAcquire));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kSetRelease));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetOpaque));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kSetOpaque));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kCompareAndSet));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kCompareAndExchange));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kCompareAndExchangeAcquire));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kCompareAndExchangeRelease));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kWeakCompareAndSetPlain));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kWeakCompareAndSet));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kWeakCompareAndSetAcquire));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kWeakCompareAndSetRelease));
  EXPECT_TRUE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndSet));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndSetAcquire));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndSetRelease));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndAdd));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndAddAcquire));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndAddRelease));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseOr));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseOrRelease));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseOrAcquire));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseAnd));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseAndRelease));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseAndAcquire));
  EXPECT_TRUE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseXor));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseXorRelease));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseXorAcquire));

  // Check compatibility - "Get" pattern
  {
    const VarHandle::AccessMode access_mode = VarHandle::AccessMode::kGet;
    EXPECT_TRUE(AccessModeExactMatch(fvh, access_mode, "(Ljava/lang/Integer;)I"));
    EXPECT_TRUE(AccessModeExactMatch(fvh, access_mode, "(Ljava/lang/Integer;)V"));
    EXPECT_TRUE(AccessModeWithConversionsMatch(fvh, access_mode, "(Ljava/lang/Integer;)D"));
    EXPECT_TRUE(AccessModeNoMatch(fvh, access_mode, "(Ljava/lang/Integer;)Z"));
    EXPECT_TRUE(AccessModeNoMatch(fvh, access_mode, "(Z)Z"));
  }

  // Check compatibility - "Set" pattern
  {
    const VarHandle::AccessMode access_mode = VarHandle::AccessMode::kSet;
    EXPECT_TRUE(AccessModeExactMatch(fvh, access_mode, "(Ljava/lang/Integer;I)V"));
    EXPECT_TRUE(AccessModeWithConversionsMatch(fvh, access_mode, "(Ljava/lang/Integer;S)V"));
    EXPECT_TRUE(AccessModeNoMatch(fvh, access_mode, "(Ljava/lang/Integer;)V"));
    EXPECT_TRUE(AccessModeNoMatch(fvh, access_mode, "(Ljava/lang/Integer;)Z"));
    EXPECT_TRUE(AccessModeNoMatch(fvh, access_mode, "(Z)V"));
  }

  // Check compatibility - "CompareAndSet" pattern
  {
    const VarHandle::AccessMode access_mode = VarHandle::AccessMode::kCompareAndSet;
    EXPECT_TRUE(AccessModeExactMatch(fvh, access_mode, "(Ljava/lang/Integer;II)Z"));
    EXPECT_TRUE(AccessModeExactMatch(fvh, access_mode, "(Ljava/lang/Integer;II)V"));
    EXPECT_TRUE(AccessModeWithConversionsMatch(fvh, access_mode, "(Ljava/lang/Integer;II)Ljava/lang/Boolean;"));
    EXPECT_TRUE(AccessModeWithConversionsMatch(fvh, access_mode, "(Ljava/lang/Integer;IB)V"));
    EXPECT_TRUE(AccessModeNoMatch(fvh, access_mode, "(Ljava/lang/Integer;II)I"));
    EXPECT_TRUE(AccessModeNoMatch(fvh, access_mode, "(Ljava/lang/Integer;)Z"));
    EXPECT_TRUE(AccessModeNoMatch(fvh, access_mode, "(Z)V"));
  }

  // Check compatibility - "CompareAndExchange" pattern
  {
    const VarHandle::AccessMode access_mode = VarHandle::AccessMode::kCompareAndExchange;
    EXPECT_TRUE(AccessModeExactMatch(fvh, access_mode, "(Ljava/lang/Integer;II)I"));
    EXPECT_TRUE(AccessModeExactMatch(fvh, access_mode, "(Ljava/lang/Integer;II)V"));
    EXPECT_TRUE(AccessModeWithConversionsMatch(fvh, access_mode, "(Ljava/lang/Integer;II)J"));
    EXPECT_TRUE(AccessModeWithConversionsMatch(fvh, access_mode, "(Ljava/lang/Integer;BS)F"));
    EXPECT_TRUE(AccessModeNoMatch(fvh, access_mode, "(Ljava/lang/Integer;I)Z"));
    EXPECT_TRUE(AccessModeNoMatch(fvh, access_mode, "(IIII)V"));
  }

  // Check compatibility - "GetAndUpdate" pattern
  {
    const VarHandle::AccessMode access_mode = VarHandle::AccessMode::kGetAndAdd;
    EXPECT_TRUE(AccessModeExactMatch(fvh, access_mode, "(Ljava/lang/Integer;I)I"));
    EXPECT_TRUE(AccessModeExactMatch(fvh, access_mode, "(Ljava/lang/Integer;I)V"));
    EXPECT_TRUE(AccessModeNoMatch(fvh, access_mode, "(Ljava/lang/Integer;I)Z"));
    EXPECT_TRUE(AccessModeNoMatch(fvh, access_mode, "(II)S"));
  }

  // Check synthesized method types match expected forms.
  {
    Handle<MethodType> get = hs.NewHandle(MethodTypeOf("(Ljava/lang/Integer;)I"));
    Handle<MethodType> set = hs.NewHandle(MethodTypeOf("(Ljava/lang/Integer;I)V"));
    Handle<MethodType> compareAndSet = hs.NewHandle(MethodTypeOf("(Ljava/lang/Integer;II)Z"));
    Handle<MethodType> compareAndExchange = hs.NewHandle(MethodTypeOf("(Ljava/lang/Integer;II)I"));
    Handle<MethodType> getAndUpdate = hs.NewHandle(MethodTypeOf("(Ljava/lang/Integer;I)I"));
    auto test_mode = [=](VarHandle::AccessMode access_mode, Handle<MethodType> method_type)
        REQUIRES_SHARED(Locks::mutator_lock_) {
      return fvh->GetMethodTypeForAccessMode(self, access_mode)->IsExactMatch(method_type.Get());
    };
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGet, get));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kSet, set));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetVolatile, get));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kSetVolatile, set));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAcquire, get));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kSetRelease, set));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetOpaque, get));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kSetOpaque, set));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kCompareAndSet, compareAndSet));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kCompareAndExchange, compareAndExchange));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kCompareAndExchangeAcquire, compareAndExchange));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kCompareAndExchangeRelease, compareAndExchange));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kWeakCompareAndSetPlain, compareAndSet));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kWeakCompareAndSet, compareAndSet));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kWeakCompareAndSetAcquire, compareAndSet));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kWeakCompareAndSetRelease, compareAndSet));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndSet, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndSetAcquire, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndSetRelease, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndAdd, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndAddAcquire, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndAddRelease, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndBitwiseOr, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndBitwiseOrRelease, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndBitwiseOrAcquire, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndBitwiseAnd, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndBitwiseAndRelease, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndBitwiseAndAcquire, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndBitwiseXor, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndBitwiseXorRelease, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndBitwiseXorAcquire, getAndUpdate));
  }
}

TEST_F(VarHandleTest, StaticFieldVarHandle) {
  Thread * const self = Thread::Current();
  ScopedObjectAccess soa(self);

  ObjPtr<Object> i = BoxPrimitive(Primitive::kPrimInt, JValue::FromPrimitive<int32_t>(37));
  ArtField* value = mirror::Class::FindField(self, i->GetClass(), "MIN_VALUE", "I");
  int32_t mask = AccessModesBitMask(VarHandle::AccessMode::kSet,
                                    VarHandle::AccessMode::kGetOpaque,
                                    VarHandle::AccessMode::kGetAndBitwiseAndRelease);
  StackHandleScope<6> hs(self);
  Handle<mirror::FieldVarHandle> fvh(hs.NewHandle(CreateFieldVarHandle(self, value, mask)));
  EXPECT_FALSE(fvh.IsNull());
  EXPECT_EQ(value, fvh->GetField());

  // Check access modes
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGet));
  EXPECT_TRUE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kSet));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetVolatile));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kSetVolatile));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAcquire));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kSetRelease));
  EXPECT_TRUE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetOpaque));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kSetOpaque));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kCompareAndSet));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kCompareAndExchange));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kCompareAndExchangeAcquire));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kCompareAndExchangeRelease));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kWeakCompareAndSetPlain));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kWeakCompareAndSet));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kWeakCompareAndSetAcquire));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kWeakCompareAndSetRelease));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndSet));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndSetAcquire));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndSetRelease));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndAdd));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndAddAcquire));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndAddRelease));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseOr));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseOrRelease));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseOrAcquire));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseAnd));
  EXPECT_TRUE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseAndRelease));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseAndAcquire));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseXor));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseXorRelease));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseXorAcquire));

  // Check compatibility - "Get" pattern
  {
    const VarHandle::AccessMode access_mode = VarHandle::AccessMode::kGet;
    EXPECT_TRUE(AccessModeExactMatch(fvh, access_mode, "()I"));
    EXPECT_TRUE(AccessModeExactMatch(fvh, access_mode, "()V"));
    EXPECT_TRUE(AccessModeNoMatch(fvh, access_mode, "()Z"));
    EXPECT_TRUE(AccessModeNoMatch(fvh, access_mode, "(Z)Z"));
  }

  // Check compatibility - "Set" pattern
  {
    const VarHandle::AccessMode access_mode = VarHandle::AccessMode::kSet;
    EXPECT_TRUE(AccessModeExactMatch(fvh, access_mode, "(I)V"));
    EXPECT_TRUE(AccessModeNoMatch(fvh, access_mode, "()V"));
    EXPECT_TRUE(AccessModeNoMatch(fvh, access_mode, "()Z"));
    EXPECT_TRUE(AccessModeNoMatch(fvh, access_mode, "(F)V"));
  }

  // Check compatibility - "CompareAndSet" pattern
  {
    const VarHandle::AccessMode access_mode = VarHandle::AccessMode::kCompareAndSet;
    EXPECT_TRUE(AccessModeExactMatch(fvh, access_mode, "(II)Z"));
    EXPECT_TRUE(AccessModeNoMatch(fvh, access_mode, "(II)Ljava/lang/String;"));
    EXPECT_TRUE(AccessModeNoMatch(fvh, access_mode, "()Z"));
    EXPECT_TRUE(AccessModeNoMatch(fvh, access_mode, "(Z)V"));
  }

  // Check compatibility - "CompareAndExchange" pattern
  {
    const VarHandle::AccessMode access_mode = VarHandle::AccessMode::kCompareAndExchange;
    EXPECT_TRUE(AccessModeExactMatch(fvh, access_mode, "(II)I"));
    EXPECT_TRUE(AccessModeExactMatch(fvh, access_mode, "(II)V"));
    EXPECT_TRUE(AccessModeNoMatch(fvh, access_mode, "(ID)I"));
    EXPECT_TRUE(AccessModeNoMatch(fvh, access_mode, "(II)S"));
    EXPECT_TRUE(AccessModeNoMatch(fvh, access_mode, "(IIJ)V"));
  }

  // Check compatibility - "GetAndUpdate" pattern
  {
    const VarHandle::AccessMode access_mode = VarHandle::AccessMode::kGetAndAdd;
    EXPECT_TRUE(AccessModeExactMatch(fvh, access_mode, "(I)I"));
    EXPECT_TRUE(AccessModeExactMatch(fvh, access_mode, "(I)V"));
    EXPECT_TRUE(AccessModeNoMatch(fvh, access_mode, "(I)Z"));
    EXPECT_TRUE(AccessModeNoMatch(fvh, access_mode, "(II)V"));
  }

  // Check synthesized method types match expected forms.
  {
    Handle<MethodType> get = hs.NewHandle(MethodTypeOf("()I"));
    Handle<MethodType> set = hs.NewHandle(MethodTypeOf("(I)V"));
    Handle<MethodType> compareAndSet = hs.NewHandle(MethodTypeOf("(II)Z"));
    Handle<MethodType> compareAndExchange = hs.NewHandle(MethodTypeOf("(II)I"));
    Handle<MethodType> getAndUpdate = hs.NewHandle(MethodTypeOf("(I)I"));
    auto test_mode = [=](VarHandle::AccessMode access_mode, Handle<MethodType> method_type)
        REQUIRES_SHARED(Locks::mutator_lock_) {
      return fvh->GetMethodTypeForAccessMode(self, access_mode)->IsExactMatch(method_type.Get());
    };
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGet, get));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kSet, set));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetVolatile, get));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kSetVolatile, set));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAcquire, get));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kSetRelease, set));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetOpaque, get));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kSetOpaque, set));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kCompareAndSet, compareAndSet));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kCompareAndExchange, compareAndExchange));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kCompareAndExchangeAcquire, compareAndExchange));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kCompareAndExchangeRelease, compareAndExchange));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kWeakCompareAndSetPlain, compareAndSet));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kWeakCompareAndSet, compareAndSet));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kWeakCompareAndSetAcquire, compareAndSet));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kWeakCompareAndSetRelease, compareAndSet));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndSet, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndSetAcquire, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndSetRelease, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndAdd, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndAddAcquire, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndAddRelease, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndBitwiseOr, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndBitwiseOrRelease, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndBitwiseOrAcquire, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndBitwiseAnd, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndBitwiseAndRelease, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndBitwiseAndAcquire, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndBitwiseXor, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndBitwiseXorRelease, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndBitwiseXorAcquire, getAndUpdate));
  }
}

TEST_F(VarHandleTest, ArrayElementVarHandle) {
  Thread * const self = Thread::Current();
  ScopedObjectAccess soa(self);
  StackHandleScope<7> hs(self);

  int32_t mask = AccessModesBitMask(VarHandle::AccessMode::kGet,
                                    VarHandle::AccessMode::kSet,
                                    VarHandle::AccessMode::kGetVolatile,
                                    VarHandle::AccessMode::kSetVolatile,
                                    VarHandle::AccessMode::kGetAcquire,
                                    VarHandle::AccessMode::kSetRelease,
                                    VarHandle::AccessMode::kGetOpaque,
                                    VarHandle::AccessMode::kSetOpaque,
                                    VarHandle::AccessMode::kCompareAndSet,
                                    VarHandle::AccessMode::kCompareAndExchange,
                                    VarHandle::AccessMode::kCompareAndExchangeAcquire,
                                    VarHandle::AccessMode::kCompareAndExchangeRelease,
                                    VarHandle::AccessMode::kWeakCompareAndSetPlain,
                                    VarHandle::AccessMode::kWeakCompareAndSet,
                                    VarHandle::AccessMode::kWeakCompareAndSetAcquire,
                                    VarHandle::AccessMode::kWeakCompareAndSetRelease,
                                    VarHandle::AccessMode::kGetAndSet,
                                    VarHandle::AccessMode::kGetAndSetAcquire,
                                    VarHandle::AccessMode::kGetAndSetRelease,
                                    VarHandle::AccessMode::kGetAndAdd,
                                    VarHandle::AccessMode::kGetAndAddAcquire,
                                    VarHandle::AccessMode::kGetAndAddRelease,
                                    VarHandle::AccessMode::kGetAndBitwiseOr,
                                    VarHandle::AccessMode::kGetAndBitwiseOrRelease,
                                    VarHandle::AccessMode::kGetAndBitwiseOrAcquire,
                                    VarHandle::AccessMode::kGetAndBitwiseAnd,
                                    VarHandle::AccessMode::kGetAndBitwiseAndRelease,
                                    VarHandle::AccessMode::kGetAndBitwiseAndAcquire,
                                    VarHandle::AccessMode::kGetAndBitwiseXor,
                                    VarHandle::AccessMode::kGetAndBitwiseXorRelease,
                                    VarHandle::AccessMode::kGetAndBitwiseXorAcquire);

  Handle<mirror::Class> string_array_class = hs.NewHandle(
      GetClassRoot<mirror::ObjectArray<mirror::String>>());
  Handle<mirror::ArrayElementVarHandle> vh(
      hs.NewHandle(CreateArrayElementVarHandle(self, string_array_class, mask)));
  EXPECT_FALSE(vh.IsNull());

  // Check access modes
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGet));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kSet));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetVolatile));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kSetVolatile));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAcquire));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kSetRelease));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetOpaque));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kSetOpaque));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kCompareAndSet));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kCompareAndExchange));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kCompareAndExchangeAcquire));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kCompareAndExchangeRelease));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kWeakCompareAndSetPlain));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kWeakCompareAndSet));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kWeakCompareAndSetAcquire));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kWeakCompareAndSetRelease));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndSet));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndSetAcquire));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndSetRelease));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndAdd));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndAddAcquire));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndAddRelease));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseOr));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseOrRelease));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseOrAcquire));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseAnd));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseAndRelease));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseAndAcquire));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseXor));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseXorRelease));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseXorAcquire));

  // Check compatibility - "Get" pattern
  {
    const VarHandle::AccessMode access_mode = VarHandle::AccessMode::kGet;
    EXPECT_TRUE(AccessModeExactMatch(vh, access_mode, "([Ljava/lang/String;I)Ljava/lang/String;"));
    EXPECT_TRUE(AccessModeExactMatch(vh, access_mode, "([Ljava/lang/String;I)V"));
    EXPECT_TRUE(AccessModeNoMatch(vh, access_mode, "([Ljava/lang/String;Ljava/lang/String;)Z"));
    EXPECT_TRUE(AccessModeNoMatch(vh, access_mode, "(Z)Z"));
  }

  // Check compatibility - "Set" pattern
  {
    const VarHandle::AccessMode access_mode = VarHandle::AccessMode::kSet;
    EXPECT_TRUE(AccessModeExactMatch(vh, access_mode, "([Ljava/lang/String;ILjava/lang/String;)V"));
    EXPECT_TRUE(AccessModeNoMatch(vh, access_mode, "([Ljava/lang/String;I)V"));
    EXPECT_TRUE(AccessModeNoMatch(vh, access_mode, "([Ljava/lang/String;I)Z"));
    EXPECT_TRUE(AccessModeNoMatch(vh, access_mode, "(Z)V"));
  }

  // Check compatibility - "CompareAndSet" pattern
  {
    const VarHandle::AccessMode access_mode = VarHandle::AccessMode::kCompareAndSet;
    EXPECT_TRUE(AccessModeExactMatch(vh, access_mode, "([Ljava/lang/String;ILjava/lang/String;Ljava/lang/String;)Z"));
    EXPECT_TRUE(AccessModeNoMatch(vh, access_mode, "([Ljava/lang/String;III)I"));
    EXPECT_TRUE(AccessModeNoMatch(vh, access_mode, "([Ljava/lang/String;I)Z"));
    EXPECT_TRUE(AccessModeNoMatch(vh, access_mode, "(Z)V"));
  }

  // Check compatibility - "CompareAndExchange" pattern
  {
    const VarHandle::AccessMode access_mode = VarHandle::AccessMode::kCompareAndExchange;
    EXPECT_TRUE(AccessModeExactMatch(vh, access_mode, "([Ljava/lang/String;ILjava/lang/String;Ljava/lang/String;)Ljava/lang/String;"));
    EXPECT_TRUE(AccessModeExactMatch(vh, access_mode, "([Ljava/lang/String;ILjava/lang/String;Ljava/lang/String;)V"));
    EXPECT_TRUE(AccessModeNoMatch(vh, access_mode, "([Ljava/lang/String;II)Z"));
    EXPECT_TRUE(AccessModeNoMatch(vh, access_mode, "(III)V"));
  }

  // Check compatibility - "GetAndUpdate" pattern
  {
    const VarHandle::AccessMode access_mode = VarHandle::AccessMode::kGetAndAdd;
    EXPECT_TRUE(AccessModeExactMatch(vh, access_mode, "([Ljava/lang/String;ILjava/lang/String;)Ljava/lang/String;"));
    EXPECT_TRUE(AccessModeExactMatch(vh, access_mode, "([Ljava/lang/String;ILjava/lang/String;)V"));
    EXPECT_TRUE(AccessModeNoMatch(vh, access_mode, "([Ljava/lang/String;ILjava/lang/String;)Z"));
    EXPECT_TRUE(AccessModeNoMatch(vh, access_mode, "(II)V"));
  }

  // Check synthesized method types match expected forms.
  {
    Handle<MethodType> get = hs.NewHandle(MethodTypeOf("([Ljava/lang/String;I)Ljava/lang/String;"));
    Handle<MethodType> set =
        hs.NewHandle(MethodTypeOf("([Ljava/lang/String;ILjava/lang/String;)V"));
    Handle<MethodType> compareAndSet =
        hs.NewHandle(MethodTypeOf("([Ljava/lang/String;ILjava/lang/String;Ljava/lang/String;)Z"));
    Handle<MethodType> compareAndExchange = hs.NewHandle(MethodTypeOf(
        "([Ljava/lang/String;ILjava/lang/String;Ljava/lang/String;)Ljava/lang/String;"));
    Handle<MethodType> getAndUpdate =
        hs.NewHandle(MethodTypeOf("([Ljava/lang/String;ILjava/lang/String;)Ljava/lang/String;"));
    auto test_mode = [=](VarHandle::AccessMode access_mode, Handle<MethodType> method_type)
        REQUIRES_SHARED(Locks::mutator_lock_) {
      return vh->GetMethodTypeForAccessMode(self, access_mode)->IsExactMatch(method_type.Get());
    };
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGet, get));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kSet, set));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetVolatile, get));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kSetVolatile, set));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAcquire, get));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kSetRelease, set));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetOpaque, get));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kSetOpaque, set));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kCompareAndSet, compareAndSet));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kCompareAndExchange, compareAndExchange));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kCompareAndExchangeAcquire, compareAndExchange));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kCompareAndExchangeRelease, compareAndExchange));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kWeakCompareAndSetPlain, compareAndSet));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kWeakCompareAndSet, compareAndSet));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kWeakCompareAndSetAcquire, compareAndSet));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kWeakCompareAndSetRelease, compareAndSet));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndSet, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndSetAcquire, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndSetRelease, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndAdd, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndAddAcquire, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndAddRelease, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndBitwiseOr, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndBitwiseOrRelease, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndBitwiseOrAcquire, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndBitwiseAnd, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndBitwiseAndRelease, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndBitwiseAndAcquire, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndBitwiseXor, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndBitwiseXorRelease, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndBitwiseXorAcquire, getAndUpdate));
  }
}

TEST_F(VarHandleTest, ByteArrayViewVarHandle) {
  Thread * const self = Thread::Current();
  ScopedObjectAccess soa(self);
  StackHandleScope<7> hs(self);

  int32_t mask = AccessModesBitMask(VarHandle::AccessMode::kGet,
                                    VarHandle::AccessMode::kGetVolatile,
                                    VarHandle::AccessMode::kGetAcquire,
                                    VarHandle::AccessMode::kGetOpaque,
                                    VarHandle::AccessMode::kCompareAndSet,
                                    VarHandle::AccessMode::kCompareAndExchangeAcquire,
                                    VarHandle::AccessMode::kWeakCompareAndSetPlain,
                                    VarHandle::AccessMode::kWeakCompareAndSetAcquire,
                                    VarHandle::AccessMode::kGetAndSet,
                                    VarHandle::AccessMode::kGetAndSetRelease,
                                    VarHandle::AccessMode::kGetAndAddAcquire,
                                    VarHandle::AccessMode::kGetAndBitwiseOr,
                                    VarHandle::AccessMode::kGetAndBitwiseOrAcquire,
                                    VarHandle::AccessMode::kGetAndBitwiseAndRelease,
                                    VarHandle::AccessMode::kGetAndBitwiseXor,
                                    VarHandle::AccessMode::kGetAndBitwiseXorAcquire);

  Handle<Class> char_array_class(hs.NewHandle(GetClassRoot<mirror::CharArray>()));
  const bool native_byte_order = true;
  Handle<mirror::ByteArrayViewVarHandle> vh(
      hs.NewHandle(CreateByteArrayViewVarHandle(self, char_array_class, native_byte_order, mask)));
  EXPECT_FALSE(vh.IsNull());
  EXPECT_EQ(native_byte_order, vh->GetNativeByteOrder());

  // Check access modes
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGet));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kSet));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetVolatile));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kSetVolatile));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAcquire));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kSetRelease));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetOpaque));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kSetOpaque));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kCompareAndSet));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kCompareAndExchange));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kCompareAndExchangeAcquire));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kCompareAndExchangeRelease));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kWeakCompareAndSetPlain));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kWeakCompareAndSet));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kWeakCompareAndSetAcquire));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kWeakCompareAndSetRelease));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndSet));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndSetAcquire));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndSetRelease));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndAdd));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndAddAcquire));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndAddRelease));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseOr));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseOrRelease));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseOrAcquire));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseAnd));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseAndRelease));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseAndAcquire));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseXor));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseXorRelease));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseXorAcquire));

  // Check compatibility - "Get" pattern
  {
    const VarHandle::AccessMode access_mode = VarHandle::AccessMode::kGet;
    EXPECT_TRUE(AccessModeExactMatch(vh, access_mode, "([BI)C"));
    EXPECT_TRUE(AccessModeExactMatch(vh, access_mode, "([BI)V"));
    EXPECT_TRUE(AccessModeNoMatch(vh, access_mode, "([BC)Z"));
    EXPECT_TRUE(AccessModeNoMatch(vh, access_mode, "(Z)Z"));
  }

  // Check compatibility - "Set" pattern
  {
    const VarHandle::AccessMode access_mode = VarHandle::AccessMode::kSet;
    EXPECT_TRUE(AccessModeExactMatch(vh, access_mode, "([BIC)V"));
    EXPECT_TRUE(AccessModeNoMatch(vh, access_mode, "([BI)V"));
    EXPECT_TRUE(AccessModeNoMatch(vh, access_mode, "([BI)Z"));
    EXPECT_TRUE(AccessModeNoMatch(vh, access_mode, "(Z)V"));
  }

  // Check compatibility - "CompareAndSet" pattern
  {
    const VarHandle::AccessMode access_mode = VarHandle::AccessMode::kCompareAndSet;
    EXPECT_TRUE(AccessModeExactMatch(vh, access_mode, "([BICC)Z"));
    EXPECT_TRUE(AccessModeNoMatch(vh, access_mode, "([BIII)I"));
    EXPECT_TRUE(AccessModeNoMatch(vh, access_mode, "([BI)Z"));
    EXPECT_TRUE(AccessModeNoMatch(vh, access_mode, "(Z)V"));
  }

  // Check compatibility - "CompareAndExchange" pattern
  {
    const VarHandle::AccessMode access_mode = VarHandle::AccessMode::kCompareAndExchange;
    EXPECT_TRUE(AccessModeExactMatch(vh, access_mode, "([BICC)C"));
    EXPECT_TRUE(AccessModeExactMatch(vh, access_mode, "([BICC)V"));
    EXPECT_TRUE(AccessModeNoMatch(vh, access_mode, "([BII)Z"));
    EXPECT_TRUE(AccessModeNoMatch(vh, access_mode, "(III)V"));
  }

  // Check compatibility - "GetAndUpdate" pattern
  {
    const VarHandle::AccessMode access_mode = VarHandle::AccessMode::kGetAndAdd;
    EXPECT_TRUE(AccessModeExactMatch(vh, access_mode, "([BIC)C"));
    EXPECT_TRUE(AccessModeExactMatch(vh, access_mode, "([BIC)V"));
    EXPECT_TRUE(AccessModeNoMatch(vh, access_mode, "([BIC)Z"));
    EXPECT_TRUE(AccessModeNoMatch(vh, access_mode, "(II)V"));
  }

  // Check synthesized method types match expected forms.
  {
    Handle<MethodType> get = hs.NewHandle(MethodTypeOf("([BI)C"));
    Handle<MethodType> set = hs.NewHandle(MethodTypeOf("([BIC)V"));
    Handle<MethodType> compareAndSet = hs.NewHandle(MethodTypeOf("([BICC)Z"));
    Handle<MethodType> compareAndExchange = hs.NewHandle(MethodTypeOf("([BICC)C"));
    Handle<MethodType> getAndUpdate = hs.NewHandle(MethodTypeOf("([BIC)C"));
    auto test_mode = [=](VarHandle::AccessMode access_mode, Handle<MethodType> method_type)
        REQUIRES_SHARED(Locks::mutator_lock_) {
      return vh->GetMethodTypeForAccessMode(self, access_mode)->IsExactMatch(method_type.Get());
    };
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGet, get));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kSet, set));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetVolatile, get));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kSetVolatile, set));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAcquire, get));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kSetRelease, set));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetOpaque, get));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kSetOpaque, set));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kCompareAndSet, compareAndSet));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kCompareAndExchange, compareAndExchange));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kCompareAndExchangeAcquire, compareAndExchange));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kCompareAndExchangeRelease, compareAndExchange));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kWeakCompareAndSetPlain, compareAndSet));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kWeakCompareAndSet, compareAndSet));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kWeakCompareAndSetAcquire, compareAndSet));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kWeakCompareAndSetRelease, compareAndSet));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndSet, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndSetAcquire, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndSetRelease, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndAdd, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndAddAcquire, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndAddRelease, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndBitwiseOr, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndBitwiseOrRelease, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndBitwiseOrAcquire, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndBitwiseAnd, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndBitwiseAndRelease, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndBitwiseAndAcquire, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndBitwiseXor, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndBitwiseXorRelease, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndBitwiseXorAcquire, getAndUpdate));
  }
}

TEST_F(VarHandleTest, ByteBufferViewVarHandle) {
  Thread * const self = Thread::Current();
  ScopedObjectAccess soa(self);
  StackHandleScope<7> hs(self);

  int32_t mask = AccessModesBitMask(VarHandle::AccessMode::kGet,
                                    VarHandle::AccessMode::kGetVolatile,
                                    VarHandle::AccessMode::kGetAcquire,
                                    VarHandle::AccessMode::kGetOpaque,
                                    VarHandle::AccessMode::kCompareAndSet,
                                    VarHandle::AccessMode::kCompareAndExchangeAcquire,
                                    VarHandle::AccessMode::kWeakCompareAndSetPlain,
                                    VarHandle::AccessMode::kWeakCompareAndSetAcquire,
                                    VarHandle::AccessMode::kGetAndSet,
                                    VarHandle::AccessMode::kGetAndSetRelease,
                                    VarHandle::AccessMode::kGetAndAddAcquire,
                                    VarHandle::AccessMode::kGetAndBitwiseOr,
                                    VarHandle::AccessMode::kGetAndBitwiseOrAcquire,
                                    VarHandle::AccessMode::kGetAndBitwiseAndRelease,
                                    VarHandle::AccessMode::kGetAndBitwiseXor,
                                    VarHandle::AccessMode::kGetAndBitwiseXorAcquire);

  Handle<Class> double_array_class(hs.NewHandle(GetClassRoot<mirror::DoubleArray>()));
  const bool native_byte_order = false;
  Handle<mirror::ByteBufferViewVarHandle> vh(hs.NewHandle(
      CreateByteBufferViewVarHandle(self, double_array_class, native_byte_order, mask)));
  EXPECT_FALSE(vh.IsNull());
  EXPECT_EQ(native_byte_order, vh->GetNativeByteOrder());

  // Check access modes
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGet));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kSet));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetVolatile));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kSetVolatile));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAcquire));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kSetRelease));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetOpaque));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kSetOpaque));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kCompareAndSet));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kCompareAndExchange));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kCompareAndExchangeAcquire));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kCompareAndExchangeRelease));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kWeakCompareAndSetPlain));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kWeakCompareAndSet));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kWeakCompareAndSetAcquire));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kWeakCompareAndSetRelease));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndSet));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndSetAcquire));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndSetRelease));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndAdd));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndAddAcquire));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndAddRelease));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseOr));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseOrRelease));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseOrAcquire));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseAnd));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseAndRelease));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseAndAcquire));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseXor));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseXorRelease));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseXorAcquire));

  // Check compatibility - "Get" pattern
  {
    const VarHandle::AccessMode access_mode = VarHandle::AccessMode::kGet;
    EXPECT_TRUE(AccessModeExactMatch(vh, access_mode, "(Ljava/nio/ByteBuffer;I)D"));
    EXPECT_TRUE(AccessModeExactMatch(vh, access_mode, "(Ljava/nio/ByteBuffer;I)V"));
    EXPECT_TRUE(AccessModeNoMatch(vh, access_mode, "(Ljava/nio/ByteBuffer;D)Z"));
    EXPECT_TRUE(AccessModeNoMatch(vh, access_mode, "(Z)Z"));
  }

  // Check compatibility - "Set" pattern
  {
    const VarHandle::AccessMode access_mode = VarHandle::AccessMode::kSet;
    EXPECT_TRUE(AccessModeExactMatch(vh, access_mode, "(Ljava/nio/ByteBuffer;ID)V"));
    EXPECT_TRUE(AccessModeNoMatch(vh, access_mode, "(Ljava/nio/ByteBuffer;I)V"));
    EXPECT_TRUE(AccessModeNoMatch(vh, access_mode, "(Ljava/nio/ByteBuffer;I)Z"));
    EXPECT_TRUE(AccessModeNoMatch(vh, access_mode, "(Z)V"));
  }

  // Check compatibility - "CompareAndSet" pattern
  {
    const VarHandle::AccessMode access_mode = VarHandle::AccessMode::kCompareAndSet;
    EXPECT_TRUE(AccessModeExactMatch(vh, access_mode, "(Ljava/nio/ByteBuffer;IDD)Z"));
    EXPECT_TRUE(AccessModeNoMatch(vh, access_mode, "(Ljava/nio/ByteBuffer;IDI)D"));
    EXPECT_TRUE(AccessModeNoMatch(vh, access_mode, "(Ljava/nio/ByteBuffer;I)Z"));
    EXPECT_TRUE(AccessModeNoMatch(vh, access_mode, "(Z)V"));
  }

  // Check compatibility - "CompareAndExchange" pattern
  {
    const VarHandle::AccessMode access_mode = VarHandle::AccessMode::kCompareAndExchange;
    EXPECT_TRUE(AccessModeExactMatch(vh, access_mode, "(Ljava/nio/ByteBuffer;IDD)D"));
    EXPECT_TRUE(AccessModeExactMatch(vh, access_mode, "(Ljava/nio/ByteBuffer;IDD)V"));
    EXPECT_TRUE(AccessModeNoMatch(vh, access_mode, "(Ljava/nio/ByteBuffer;II)Z"));
    EXPECT_TRUE(AccessModeNoMatch(vh, access_mode, "(III)V"));
  }

  // Check compatibility - "GetAndUpdate" pattern
  {
    const VarHandle::AccessMode access_mode = VarHandle::AccessMode::kGetAndAdd;
    EXPECT_TRUE(AccessModeExactMatch(vh, access_mode, "(Ljava/nio/ByteBuffer;ID)D"));
    EXPECT_TRUE(AccessModeExactMatch(vh, access_mode, "(Ljava/nio/ByteBuffer;ID)V"));
    EXPECT_TRUE(AccessModeNoMatch(vh, access_mode, "(Ljava/nio/ByteBuffer;ID)Z"));
    EXPECT_TRUE(AccessModeNoMatch(vh, access_mode, "(II)V"));
  }

  // Check synthesized method types match expected forms.
  {
    Handle<MethodType> get = hs.NewHandle(MethodTypeOf("(Ljava/nio/ByteBuffer;I)D"));
    Handle<MethodType> set = hs.NewHandle(MethodTypeOf("(Ljava/nio/ByteBuffer;ID)V"));
    Handle<MethodType> compareAndSet = hs.NewHandle(MethodTypeOf("(Ljava/nio/ByteBuffer;IDD)Z"));
    Handle<MethodType> compareAndExchange =
        hs.NewHandle(MethodTypeOf("(Ljava/nio/ByteBuffer;IDD)D"));
    Handle<MethodType> getAndUpdate = hs.NewHandle(MethodTypeOf("(Ljava/nio/ByteBuffer;ID)D"));
    auto test_mode = [=](VarHandle::AccessMode access_mode, Handle<MethodType> method_type)
        REQUIRES_SHARED(Locks::mutator_lock_) {
      return vh->GetMethodTypeForAccessMode(self, access_mode)->IsExactMatch(method_type.Get());
    };
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGet, get));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kSet, set));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetVolatile, get));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kSetVolatile, set));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAcquire, get));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kSetRelease, set));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetOpaque, get));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kSetOpaque, set));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kCompareAndSet, compareAndSet));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kCompareAndExchange, compareAndExchange));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kCompareAndExchangeAcquire, compareAndExchange));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kCompareAndExchangeRelease, compareAndExchange));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kWeakCompareAndSetPlain, compareAndSet));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kWeakCompareAndSet, compareAndSet));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kWeakCompareAndSetAcquire, compareAndSet));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kWeakCompareAndSetRelease, compareAndSet));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndSet, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndSetAcquire, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndSetRelease, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndAdd, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndAddAcquire, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndAddRelease, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndBitwiseOr, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndBitwiseOrRelease, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndBitwiseOrAcquire, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndBitwiseAnd, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndBitwiseAndRelease, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndBitwiseAndAcquire, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndBitwiseXor, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndBitwiseXorRelease, getAndUpdate));
    EXPECT_TRUE(test_mode(VarHandle::AccessMode::kGetAndBitwiseXorAcquire, getAndUpdate));
  }
}

TEST_F(VarHandleTest, GetMethodTypeForAccessMode) {
  VarHandle::AccessMode access_mode;

  // Invalid access mode names
  EXPECT_FALSE(VarHandle::GetAccessModeByMethodName(nullptr, &access_mode));
  EXPECT_FALSE(VarHandle::GetAccessModeByMethodName("", &access_mode));
  EXPECT_FALSE(VarHandle::GetAccessModeByMethodName("CompareAndExchange", &access_mode));
  EXPECT_FALSE(VarHandle::GetAccessModeByMethodName("compareAndExchangX", &access_mode));

  // Valid access mode names
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("compareAndExchange", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kCompareAndExchange, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("compareAndExchangeAcquire", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kCompareAndExchangeAcquire, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("compareAndExchangeRelease", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kCompareAndExchangeRelease, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("compareAndSet", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kCompareAndSet, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("get", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kGet, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("getAcquire", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kGetAcquire, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("getAndAdd", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kGetAndAdd, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("getAndAddAcquire", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kGetAndAddAcquire, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("getAndAddRelease", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kGetAndAddRelease, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("getAndBitwiseAnd", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kGetAndBitwiseAnd, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("getAndBitwiseAndAcquire", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kGetAndBitwiseAndAcquire, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("getAndBitwiseAndRelease", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kGetAndBitwiseAndRelease, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("getAndBitwiseOr", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kGetAndBitwiseOr, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("getAndBitwiseOrAcquire", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kGetAndBitwiseOrAcquire, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("getAndBitwiseOrRelease", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kGetAndBitwiseOrRelease, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("getAndBitwiseXor", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kGetAndBitwiseXor, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("getAndBitwiseXorAcquire", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kGetAndBitwiseXorAcquire, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("getAndBitwiseXorRelease", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kGetAndBitwiseXorRelease, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("getAndSet", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kGetAndSet, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("getAndSetAcquire", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kGetAndSetAcquire, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("getAndSetRelease", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kGetAndSetRelease, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("getOpaque", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kGetOpaque, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("getVolatile", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kGetVolatile, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("set", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kSet, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("setOpaque", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kSetOpaque, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("setRelease", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kSetRelease, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("setVolatile", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kSetVolatile, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("weakCompareAndSet", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kWeakCompareAndSet, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("weakCompareAndSetAcquire", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kWeakCompareAndSetAcquire, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("weakCompareAndSetPlain", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kWeakCompareAndSetPlain, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("weakCompareAndSetRelease", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kWeakCompareAndSetRelease, access_mode);
}

}  // namespace mirror
}  // namespace art
