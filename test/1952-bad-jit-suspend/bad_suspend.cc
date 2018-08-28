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

#include <atomic>
#include <mutex>
#include <vector>

#include <jvmti.h>
#include <jni.h>

#include <android-base/logging.h>
#include <android-base/macros.h>

#include "art_method.h"
#include "base/runtime_debug.h"
#include "jit/jit.h"
#include "jit/jit_code_cache.h"
#include "jni/jni_internal.h"
#include "runtime-inl.h"
#include "scoped_thread_state_change-inl.h"
#include "thread_list.h"
#include "thread-inl.h"

// Test infrastructure
#include "nativehelper/ScopedLocalRef.h"

// Slicer's headers have code that triggers these warnings. b/65298177
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-compare"
#pragma clang diagnostic ignored "-Wunused-parameter"
#include "slicer/instrumentation.h"
#include "slicer/reader.h"
#include "slicer/writer.h"
#pragma clang diagnostic pop


namespace art {
namespace Test1952BadSuspend {

// The jvmti env we will be using.
static jvmtiEnv* jvmti_env = nullptr;

// Marker to allow us to stop after 30 seconds.
static std::atomic<bool> should_continue(true);

// Whether we need to add or remove a nop from the class being redefined. This toggles every
// transform.
static bool add_nops = true;

// Marker to request that the main thread clear all compiled jit code and a barrier to wait for this
// to occur.
static std::atomic<bool> clear_jit(false);
static Barrier barrier(0);

// Marker and barrier to ensure both redefine thread and main thread have started.
static std::atomic<bool> starting(true);
static Barrier start_barrier(2);

static jthread GetJitThread() {
  ScopedObjectAccess soa(Thread::Current());
  auto* jit = Runtime::Current()->GetJit();
  if (jit == nullptr) {
    return nullptr;
  }
  auto* thread_pool = jit->GetThreadPool();
  if (thread_pool == nullptr) {
    return nullptr;
  }
  // It doesn't really matter which jit-thread we pick as long as a class-load happens on it. Since
  // this is equally likely to happen on any of them (and currently we only have one jit-thread) we
  // just pick the first thread.
  return soa.AddLocalReference<jthread>(
          thread_pool->GetWorkers()[0]->GetThread()->GetPeerFromOtherThread());
}

static JNICALL void AgentThreadWait(jvmtiEnv* jvmti ATTRIBUTE_UNUSED,
                                    JNIEnv* env ATTRIBUTE_UNUSED,
                                    void* arg ATTRIBUTE_UNUSED) {
  // Give us 30 seconds to deadlock. If we don't get a deadlock then, better restart the test than
  // continue.
  sleep(30);
  should_continue.store(false);
}

static JNICALL void AgentThreadRedefine(jvmtiEnv* jvmti,
                                        JNIEnv* env,
                                        void* arg ATTRIBUTE_UNUSED) {
  start_barrier.Wait(Thread::Current());

  jclass target = env->FindClass("Main$TargetClass");
  jclass main = env->FindClass("Main");
  ArtMethod* doNothingMethod =
      jni::DecodeArtMethod(env->GetStaticMethodID(main, "doNothing", "()V"));
  Runtime* runtime = Runtime::Current();

  while (should_continue.load()) {
    if (runtime->GetJit() != nullptr &&
        runtime->GetJit()->GetCodeCache()->WillExecuteJitCode(doNothingMethod)) {
      // Let main thread clear the jit so the method won't be live on stack.
      clear_jit.store(true);
      barrier.Increment(Thread::Current(), 1);
      continue;
    }
    jvmti->RetransformClasses(1, &target);
  }
}

static void StartThread(jvmtiEnv* jvmti, jvmtiStartFunction func, jobject thr) {
  jvmtiError err = jvmti->RunAgentThread(thr, func, nullptr, JVMTI_THREAD_NORM_PRIORITY);
  CHECK_EQ(err, JVMTI_ERROR_NONE);
}

class NoOps : public slicer::Transformation {
 public:
  explicit NoOps(bool should_add_nops) : should_add_nops_(should_add_nops) {}

  virtual bool Apply(lir::CodeIr* code_ir) {
    ir::Builder builder(code_ir->dex_ir);

    // insert the hook before the first bytecode in the method body
    for (auto instr : code_ir->instructions) {
      // Annoying laziness on part of slicer. This dynamic cast is nessecary to write a
      // transformation.
      auto bytecode = dynamic_cast<lir::Bytecode*>(instr);
      if (bytecode == nullptr) {
        continue;
      }
      if (should_add_nops_) {
        // add nop bytecode.
        auto hook_invoke = code_ir->Alloc<lir::Bytecode>();
        hook_invoke->opcode = ::dex::OP_NOP;
        code_ir->instructions.InsertBefore(bytecode, hook_invoke);
      } else {
        code_ir->instructions.Remove(bytecode);
      }
      break;
    }

    return true;
  }

