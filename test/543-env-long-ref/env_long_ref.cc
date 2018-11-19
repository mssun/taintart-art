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

extern "C" JNIEXPORT void JNICALL Java_Main_lookForMyRegisters(JNIEnv*, jclass, jobject value) {
  ScopedObjectAccess soa(Thread::Current());
  std::unique_ptr<Context> context(Context::Create());
  bool found = false;
  StackVisitor::WalkStack(
      [&](const art::StackVisitor* stack_visitor) REQUIRES_SHARED(Locks::mutator_lock_) {
        ArtMethod* m = stack_visitor->GetMethod();
        std::string m_name(m->GetName());

        if (m_name == "testCase") {
          found = true;
          uint32_t stack_value = 0;
          CHECK(stack_visitor->GetVReg(m, 1, kReferenceVReg, &stack_value));
          CHECK_EQ(reinterpret_cast<mirror::Object*>(stack_value),
                   soa.Decode<mirror::Object>(value).Ptr());
        }
        return true;
      },
      soa.Self(),
      context.get(),
      art::StackVisitor::StackWalkKind::kIncludeInlinedFrames);
  CHECK(found);
}

}  // namespace art
