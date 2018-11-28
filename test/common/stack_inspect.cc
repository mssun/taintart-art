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

#include "jni.h"

#include <android-base/logging.h>

#include "base/mutex.h"
#include "dex/dex_file-inl.h"
#include "jni/jni_internal.h"
#include "mirror/class-inl.h"
#include "nth_caller_visitor.h"
#include "oat_file.h"
#include "runtime.h"
#include "scoped_thread_state_change-inl.h"
#include "stack.h"
#include "thread-current-inl.h"

namespace art {

static bool asserts_enabled = true;

// public static native void disableStackFrameAsserts();
// Note: to globally disable asserts in unsupported configurations.

extern "C" JNIEXPORT void JNICALL Java_Main_disableStackFrameAsserts(JNIEnv* env ATTRIBUTE_UNUSED,
                                                                     jclass cls ATTRIBUTE_UNUSED) {
  asserts_enabled = false;
}

static jboolean IsInterpreted(JNIEnv* env, jclass, size_t level) {
  ScopedObjectAccess soa(env);
  NthCallerVisitor caller(soa.Self(), level, false);
  caller.WalkStack();
  CHECK(caller.caller != nullptr);
  return caller.GetCurrentShadowFrame() != nullptr ? JNI_TRUE : JNI_FALSE;
}

// public static native boolean isInterpreted();

extern "C" JNIEXPORT jboolean JNICALL Java_Main_isInterpreted(JNIEnv* env, jclass klass) {
  return IsInterpreted(env, klass, 1);
}

// public static native boolean isInterpreted(int depth);

extern "C" JNIEXPORT jboolean JNICALL Java_Main_isInterpretedAt(JNIEnv* env,
                                                                jclass klass,
                                                                jint depth) {
  return IsInterpreted(env, klass, depth);
}


// public static native boolean isInterpretedFunction(String smali);

static bool IsMethodInterpreted(Thread* self,
                                const ArtMethod* goal,
                                const bool require_deoptable,
                                /* out */ bool* method_is_interpreted)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  *method_is_interpreted = true;
  bool method_found = false;
  bool prev_was_runtime = true;
  StackVisitor::WalkStack(
      [&](const art::StackVisitor* stack_visitor) REQUIRES_SHARED(Locks::mutator_lock_) {
        if (goal == stack_visitor->GetMethod()) {
          *method_is_interpreted =
              (require_deoptable && prev_was_runtime) || stack_visitor->IsShadowFrame();
          method_found = true;
          return false;
        }
        prev_was_runtime = stack_visitor->GetMethod()->IsRuntimeMethod();
        return true;
      },
      self,
      /* context= */ nullptr,
      art::StackVisitor::StackWalkKind::kIncludeInlinedFrames);
  return method_found;
}

// TODO Remove 'require_deoptimizable' option once we have deoptimization through runtime frames.
extern "C" JNIEXPORT jboolean JNICALL Java_Main_isInterpretedFunction(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED, jobject method, jboolean require_deoptimizable) {
  // Return false if this seems to not be an ART runtime.
  if (Runtime::Current() == nullptr) {
    return JNI_FALSE;
  }
  if (method == nullptr) {
    env->ThrowNew(env->FindClass("java/lang/NullPointerException"), "method is null!");
    return JNI_FALSE;
  }
  jmethodID id = env->FromReflectedMethod(method);
  if (id == nullptr) {
    env->ThrowNew(env->FindClass("java/lang/Error"), "Unable to interpret method argument!");
    return JNI_FALSE;
  }
  {
    ScopedObjectAccess soa(env);
    ArtMethod* goal = jni::DecodeArtMethod(id);
    bool is_interpreted;
    if (!IsMethodInterpreted(soa.Self(), goal, require_deoptimizable, &is_interpreted)) {
      env->ThrowNew(env->FindClass("java/lang/Error"), "Unable to find given method in stack!");
      return JNI_FALSE;
    }
    bool enters_interpreter = Runtime::Current()->GetClassLinker()->IsQuickToInterpreterBridge(
        goal->GetEntryPointFromQuickCompiledCode());
    return (is_interpreted || enters_interpreter);
  }
}

// public static native void assertIsInterpreted();

extern "C" JNIEXPORT void JNICALL Java_Main_assertIsInterpreted(JNIEnv* env, jclass klass) {
  if (asserts_enabled) {
    CHECK(Java_Main_isInterpreted(env, klass));
  }
}

static jboolean IsManaged(JNIEnv* env, jclass, size_t level) {
  ScopedObjectAccess soa(env);
  NthCallerVisitor caller(soa.Self(), level, false);
  caller.WalkStack();
  CHECK(caller.caller != nullptr);
  return caller.GetCurrentShadowFrame() != nullptr ? JNI_FALSE : JNI_TRUE;
}

// public static native boolean isManaged();

extern "C" JNIEXPORT jboolean JNICALL Java_Main_isManaged(JNIEnv* env, jclass cls) {
  return IsManaged(env, cls, 1);
}

// public static native void assertIsManaged();

extern "C" JNIEXPORT void JNICALL Java_Main_assertIsManaged(JNIEnv* env, jclass cls) {
  if (asserts_enabled) {
    CHECK(Java_Main_isManaged(env, cls));
  }
}

// public static native boolean isCallerInterpreted();

extern "C" JNIEXPORT jboolean JNICALL Java_Main_isCallerInterpreted(JNIEnv* env, jclass klass) {
  return IsInterpreted(env, klass, 2);
}

// public static native void assertCallerIsInterpreted();

extern "C" JNIEXPORT void JNICALL Java_Main_assertCallerIsInterpreted(JNIEnv* env, jclass klass) {
  if (asserts_enabled) {
    CHECK(Java_Main_isCallerInterpreted(env, klass));
  }
}

// public static native boolean isCallerManaged();

extern "C" JNIEXPORT jboolean JNICALL Java_Main_isCallerManaged(JNIEnv* env, jclass cls) {
  return IsManaged(env, cls, 2);
}

// public static native void assertCallerIsManaged();

extern "C" JNIEXPORT void JNICALL Java_Main_assertCallerIsManaged(JNIEnv* env, jclass cls) {
  if (asserts_enabled) {
    CHECK(Java_Main_isCallerManaged(env, cls));
  }
}

extern "C" JNIEXPORT jobject JNICALL Java_Main_getThisOfCaller(
    JNIEnv* env, jclass cls ATTRIBUTE_UNUSED) {
  ScopedObjectAccess soa(env);
  std::unique_ptr<art::Context> context(art::Context::Create());
  jobject result = nullptr;
  StackVisitor::WalkStack(
      [&](const art::StackVisitor* stack_visitor) REQUIRES_SHARED(Locks::mutator_lock_) {
        // Discard stubs and Main.getThisOfCaller.
        if (stack_visitor->GetMethod() == nullptr || stack_visitor->GetMethod()->IsNative()) {
          return true;
        }
        result = soa.AddLocalReference<jobject>(stack_visitor->GetThisObject());
        return false;
      },
      soa.Self(),
      context.get(),
      art::StackVisitor::StackWalkKind::kIncludeInlinedFrames);
  return result;
}

}  // namespace art