 private:
  bool should_add_nops_;
};

static JNICALL void LoadHookCb(jvmtiEnv *jvmti,
                               JNIEnv* env ATTRIBUTE_UNUSED,
                               jclass class_being_redefined,
                               jobject loader ATTRIBUTE_UNUSED,
                               const char* name,
                               jobject protection_domain ATTRIBUTE_UNUSED,
                               jint class_data_len,
                               const unsigned char* class_data,
                               jint* new_class_data_len,
                               unsigned char** new_class_data) {
  if (class_being_redefined == nullptr || strcmp(name, "LMain$TargetClass") != 0) {
    return;
  }
  ::dex::Reader reader(class_data, class_data_len);
  ::dex::u4 class_index = reader.FindClassIndex("LMain$TargetClass;");
  if (class_index == ::dex::kNoIndex) {
    LOG(FATAL) << "Failed to find object in dex file!";
    return;
  }

  reader.CreateClassIr(class_index);
  auto dex_ir = reader.GetIr();

  slicer::MethodInstrumenter mi(dex_ir);
  mi.AddTransformation<NoOps>(add_nops);
  if (!mi.InstrumentMethod(ir::MethodId("LMain$TargetClass", "foo", "()V"))) {
    LOG(FATAL) << "Failed to find LMain$TargetClass->foo()V in dex file!";
    return;
  }


  ::dex::Writer writer(dex_ir);

  class JvmtiAllocator : public ::dex::Writer::Allocator {
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
  JvmtiAllocator allocator(jvmti);
  size_t new_size;
  *new_class_data = writer.CreateImage(&allocator, &new_size);
  if (new_size > std::numeric_limits<jint>::max()) {
    *new_class_data = nullptr;
    LOG(FATAL) << "transform result is too large!";
    return;
  }
  *new_class_data_len = static_cast<jint>(new_size);
  add_nops = !add_nops;
}

extern "C" JNIEXPORT JNICALL void Java_Main_startWaitThread(JNIEnv*, jclass, jobject thr) {
  StartThread(jvmti_env, AgentThreadWait, thr);
}
extern "C" JNIEXPORT JNICALL void Java_Main_startRedefineThread(JNIEnv*, jclass, jobject thr) {
  StartThread(jvmti_env, AgentThreadRedefine, thr);
}

// This function will be called when a class is prepared according to JLS 12.3.2. The event is
// restricted to just the JIT thread by the JVMTI SetEventNotificationMode thread argument. See
// go/jvmti-spec#ClassPrepare for more information.
//
// Pause the jit thread and never let it finish compiling whatever class we have here. Currently we
// have a bit of a hack (b/70838465) that prevents SuspendThread from working on the JitThread but
// this demonstrates that this solution is not really sufficient.
JNICALL void ClassPrepareJit(jvmtiEnv* jvmti ATTRIBUTE_UNUSED,
                             JNIEnv* jni_env ATTRIBUTE_UNUSED,
                             jthread thread ATTRIBUTE_UNUSED,
                             jclass klass ATTRIBUTE_UNUSED) {
  Runtime* runtime = Runtime::Current();
  Thread* self = Thread::Current();
  LOG(WARNING) << "Looping forever on jit thread!";
  while (!runtime->IsShuttingDown(self)) {
    sched_yield();
  }
}

JNICALL void VmInitCb(jvmtiEnv *jvmti,
                      JNIEnv* env ATTRIBUTE_UNUSED,
                      jthread curthread ATTRIBUTE_UNUSED) {
  // Handler already set to ClassPrepareJit
  jthread jit_thread = GetJitThread();
  if (jit_thread != nullptr) {
    CHECK_EQ(jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_CLASS_PREPARE, jit_thread),
             JVMTI_ERROR_NONE);
  }
}

extern "C" jint JNICALL Agent_OnLoad(JavaVM* vm,
                                     char* options ATTRIBUTE_UNUSED,
                                     void* reserved ATTRIBUTE_UNUSED) {
  jvmtiEnv* env = nullptr;

#define CHECK_CALL_SUCCESS(c) \
  do { \
    auto vc = (c); \
    CHECK(vc == JNI_OK || vc == JVMTI_ERROR_NONE) << "call " << #c  << " did not succeed\n"; \
  } while (false)

  CHECK_CALL_SUCCESS(vm->GetEnv(reinterpret_cast<void**>(&env), JVMTI_VERSION_1_0));
  jvmtiEventCallbacks cb {
        .VMInit = VmInitCb,
        .ClassFileLoadHook = LoadHookCb,
        .ClassPrepare = ClassPrepareJit,
  };
  jvmtiCapabilities caps { .can_retransform_classes = 1, .can_suspend = 1, };
  CHECK_CALL_SUCCESS(env->SetEventCallbacks(&cb, sizeof(cb)));
  CHECK_CALL_SUCCESS(env->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_INIT, nullptr));
  CHECK_CALL_SUCCESS(
      env->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, nullptr));
  CHECK_CALL_SUCCESS(env->AddCapabilities(&caps));

  jvmti_env = env;
#undef CHECK_CALL_SUCCESS

  return 0;
}

extern "C" JNIEXPORT jboolean Java_Main_shouldContinue(JNIEnv* env, jclass main) {
  if (starting.load()) {
    start_barrier.Pass(Thread::Current());
    starting.store(false);
  }
  if (clear_jit.load()) {
    Runtime* runtime = Runtime::Current();
    ArtMethod* doNothingMethod =
        jni::DecodeArtMethod(env->GetStaticMethodID(main, "doNothing", "()V"));
    jit::ScopedJitSuspend sjs;
    if (runtime->GetJit() != nullptr) {
      {
        // Remove the method's compiled code in the same way that JVMTI class redefinition would.
        ScopedSuspendAll soa("Remove method from jit", /*long*/ false);
        runtime->GetJit()->GetCodeCache()->NotifyMethodRedefined(doNothingMethod);
        runtime->GetInstrumentation()->UpdateMethodsCodeToInterpreterEntryPoint(doNothingMethod);
      }
      {
        ScopedObjectAccess soa(Thread::Current());
        // Clear the jit and try again.
        runtime->GetJit()->GetCodeCache()->GarbageCollectCache(Thread::Current());
      }
    }
    clear_jit.store(false);
    // Tell the redefine thread that we just cleared the jit and allow it to continue.
    barrier.Pass(Thread::Current());
    // Try to make sure the redefine thread has a chance to wake up by yielding. This seems to make
    // the deadlock much more likely.
    sched_yield();
  }
  return should_continue.load();
}

};  // namespace Test1952BadSuspend
};  // namespace art
