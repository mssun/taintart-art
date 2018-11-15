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

#include "arch/context.h"
#include "art_method-inl.h"
#include "jni.h"
#include "scoped_thread_state_change-inl.h"
#include "stack.h"
#include "thread.h"

namespace art {

namespace {

jint FindMethodIndex(jobject this_value_jobj) {
  ScopedObjectAccess soa(Thread::Current());
  std::unique_ptr<Context> context(Context::Create());
  ObjPtr<mirror::Object> this_value = soa.Decode<mirror::Object>(this_value_jobj);
  jint found_method_index = 0;
  StackVisitor::WalkStack(
      [&](const art::StackVisitor* stack_visitor) REQUIRES_SHARED(Locks::mutator_lock_) {
        ArtMethod* m = stack_visitor->GetMethod();
        std::string m_name(m->GetName());

        if (m_name.compare("$noinline$testThisWithInstanceCall") == 0) {
          found_method_index = 1;
          uint32_t value = 0;
          CHECK(stack_visitor->GetVReg(m, 1, kReferenceVReg, &value));
          CHECK_EQ(reinterpret_cast<mirror::Object*>(value), this_value);
          CHECK_EQ(stack_visitor->GetThisObject(), this_value);
        } else if (m_name.compare("$noinline$testThisWithStaticCall") == 0) {
          found_method_index = 2;
          uint32_t value = 0;
          CHECK(stack_visitor->GetVReg(m, 1, kReferenceVReg, &value));
        } else if (m_name.compare("$noinline$testParameter") == 0) {
          found_method_index = 3;
          uint32_t value = 0;
          CHECK(stack_visitor->GetVReg(m, 1, kReferenceVReg, &value));
        } else if (m_name.compare("$noinline$testObjectInScope") == 0) {
          found_method_index = 4;
          uint32_t value = 0;
          CHECK(stack_visitor->GetVReg(m, 0, kReferenceVReg, &value));
        }

        return true;
      },
      soa.Self(),
      context.get(),
      art::StackVisitor::StackWalkKind::kIncludeInlinedFrames);
  return found_method_index;
}

extern "C" JNIEXPORT jint JNICALL Java_Main_doNativeCallRef(JNIEnv*, jobject value) {
  return FindMethodIndex(value);
}

extern "C" JNIEXPORT jint JNICALL Java_Main_doStaticNativeCallRef(JNIEnv*, jclass) {
  return FindMethodIndex(nullptr);
}

}  // namespace

}  // namespace art
