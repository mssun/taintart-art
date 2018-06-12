// Copyright (C) 2018 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <android-base/logging.h>

#include <atomic>
#include <iostream>
#include <istream>
#include <iomanip>
#include <jni.h>
#include <jvmti.h>
#include <memory>
#include <string>
#include <sstream>
#include <vector>

namespace tifast {

#define EVENT(x) JVMTI_EVENT_ ## x

namespace {

// Special art ti-version number. We will use this as a fallback if we cannot get a regular JVMTI
// env.
static constexpr jint kArtTiVersion = JVMTI_VERSION_1_2 | 0x40000000;

static void AddCapsForEvent(jvmtiEvent event, jvmtiCapabilities* caps) {
  switch (event) {
#define DO_CASE(name, cap_name) \
    case EVENT(name):           \
      caps->cap_name = 1;       \
      break
    DO_CASE(SINGLE_STEP, can_generate_single_step_events);
    DO_CASE(METHOD_ENTRY, can_generate_method_entry_events);
    DO_CASE(METHOD_EXIT, can_generate_method_exit_events);
    DO_CASE(NATIVE_METHOD_BIND, can_generate_native_method_bind_events);
    DO_CASE(EXCEPTION, can_generate_exception_events);
    DO_CASE(EXCEPTION_CATCH, can_generate_exception_events);
    DO_CASE(COMPILED_METHOD_LOAD, can_generate_compiled_method_load_events);
    DO_CASE(COMPILED_METHOD_UNLOAD, can_generate_compiled_method_load_events);
    DO_CASE(MONITOR_CONTENDED_ENTER, can_generate_monitor_events);
    DO_CASE(MONITOR_CONTENDED_ENTERED, can_generate_monitor_events);
    DO_CASE(MONITOR_WAIT, can_generate_monitor_events);
    DO_CASE(MONITOR_WAITED, can_generate_monitor_events);
    DO_CASE(VM_OBJECT_ALLOC, can_generate_vm_object_alloc_events);
    DO_CASE(GARBAGE_COLLECTION_START, can_generate_garbage_collection_events);
    DO_CASE(GARBAGE_COLLECTION_FINISH, can_generate_garbage_collection_events);
#undef DO_CASE
    default: break;
  }
}

// Setup for all supported events. Give a macro with fun(name, event_num, args)
#define FOR_ALL_SUPPORTED_EVENTS(fun) \
    fun(SingleStep, EVENT(SINGLE_STEP), (jvmtiEnv*, JNIEnv*, jthread, jmethodID, jlocation)) \
    fun(MethodEntry, EVENT(METHOD_ENTRY), (jvmtiEnv*, JNIEnv*, jthread, jmethodID)) \
    fun(MethodExit, EVENT(METHOD_EXIT), (jvmtiEnv*, JNIEnv*, jthread, jmethodID, jboolean, jvalue)) \
    fun(NativeMethodBind, EVENT(NATIVE_METHOD_BIND), (jvmtiEnv*, JNIEnv*, jthread, jmethodID, void*, void**)) \
    fun(Exception, EVENT(EXCEPTION), (jvmtiEnv*, JNIEnv*, jthread, jmethodID, jlocation, jobject, jmethodID, jlocation)) \
    fun(ExceptionCatch, EVENT(EXCEPTION_CATCH), (jvmtiEnv*, JNIEnv*, jthread, jmethodID, jlocation, jobject)) \
    fun(ThreadStart, EVENT(THREAD_START), (jvmtiEnv*, JNIEnv*, jthread)) \
    fun(ThreadEnd, EVENT(THREAD_END), (jvmtiEnv*, JNIEnv*, jthread)) \
    fun(ClassLoad, EVENT(CLASS_LOAD), (jvmtiEnv*, JNIEnv*, jthread, jclass)) \
    fun(ClassPrepare, EVENT(CLASS_PREPARE), (jvmtiEnv*, JNIEnv*, jthread, jclass)) \
    fun(ClassFileLoadHook, EVENT(CLASS_FILE_LOAD_HOOK), (jvmtiEnv*, JNIEnv*, jclass, jobject, const char*, jobject, jint, const unsigned char*, jint*, unsigned char**)) \
    fun(CompiledMethodLoad, EVENT(COMPILED_METHOD_LOAD), (jvmtiEnv*, jmethodID, jint, const void*, jint, const jvmtiAddrLocationMap*, const void*)) \
    fun(CompiledMethodUnload, EVENT(COMPILED_METHOD_UNLOAD), (jvmtiEnv*, jmethodID, const void*)) \
    fun(DynamicCodeGenerated, EVENT(DYNAMIC_CODE_GENERATED), (jvmtiEnv*, const char*, const void*, jint)) \
    fun(DataDumpRequest, EVENT(DATA_DUMP_REQUEST), (jvmtiEnv*)) \
    fun(MonitorContendedEnter, EVENT(MONITOR_CONTENDED_ENTER), (jvmtiEnv*, JNIEnv*, jthread, jobject)) \
    fun(MonitorContendedEntered, EVENT(MONITOR_CONTENDED_ENTERED), (jvmtiEnv*, JNIEnv*, jthread, jobject)) \
    fun(MonitorWait, EVENT(MONITOR_WAIT), (jvmtiEnv*, JNIEnv*, jthread, jobject, jlong)) \
    fun(MonitorWaited, EVENT(MONITOR_WAITED), (jvmtiEnv*, JNIEnv*, jthread, jobject, jboolean)) \
    fun(ResourceExhausted, EVENT(RESOURCE_EXHAUSTED), (jvmtiEnv*, JNIEnv*, jint, const void*, const char*)) \
    fun(VMObjectAlloc, EVENT(VM_OBJECT_ALLOC), (jvmtiEnv*, JNIEnv*, jthread, jobject, jclass, jlong)) \
    fun(GarbageCollectionStart, EVENT(GARBAGE_COLLECTION_START), (jvmtiEnv*)) \
    fun(GarbageCollectionFinish, EVENT(GARBAGE_COLLECTION_FINISH), (jvmtiEnv*))

#define GENERATE_EMPTY_FUNCTION(name, number, args) \
    static void JNICALL empty ## name  args { }
FOR_ALL_SUPPORTED_EVENTS(GENERATE_EMPTY_FUNCTION)
#undef GENERATE_EMPTY_FUNCTION

static jvmtiEventCallbacks kEmptyCallbacks {
#define CREATE_EMPTY_EVENT_CALLBACKS(name, num, args) \
    .name = empty ## name,
  FOR_ALL_SUPPORTED_EVENTS(CREATE_EMPTY_EVENT_CALLBACKS)
#undef CREATE_EMPTY_EVENT_CALLBACKS
};

#define GENERATE_LOG_FUNCTION(name, number, args) \
    static void JNICALL log ## name  args { \
      LOG(INFO) << "Got event " << #name ; \
    }
FOR_ALL_SUPPORTED_EVENTS(GENERATE_LOG_FUNCTION)
#undef GENERATE_LOG_FUNCTION

static jvmtiEventCallbacks kLogCallbacks {
#define CREATE_LOG_EVENT_CALLBACK(name, num, args) \
    .name = log ## name,
  FOR_ALL_SUPPORTED_EVENTS(CREATE_LOG_EVENT_CALLBACK)
#undef CREATE_LOG_EVENT_CALLBACK
};

static jvmtiEvent NameToEvent(const std::string& desired_name) {
#define CHECK_NAME(name, event, args) \
  if (desired_name == #name) { \
    return event; \
  }
  FOR_ALL_SUPPORTED_EVENTS(CHECK_NAME);
  LOG(FATAL) << "Unknown event " << desired_name;
  __builtin_unreachable();
#undef CHECK_NAME
}

#undef FOR_ALL_SUPPORTED_EVENTS
static std::vector<jvmtiEvent> GetRequestedEventList(const std::string& args) {
  std::vector<jvmtiEvent> res;
  std::stringstream args_stream(args);
  std::string item;
  while (std::getline(args_stream, item, ',')) {
    if (item == "") {
      continue;
    }
    res.push_back(NameToEvent(item));
  }
  return res;
}

static jint SetupJvmtiEnv(JavaVM* vm, jvmtiEnv** jvmti) {
  jint res = 0;
  res = vm->GetEnv(reinterpret_cast<void**>(jvmti), JVMTI_VERSION_1_1);

  if (res != JNI_OK || *jvmti == nullptr) {
    LOG(ERROR) << "Unable to access JVMTI, error code " << res;
    return vm->GetEnv(reinterpret_cast<void**>(jvmti), kArtTiVersion);
  }
  return res;
}

}  // namespace

