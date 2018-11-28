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

#include "art_method-inl.h"
#include "jit/jit.h"
#include "jit/jit_code_cache.h"
#include "jit/profiling_info.h"
#include "nativehelper/ScopedUtfChars.h"
#include "oat_quick_method_header.h"
#include "scoped_thread_state_change-inl.h"
#include "stack.h"
#include "stack_map.h"
#include "thread-current-inl.h"

namespace art {

namespace {

template <typename Handler>
void ProcessMethodWithName(JNIEnv* env, jstring method_name, const Handler& handler) {
  ScopedUtfChars chars(env, method_name);
  CHECK(chars.c_str() != nullptr);
  ScopedObjectAccess soa(Thread::Current());
  StackVisitor::WalkStack(
      [&](const art::StackVisitor* stack_visitor) REQUIRES_SHARED(Locks::mutator_lock_) {
        std::string m_name(stack_visitor->GetMethod()->GetName());

        if (m_name.compare(chars.c_str()) == 0) {
          handler(stack_visitor);
          return false;
        }
        return true;
      },
      soa.Self(),
      /* context= */ nullptr,
      art::StackVisitor::StackWalkKind::kIncludeInlinedFrames);
}

}  // namespace

extern "C" JNIEXPORT jboolean JNICALL Java_Main_isInOsrCode(JNIEnv* env,
                                                            jclass,
                                                            jstring method_name) {
  jit::Jit* jit = Runtime::Current()->GetJit();
  if (jit == nullptr) {
    // Just return true for non-jit configurations to stop the infinite loop.
    return JNI_TRUE;
  }
  bool in_osr_code = false;
  ProcessMethodWithName(
      env,
      method_name,
      [&](const art::StackVisitor* stack_visitor) REQUIRES_SHARED(Locks::mutator_lock_) {
        ArtMethod* m = stack_visitor->GetMethod();
        const OatQuickMethodHeader* header =
            Runtime::Current()->GetJit()->GetCodeCache()->LookupOsrMethodHeader(m);
        if (header != nullptr && header == stack_visitor->GetCurrentOatQuickMethodHeader()) {
          in_osr_code = true;
        }
      });
  return in_osr_code;
}

extern "C" JNIEXPORT jboolean JNICALL Java_Main_isInInterpreter(JNIEnv* env,
                                                                jclass,
                                                                jstring method_name) {
  if (!Runtime::Current()->UseJitCompilation()) {
    // The return value is irrelevant if we're not using JIT.
    return false;
  }
  bool in_interpreter = false;
  ProcessMethodWithName(
      env,
      method_name,
      [&](const art::StackVisitor* stack_visitor) REQUIRES_SHARED(Locks::mutator_lock_) {
        ArtMethod* m = stack_visitor->GetMethod();
        const OatQuickMethodHeader* header =
            Runtime::Current()->GetJit()->GetCodeCache()->LookupOsrMethodHeader(m);
        if ((header == nullptr || header != stack_visitor->GetCurrentOatQuickMethodHeader()) &&
            stack_visitor->IsShadowFrame()) {
          in_interpreter = true;
        }
      });
  return in_interpreter;
}

extern "C" JNIEXPORT void JNICALL Java_Main_ensureHasProfilingInfo(JNIEnv* env,
                                                                   jclass,
                                                                   jstring method_name) {
  if (!Runtime::Current()->UseJitCompilation()) {
    return;
  }
  ProcessMethodWithName(
      env,
      method_name,
      [&](const art::StackVisitor* stack_visitor) REQUIRES_SHARED(Locks::mutator_lock_) {
        ArtMethod* m = stack_visitor->GetMethod();
        ProfilingInfo::Create(Thread::Current(), m, /* retry_allocation */ true);
      });
}

extern "C" JNIEXPORT void JNICALL Java_Main_ensureHasOsrCode(JNIEnv* env,
                                                             jclass,
                                                             jstring method_name) {
  if (!Runtime::Current()->UseJitCompilation()) {
    return;
  }
  ProcessMethodWithName(
      env,
      method_name,
      [&](const art::StackVisitor* stack_visitor) REQUIRES_SHARED(Locks::mutator_lock_) {
        ArtMethod* m = stack_visitor->GetMethod();
        jit::Jit* jit = Runtime::Current()->GetJit();
        while (jit->GetCodeCache()->LookupOsrMethodHeader(m) == nullptr) {
          // Sleep to yield to the compiler thread.
          usleep(1000);
          // Will either ensure it's compiled or do the compilation itself.
          jit->CompileMethod(m, Thread::Current(), /* osr */ true);
        }
      });
}

}  // namespace art
