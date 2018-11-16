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

#include "base/sdk_version.h"
#include "common_runtime_test.h"
#include "jni/jni_internal.h"
#include "proxy_test.h"

namespace art {

using hiddenapi::detail::MemberSignature;
using hiddenapi::detail::ShouldDenyAccessToMemberImpl;

class HiddenApiTest : public CommonRuntimeTest {
 protected:
  void SetUp() override {
    // Do the normal setup.
    CommonRuntimeTest::SetUp();
    self_ = Thread::Current();
    self_->TransitionFromSuspendedToRunnable();
    jclass_loader_ = LoadDex("HiddenApiSignatures");
    bool started = runtime_->Start();
    CHECK(started);

    class1_field1_ = getArtField("mypackage/packagea/Class1", "field1", "I");
    class1_field12_ = getArtField("mypackage/packagea/Class1", "field12", "I");
    class1_init_ = getArtMethod("mypackage/packagea/Class1", "<init>", "()V");
    class1_method1_ = getArtMethod("mypackage/packagea/Class1", "method1", "()V");
    class1_method1_i_ = getArtMethod("mypackage/packagea/Class1", "method1", "(I)V");
    class1_method12_ = getArtMethod("mypackage/packagea/Class1", "method12", "()V");
    class12_field1_ = getArtField("mypackage/packagea/Class12", "field1", "I");
    class12_method1_ = getArtMethod("mypackage/packagea/Class12", "method1", "()V");
    class2_field1_ = getArtField("mypackage/packagea/Class2", "field1", "I");
    class2_method1_ = getArtMethod("mypackage/packagea/Class2", "method1", "()V");
    class2_method1_i_ = getArtMethod("mypackage/packagea/Class2", "method1", "(I)V");
    class3_field1_ = getArtField("mypackage/packageb/Class3", "field1", "I");
    class3_method1_ = getArtMethod("mypackage/packageb/Class3", "method1", "()V");
    class3_method1_i_ = getArtMethod("mypackage/packageb/Class3", "method1", "(I)V");
  }

  ArtMethod* getArtMethod(const char* class_name, const char* name, const char* signature) {
    JNIEnv* env = Thread::Current()->GetJniEnv();
    jclass klass = env->FindClass(class_name);
    jmethodID method_id = env->GetMethodID(klass, name, signature);
    ArtMethod* art_method = jni::DecodeArtMethod(method_id);
    return art_method;
  }

  ArtField* getArtField(const char* class_name, const char* name, const char* signature) {
    JNIEnv* env = Thread::Current()->GetJniEnv();
    jclass klass = env->FindClass(class_name);
    jfieldID field_id = env->GetFieldID(klass, name, signature);
    ArtField* art_field = jni::DecodeArtField(field_id);
    return art_field;
  }

  bool ShouldDenyAccess(hiddenapi::ApiList list) REQUIRES_SHARED(Locks::mutator_lock_) {
    // Choose parameters such that there are no side effects (AccessMethod::kNone)
    // and that the member is not on the exemptions list (here we choose one which
    // is not even in boot class path).
    return ShouldDenyAccessToMemberImpl(/* member= */ class1_field1_,
                                        list,
                                        /* access_method= */ hiddenapi::AccessMethod::kNone);
  }