static jint AgentStart(JavaVM* vm,
                       char* options,
                       void* reserved ATTRIBUTE_UNUSED) {
  jvmtiEnv* jvmti = nullptr;
  jvmtiError error = JVMTI_ERROR_NONE;
  if (SetupJvmtiEnv(vm, &jvmti) != JNI_OK) {
    LOG(ERROR) << "Could not get JVMTI env or ArtTiEnv!";
    return JNI_ERR;
  }
  std::string args(options);
  bool is_log = false;
  if (args.compare(0, 3, "log") == 0) {
    is_log = true;
    args = args.substr(3);
  }

  std::vector<jvmtiEvent> events = GetRequestedEventList(args);

  jvmtiCapabilities caps{};
  for (jvmtiEvent e : events) {
    AddCapsForEvent(e, &caps);
  }
  error = jvmti->AddCapabilities(&caps);
  if (error != JVMTI_ERROR_NONE) {
    LOG(ERROR) << "Unable to set caps";
    return JNI_ERR;
  }

  if (is_log) {
    error = jvmti->SetEventCallbacks(&kLogCallbacks, static_cast<jint>(sizeof(kLogCallbacks)));
  } else {
    error = jvmti->SetEventCallbacks(&kEmptyCallbacks, static_cast<jint>(sizeof(kEmptyCallbacks)));
  }
  if (error != JVMTI_ERROR_NONE) {
    LOG(ERROR) << "Unable to set event callbacks.";
    return JNI_ERR;
  }
  for (jvmtiEvent e : events) {
    error = jvmti->SetEventNotificationMode(JVMTI_ENABLE,
                                            e,
                                            nullptr /* all threads */);
    if (error != JVMTI_ERROR_NONE) {
      LOG(ERROR) << "Unable to enable event " << e;
      return JNI_ERR;
    }
  }
  return JNI_OK;
}

// Late attachment (e.g. 'am attach-agent').
extern "C" JNIEXPORT jint JNICALL Agent_OnAttach(JavaVM *vm, char* options, void* reserved) {
  return AgentStart(vm, options, reserved);
}

// Early attachment
extern "C" JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM* jvm, char* options, void* reserved) {
  return AgentStart(jvm, options, reserved);
}

}  // namespace tifast

