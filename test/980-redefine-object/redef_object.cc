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

#include <limits>
#include <memory>

#include "jni.h"
#include "jvmti.h"

// Test infrastructure
#include "jvmti_helper.h"
#include "test_env.h"

// Slicer's headers have code that triggers these warnings. b/65298177
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-compare"
#pragma clang diagnostic ignored "-Wunused-parameter"
#include "slicer/instrumentation.h"
#include "slicer/reader.h"
#include "slicer/writer.h"
#pragma clang diagnostic pop

namespace art {
namespace Test980RedefineObject {

static void JNICALL RedefineObjectHook(jvmtiEnv *jvmti_env,
                                       JNIEnv* env,
                                       jclass class_being_redefined ATTRIBUTE_UNUSED,
                                       jobject loader ATTRIBUTE_UNUSED,
                                       const char* name,
                                       jobject protection_domain ATTRIBUTE_UNUSED,
                                       jint class_data_len,
                                       const unsigned char* class_data,
                                       jint* new_class_data_len,
                                       unsigned char** new_class_data) {
  if (strcmp(name, "java/lang/Object") != 0) {
    return;
  }

  dex::Reader reader(class_data, class_data_len);
  dex::u4 class_index = reader.FindClassIndex("Ljava/lang/Object;");
  if (class_index == dex::kNoIndex) {
    env->ThrowNew(env->FindClass("java/lang/RuntimeException"),
                  "Failed to find object in dex file!");
    return;
  }

  reader.CreateClassIr(class_index);
  auto dex_ir = reader.GetIr();

  slicer::MethodInstrumenter mi(dex_ir);
  mi.AddTransformation<slicer::EntryHook>(ir::MethodId("Lart/test/TestWatcher;",
                                                       "NotifyConstructed"),
                                          /*this_as_object*/ true);
  if (!mi.InstrumentMethod(ir::MethodId("Ljava/lang/Object;",
                                        "<init>",
                                        "()V"))) {
    env->ThrowNew(env->FindClass("java/lang/RuntimeException"),
                  "Failed to find Object;-><init>()V in dex file!");
    return;
  }


  dex::Writer writer(dex_ir);

  class JvmtiAllocator : public dex::Writer::Allocator {
   public:
    explicit JvmtiAllocator(jvmtiEnv* jvmti) : jvmti_(jvmti) {}

    void* Allocate(size_t size) {
      unsigned char* res = nullptr;
      jvmti_->Allocate(size, &res);
      return res;
    }

    void Free(void* ptr) {
      jvmti_->Deallocate(reinterpret_cast<unsigned char*>(ptr));
    }

   private:
    jvmtiEnv* jvmti_;
  };
  JvmtiAllocator allocator(jvmti_env);
  size_t new_size;
  *new_class_data = writer.CreateImage(&allocator, &new_size);
  if (new_size > std::numeric_limits<jint>::max()) {
    *new_class_data = nullptr;
    env->ThrowNew(env->FindClass("java/lang/RuntimeException"),
                  "transform result is too large!");
    return;
  }
  *new_class_data_len = static_cast<jint>(new_size);
}

extern "C" JNIEXPORT void JNICALL Java_Main_addMemoryTrackingCall(JNIEnv* env,
                                                                  jclass klass ATTRIBUTE_UNUSED,
                                                                  jclass obj_class,
                                                                  jthread thr) {
  jvmtiCapabilities caps {.can_retransform_classes = 1};
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->AddCapabilities(&caps))) {
    return;
  }
  jvmtiEventCallbacks cb {.ClassFileLoadHook = RedefineObjectHook };
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->SetEventCallbacks(&cb, sizeof(cb)))) {
    return;
  }
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->SetEventNotificationMode(JVMTI_ENABLE,
                                                                JVMTI_EVENT_CLASS_FILE_LOAD_HOOK,
                                                                thr))) {
    return;
  }
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->RetransformClasses(1, &obj_class))) {
    return;
  }
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->SetEventNotificationMode(JVMTI_DISABLE,
                                                                JVMTI_EVENT_CLASS_FILE_LOAD_HOOK,
                                                                thr))) {
    return;
  }
}

}  // namespace Test980RedefineObject
}  // namespace art