 protected:
  Thread* self_;
  jobject jclass_loader_;
  ArtField* class1_field1_;
  ArtField* class1_field12_;
  ArtMethod* class1_init_;
  ArtMethod* class1_method1_;
  ArtMethod* class1_method1_i_;
  ArtMethod* class1_method12_;
  ArtField* class12_field1_;
  ArtMethod* class12_method1_;
  ArtField* class2_field1_;
  ArtMethod* class2_method1_;
  ArtMethod* class2_method1_i_;
  ArtField* class3_field1_;
  ArtMethod* class3_method1_;
  ArtMethod* class3_method1_i_;
};

TEST_F(HiddenApiTest, CheckGetActionFromRuntimeFlags) {
  ScopedObjectAccess soa(self_);

  runtime_->SetHiddenApiEnforcementPolicy(hiddenapi::EnforcementPolicy::kDisabled);
  ASSERT_EQ(ShouldDenyAccess(hiddenapi::ApiList::kWhitelist), false);
  ASSERT_EQ(ShouldDenyAccess(hiddenapi::ApiList::kLightGreylist), false);
  ASSERT_EQ(ShouldDenyAccess(hiddenapi::ApiList::kDarkGreylist), false);
  ASSERT_EQ(ShouldDenyAccess(hiddenapi::ApiList::kBlacklist), false);

  runtime_->SetHiddenApiEnforcementPolicy(hiddenapi::EnforcementPolicy::kJustWarn);
  ASSERT_EQ(ShouldDenyAccess(hiddenapi::ApiList::kWhitelist), false);
  ASSERT_EQ(ShouldDenyAccess(hiddenapi::ApiList::kLightGreylist), false);
  ASSERT_EQ(ShouldDenyAccess(hiddenapi::ApiList::kDarkGreylist), false);
  ASSERT_EQ(ShouldDenyAccess(hiddenapi::ApiList::kBlacklist), false);

  runtime_->SetHiddenApiEnforcementPolicy(hiddenapi::EnforcementPolicy::kEnabled);
  runtime_->SetTargetSdkVersion(static_cast<uint32_t>(SdkVersion::kO_MR1));
  ASSERT_EQ(ShouldDenyAccess(hiddenapi::ApiList::kWhitelist), false);
  ASSERT_EQ(ShouldDenyAccess(hiddenapi::ApiList::kLightGreylist), false);
  ASSERT_EQ(ShouldDenyAccess(hiddenapi::ApiList::kDarkGreylist), false);
  ASSERT_EQ(ShouldDenyAccess(hiddenapi::ApiList::kBlacklist), true);

  runtime_->SetHiddenApiEnforcementPolicy(hiddenapi::EnforcementPolicy::kEnabled);
  runtime_->SetTargetSdkVersion(static_cast<uint32_t>(SdkVersion::kP));
  ASSERT_EQ(ShouldDenyAccess(hiddenapi::ApiList::kWhitelist), false);
  ASSERT_EQ(ShouldDenyAccess(hiddenapi::ApiList::kLightGreylist), false);
  ASSERT_EQ(ShouldDenyAccess(hiddenapi::ApiList::kDarkGreylist), true);
  ASSERT_EQ(ShouldDenyAccess(hiddenapi::ApiList::kBlacklist), true);
}

TEST_F(HiddenApiTest, CheckMembersRead) {
  ASSERT_NE(nullptr, class1_field1_);
  ASSERT_NE(nullptr, class1_field12_);
  ASSERT_NE(nullptr, class1_init_);
  ASSERT_NE(nullptr, class1_method1_);
  ASSERT_NE(nullptr, class1_method1_i_);
  ASSERT_NE(nullptr, class1_method12_);
  ASSERT_NE(nullptr, class12_field1_);
  ASSERT_NE(nullptr, class12_method1_);
  ASSERT_NE(nullptr, class2_field1_);
  ASSERT_NE(nullptr, class2_method1_);
  ASSERT_NE(nullptr, class2_method1_i_);
  ASSERT_NE(nullptr, class3_field1_);
  ASSERT_NE(nullptr, class3_method1_);
  ASSERT_NE(nullptr, class3_method1_i_);
}

TEST_F(HiddenApiTest, CheckEverythingMatchesL) {
  ScopedObjectAccess soa(self_);
  std::string prefix("L");
  ASSERT_TRUE(MemberSignature(class1_field1_).DoesPrefixMatch(prefix));
  ASSERT_TRUE(MemberSignature(class1_field12_).DoesPrefixMatch(prefix));
  ASSERT_TRUE(MemberSignature(class1_init_).DoesPrefixMatch(prefix));
  ASSERT_TRUE(MemberSignature(class1_method1_).DoesPrefixMatch(prefix));
  ASSERT_TRUE(MemberSignature(class1_method1_i_).DoesPrefixMatch(prefix));
  ASSERT_TRUE(MemberSignature(class12_field1_).DoesPrefixMatch(prefix));
  ASSERT_TRUE(MemberSignature(class12_method1_).DoesPrefixMatch(prefix));
  ASSERT_TRUE(MemberSignature(class1_method12_).DoesPrefixMatch(prefix));
  ASSERT_TRUE(MemberSignature(class2_field1_).DoesPrefixMatch(prefix));
  ASSERT_TRUE(MemberSignature(class2_method1_).DoesPrefixMatch(prefix));
  ASSERT_TRUE(MemberSignature(class2_method1_i_).DoesPrefixMatch(prefix));
  ASSERT_TRUE(MemberSignature(class3_field1_).DoesPrefixMatch(prefix));
  ASSERT_TRUE(MemberSignature(class3_method1_).DoesPrefixMatch(prefix));
  ASSERT_TRUE(MemberSignature(class3_method1_i_).DoesPrefixMatch(prefix));
}

TEST_F(HiddenApiTest, CheckPackageMatch) {
  ScopedObjectAccess soa(self_);
  std::string prefix("Lmypackage/packagea/");
  ASSERT_TRUE(MemberSignature(class1_field1_).DoesPrefixMatch(prefix));
  ASSERT_TRUE(MemberSignature(class1_field12_).DoesPrefixMatch(prefix));
  ASSERT_TRUE(MemberSignature(class1_init_).DoesPrefixMatch(prefix));
  ASSERT_TRUE(MemberSignature(class1_method1_).DoesPrefixMatch(prefix));
  ASSERT_TRUE(MemberSignature(class1_method1_i_).DoesPrefixMatch(prefix));
  ASSERT_TRUE(MemberSignature(class1_method12_).DoesPrefixMatch(prefix));
  ASSERT_TRUE(MemberSignature(class12_field1_).DoesPrefixMatch(prefix));
  ASSERT_TRUE(MemberSignature(class12_method1_).DoesPrefixMatch(prefix));
  ASSERT_TRUE(MemberSignature(class2_field1_).DoesPrefixMatch(prefix));
  ASSERT_TRUE(MemberSignature(class2_method1_).DoesPrefixMatch(prefix));
  ASSERT_TRUE(MemberSignature(class2_method1_i_).DoesPrefixMatch(prefix));
  ASSERT_FALSE(MemberSignature(class3_field1_).DoesPrefixMatch(prefix));
  ASSERT_FALSE(MemberSignature(class3_method1_).DoesPrefixMatch(prefix));
  ASSERT_FALSE(MemberSignature(class3_method1_i_).DoesPrefixMatch(prefix));
}

TEST_F(HiddenApiTest, CheckClassMatch) {
  ScopedObjectAccess soa(self_);
  std::string prefix("Lmypackage/packagea/Class1");
  ASSERT_TRUE(MemberSignature(class1_field1_).DoesPrefixMatch(prefix));
  ASSERT_TRUE(MemberSignature(class1_field12_).DoesPrefixMatch(prefix));
  ASSERT_TRUE(MemberSignature(class1_init_).DoesPrefixMatch(prefix));
  ASSERT_TRUE(MemberSignature(class1_method1_).DoesPrefixMatch(prefix));
  ASSERT_TRUE(MemberSignature(class1_method1_i_).DoesPrefixMatch(prefix));
  ASSERT_TRUE(MemberSignature(class1_method12_).DoesPrefixMatch(prefix));
  ASSERT_TRUE(MemberSignature(class12_field1_).DoesPrefixMatch(prefix));
  ASSERT_TRUE(MemberSignature(class12_method1_).DoesPrefixMatch(prefix));
  ASSERT_FALSE(MemberSignature(class2_field1_).DoesPrefixMatch(prefix));
  ASSERT_FALSE(MemberSignature(class2_method1_).DoesPrefixMatch(prefix));
  ASSERT_FALSE(MemberSignature(class2_method1_i_).DoesPrefixMatch(prefix));
}

TEST_F(HiddenApiTest, CheckClassExactMatch) {
  ScopedObjectAccess soa(self_);
  std::string prefix("Lmypackage/packagea/Class1;");
  ASSERT_TRUE(MemberSignature(class1_field1_).DoesPrefixMatch(prefix));
  ASSERT_TRUE(MemberSignature(class1_field12_).DoesPrefixMatch(prefix));
  ASSERT_TRUE(MemberSignature(class1_init_).DoesPrefixMatch(prefix));
  ASSERT_TRUE(MemberSignature(class1_method1_).DoesPrefixMatch(prefix));
  ASSERT_TRUE(MemberSignature(class1_method1_i_).DoesPrefixMatch(prefix));
  ASSERT_FALSE(MemberSignature(class12_field1_).DoesPrefixMatch(prefix));
  ASSERT_FALSE(MemberSignature(class12_method1_).DoesPrefixMatch(prefix));
  ASSERT_FALSE(MemberSignature(class2_field1_).DoesPrefixMatch(prefix));
  ASSERT_FALSE(MemberSignature(class2_method1_).DoesPrefixMatch(prefix));
  ASSERT_FALSE(MemberSignature(class2_method1_i_).DoesPrefixMatch(prefix));
}

TEST_F(HiddenApiTest, CheckMethodMatch) {
  ScopedObjectAccess soa(self_);
  std::string prefix("Lmypackage/packagea/Class1;->method1");
  ASSERT_FALSE(MemberSignature(class1_field1_).DoesPrefixMatch(prefix));
  ASSERT_FALSE(MemberSignature(class1_field12_).DoesPrefixMatch(prefix));
  ASSERT_FALSE(MemberSignature(class1_init_).DoesPrefixMatch(prefix));
  ASSERT_TRUE(MemberSignature(class1_method1_).DoesPrefixMatch(prefix));
  ASSERT_TRUE(MemberSignature(class1_method1_i_).DoesPrefixMatch(prefix));
  ASSERT_TRUE(MemberSignature(class1_method12_).DoesPrefixMatch(prefix));
  ASSERT_FALSE(MemberSignature(class12_field1_).DoesPrefixMatch(prefix));
  ASSERT_FALSE(MemberSignature(class12_method1_).DoesPrefixMatch(prefix));
}

TEST_F(HiddenApiTest, CheckMethodExactMatch) {
  ScopedObjectAccess soa(self_);
  std::string prefix("Lmypackage/packagea/Class1;->method1(");
  ASSERT_FALSE(MemberSignature(class1_field1_).DoesPrefixMatch(prefix));
  ASSERT_FALSE(MemberSignature(class1_field12_).DoesPrefixMatch(prefix));
  ASSERT_FALSE(MemberSignature(class1_init_).DoesPrefixMatch(prefix));
  ASSERT_TRUE(MemberSignature(class1_method1_).DoesPrefixMatch(prefix));
  ASSERT_TRUE(MemberSignature(class1_method1_i_).DoesPrefixMatch(prefix));
  ASSERT_FALSE(MemberSignature(class1_method12_).DoesPrefixMatch(prefix));
}

TEST_F(HiddenApiTest, CheckMethodSignatureMatch) {
  ScopedObjectAccess soa(self_);
  std::string prefix("Lmypackage/packagea/Class1;->method1(I)");
  ASSERT_FALSE(MemberSignature(class1_field1_).DoesPrefixMatch(prefix));
  ASSERT_FALSE(MemberSignature(class1_field12_).DoesPrefixMatch(prefix));
  ASSERT_FALSE(MemberSignature(class1_method1_).DoesPrefixMatch(prefix));
  ASSERT_TRUE(MemberSignature(class1_method1_i_).DoesPrefixMatch(prefix));
  ASSERT_FALSE(MemberSignature(class1_method12_).DoesPrefixMatch(prefix));
}

TEST_F(HiddenApiTest, CheckMethodSignatureAndReturnMatch) {
  ScopedObjectAccess soa(self_);
  std::string prefix("Lmypackage/packagea/Class1;->method1()V");
  ASSERT_FALSE(MemberSignature(class1_field1_).DoesPrefixMatch(prefix));
  ASSERT_FALSE(MemberSignature(class1_field12_).DoesPrefixMatch(prefix));
  ASSERT_TRUE(MemberSignature(class1_method1_).DoesPrefixMatch(prefix));
  ASSERT_FALSE(MemberSignature(class1_method1_i_).DoesPrefixMatch(prefix));
  ASSERT_FALSE(MemberSignature(class1_method12_).DoesPrefixMatch(prefix));
}

TEST_F(HiddenApiTest, CheckFieldMatch) {
  ScopedObjectAccess soa(self_);
  std::string prefix("Lmypackage/packagea/Class1;->field1");
  ASSERT_TRUE(MemberSignature(class1_field1_).DoesPrefixMatch(prefix));
  ASSERT_TRUE(MemberSignature(class1_field12_).DoesPrefixMatch(prefix));
  ASSERT_FALSE(MemberSignature(class1_method1_).DoesPrefixMatch(prefix));
  ASSERT_FALSE(MemberSignature(class1_method1_i_).DoesPrefixMatch(prefix));
  ASSERT_FALSE(MemberSignature(class1_method12_).DoesPrefixMatch(prefix));
}

TEST_F(HiddenApiTest, CheckFieldExactMatch) {
  ScopedObjectAccess soa(self_);
  std::string prefix("Lmypackage/packagea/Class1;->field1:");
  ASSERT_TRUE(MemberSignature(class1_field1_).DoesPrefixMatch(prefix));
  ASSERT_FALSE(MemberSignature(class1_field12_).DoesPrefixMatch(prefix));
  ASSERT_FALSE(MemberSignature(class1_method1_).DoesPrefixMatch(prefix));
}

TEST_F(HiddenApiTest, CheckFieldTypeMatch) {
  ScopedObjectAccess soa(self_);
  std::string prefix("Lmypackage/packagea/Class1;->field1:I");
  ASSERT_TRUE(MemberSignature(class1_field1_).DoesPrefixMatch(prefix));
  ASSERT_FALSE(MemberSignature(class1_field12_).DoesPrefixMatch(prefix));
  ASSERT_FALSE(MemberSignature(class1_method1_).DoesPrefixMatch(prefix));
}

TEST_F(HiddenApiTest, CheckConstructorMatch) {
  ScopedObjectAccess soa(self_);
  std::string prefix("Lmypackage/packagea/Class1;-><init>");
  ASSERT_TRUE(MemberSignature(class1_init_).DoesPrefixMatch(prefix));
  ASSERT_FALSE(MemberSignature(class1_method1_).DoesPrefixMatch(prefix));
}

TEST_F(HiddenApiTest, CheckConstructorExactMatch) {
  ScopedObjectAccess soa(self_);
  std::string prefix("Lmypackage/packagea/Class1;-><init>()V");
  ASSERT_TRUE(MemberSignature(class1_init_).DoesPrefixMatch(prefix));
  ASSERT_FALSE(MemberSignature(class1_method1_).DoesPrefixMatch(prefix));
}

TEST_F(HiddenApiTest, CheckMethodSignatureTrailingCharsNoMatch) {
  ScopedObjectAccess soa(self_);
  std::string prefix("Lmypackage/packagea/Class1;->method1()Vfoo");
  ASSERT_FALSE(MemberSignature(class1_method1_).DoesPrefixMatch(prefix));
}

TEST_F(HiddenApiTest, CheckConstructorTrailingCharsNoMatch) {
  ScopedObjectAccess soa(self_);
  std::string prefix("Lmypackage/packagea/Class1;-><init>()Vfoo");
  ASSERT_FALSE(MemberSignature(class1_init_).DoesPrefixMatch(prefix));
}

TEST_F(HiddenApiTest, CheckFieldTrailingCharsNoMatch) {
  ScopedObjectAccess soa(self_);
  std::string prefix("Lmypackage/packagea/Class1;->field1:Ifoo");
  ASSERT_FALSE(MemberSignature(class1_field1_).DoesPrefixMatch(prefix));
}

TEST_F(HiddenApiTest, CheckMemberSignatureForProxyClass) {
  ScopedObjectAccess soa(self_);
  StackHandleScope<4> hs(soa.Self());
  Handle<mirror::ClassLoader> class_loader(
      hs.NewHandle(soa.Decode<mirror::ClassLoader>(jclass_loader_)));

  // Find interface we will create a proxy for.
  Handle<mirror::Class> h_iface(hs.NewHandle(
      class_linker_->FindClass(soa.Self(), "Lmypackage/packagea/Interface;", class_loader)));
  ASSERT_TRUE(h_iface != nullptr);

  // Create the proxy class.
  std::vector<Handle<mirror::Class>> interfaces;
  interfaces.push_back(h_iface);
  Handle<mirror::Class> proxyClass = hs.NewHandle(proxy_test::GenerateProxyClass(
      soa, jclass_loader_, runtime_->GetClassLinker(), "$Proxy1234", interfaces));
  ASSERT_TRUE(proxyClass != nullptr);
  ASSERT_TRUE(proxyClass->IsProxyClass());
  ASSERT_TRUE(proxyClass->IsInitialized());

  // Find the "method" virtual method.
  ArtMethod* method = nullptr;
  for (auto& m : proxyClass->GetDeclaredVirtualMethods(kRuntimePointerSize)) {
    if (strcmp("method", m.GetInterfaceMethodIfProxy(kRuntimePointerSize)->GetName()) == 0) {
      method = &m;
      break;
    }
  }
  ASSERT_TRUE(method != nullptr);

  // Find the "interfaces" static field. This is generated for all proxies.
  ArtField* field = nullptr;
  for (size_t i = 0; i < proxyClass->NumStaticFields(); ++i) {
    ArtField* f = proxyClass->GetStaticField(i);
    if (strcmp("interfaces", f->GetName()) == 0) {
      field = f;
      break;
    }
  }
  ASSERT_TRUE(field != nullptr);

  // Test the signature. We expect the signature from the interface class.
  std::ostringstream ss_method;
  MemberSignature(method).Dump(ss_method);
  ASSERT_EQ("Lmypackage/packagea/Interface;->method()V", ss_method.str());

  // Test the signature. We expect the signature of the proxy class.
  std::ostringstream ss_field;
  MemberSignature(field).Dump(ss_field);
  ASSERT_EQ("L$Proxy1234;->interfaces:[Ljava/lang/Class;", ss_field.str());
}

}  // namespace art
