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

template <typename ...Args> static void Unused(Args... args ATTRIBUTE_UNUSED) {}

// jthread is a typedef of jobject so we use this to allow the templates to distinguish them.
struct jthreadContainer { jthread thread; };
// jlocation is a typedef of jlong so use this to distinguish the less common jlong.
struct jlongContainer { jlong val; };

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
#define FOR_ALL_SUPPORTED_JNI_EVENTS(fun) \
    fun(SingleStep, EVENT(SINGLE_STEP), (jvmtiEnv* jvmti, JNIEnv* jni, jthread thread, jmethodID meth, jlocation loc), (jvmti, jni, jthreadContainer{.thread = thread}, meth, loc)) \
    fun(MethodEntry, EVENT(METHOD_ENTRY), (jvmtiEnv* jvmti, JNIEnv* jni, jthread thread, jmethodID meth), (jvmti, jni, jthreadContainer{.thread = thread}, meth)) \
    fun(MethodExit, EVENT(METHOD_EXIT), (jvmtiEnv* jvmti, JNIEnv* jni, jthread thread, jmethodID meth, jboolean jb, jvalue jv), (jvmti, jni, jthreadContainer{.thread = thread}, meth, jb, jv)) \
    fun(NativeMethodBind, EVENT(NATIVE_METHOD_BIND), (jvmtiEnv* jvmti, JNIEnv* jni, jthread thread, jmethodID meth, void* v1, void** v2), (jvmti, jni, jthreadContainer{.thread = thread}, meth, v1, v2)) \
    fun(Exception, EVENT(EXCEPTION), (jvmtiEnv* jvmti, JNIEnv* jni, jthread thread, jmethodID meth1, jlocation loc1, jobject obj, jmethodID meth2, jlocation loc2), (jvmti, jni, jthreadContainer{.thread = thread}, meth1, loc1, obj, meth2, loc2)) \
    fun(ExceptionCatch, EVENT(EXCEPTION_CATCH), (jvmtiEnv* jvmti, JNIEnv* jni, jthread thread, jmethodID meth, jlocation loc, jobject obj), (jvmti, jni, jthreadContainer{.thread = thread}, meth, loc, obj)) \
    fun(ThreadStart, EVENT(THREAD_START), (jvmtiEnv* jvmti, JNIEnv* jni, jthread thread), (jvmti, jni, jthreadContainer{.thread = thread})) \
    fun(ThreadEnd, EVENT(THREAD_END), (jvmtiEnv* jvmti, JNIEnv* jni, jthread thread), (jvmti, jni, jthreadContainer{.thread = thread})) \
    fun(ClassLoad, EVENT(CLASS_LOAD), (jvmtiEnv* jvmti, JNIEnv* jni, jthread thread, jclass klass), (jvmti, jni, jthreadContainer{.thread = thread}, klass) ) \
    fun(ClassPrepare, EVENT(CLASS_PREPARE), (jvmtiEnv* jvmti, JNIEnv* jni, jthread thread, jclass klass), (jvmti, jni, jthreadContainer{.thread = thread}, klass)) \
    fun(ClassFileLoadHook, EVENT(CLASS_FILE_LOAD_HOOK), (jvmtiEnv* jvmti, JNIEnv* jni, jclass klass, jobject obj1, const char* c1, jobject obj2, jint i1, const unsigned char* c2, jint* ip1, unsigned char** cp1), (jvmti, jni, klass, obj1, c1, obj2, i1, c2, ip1, cp1)) \
    fun(MonitorContendedEnter, EVENT(MONITOR_CONTENDED_ENTER), (jvmtiEnv* jvmti, JNIEnv* jni, jthread thread, jobject obj), (jvmti, jni, jthreadContainer{.thread = thread}, obj)) \
    fun(MonitorContendedEntered, EVENT(MONITOR_CONTENDED_ENTERED), (jvmtiEnv* jvmti, JNIEnv* jni, jthread thread, jobject obj), (jvmti, jni, jthreadContainer{.thread = thread}, obj)) \
    fun(MonitorWait, EVENT(MONITOR_WAIT), (jvmtiEnv* jvmti, JNIEnv* jni, jthread thread, jobject obj, jlong l1), (jvmti, jni, jthreadContainer{.thread = thread}, obj, jlongContainer{.val = l1})) \
    fun(MonitorWaited, EVENT(MONITOR_WAITED), (jvmtiEnv* jvmti, JNIEnv* jni, jthread thread, jobject obj, jboolean b1), (jvmti, jni, jthreadContainer{.thread = thread}, obj, b1)) \
    fun(ResourceExhausted, EVENT(RESOURCE_EXHAUSTED), (jvmtiEnv* jvmti, JNIEnv* jni, jint i1, const void* cv, const char* cc), (jvmti, jni, i1, cv, cc)) \
    fun(VMObjectAlloc, EVENT(VM_OBJECT_ALLOC), (jvmtiEnv* jvmti, JNIEnv* jni, jthread thread, jobject obj, jclass klass, jlong l1), (jvmti, jni, jthreadContainer{.thread = thread}, obj, klass, jlongContainer{.val = l1})) \

#define FOR_ALL_SUPPORTED_NO_JNI_EVENTS(fun) \
    fun(CompiledMethodLoad, EVENT(COMPILED_METHOD_LOAD), (jvmtiEnv* jvmti, jmethodID meth, jint i1, const void* cv1, jint i2, const jvmtiAddrLocationMap* alm, const void* cv2), (jvmti, meth, i1, cv1, i2, alm, cv2)) \
    fun(CompiledMethodUnload, EVENT(COMPILED_METHOD_UNLOAD), (jvmtiEnv* jvmti, jmethodID meth, const void* cv1), (jvmti, meth, cv1)) \
    fun(DynamicCodeGenerated, EVENT(DYNAMIC_CODE_GENERATED), (jvmtiEnv* jvmti, const char* cc, const void* cv, jint i1), (jvmti, cc, cv, i1)) \
    fun(DataDumpRequest, EVENT(DATA_DUMP_REQUEST), (jvmtiEnv* jvmti), (jvmti)) \
    fun(GarbageCollectionStart, EVENT(GARBAGE_COLLECTION_START), (jvmtiEnv* jvmti), (jvmti)) \
    fun(GarbageCollectionFinish, EVENT(GARBAGE_COLLECTION_FINISH), (jvmtiEnv* jvmti), (jvmti))

#define FOR_ALL_SUPPORTED_EVENTS(fun) \
    FOR_ALL_SUPPORTED_JNI_EVENTS(fun) \
    FOR_ALL_SUPPORTED_NO_JNI_EVENTS(fun)

static const jvmtiEvent kAllEvents[] = {
#define GET_EVENT(a, event, b, c) event,
FOR_ALL_SUPPORTED_EVENTS(GET_EVENT)
#undef GET_EVENT
};

#define GENERATE_EMPTY_FUNCTION(name, number, args, argnames) \
    static void JNICALL empty ## name  args { Unused argnames ; }
FOR_ALL_SUPPORTED_EVENTS(GENERATE_EMPTY_FUNCTION)
#undef GENERATE_EMPTY_FUNCTION

static jvmtiEventCallbacks kEmptyCallbacks {
#define CREATE_EMPTY_EVENT_CALLBACKS(name, num, args, argnames) \
    .name = empty ## name,
  FOR_ALL_SUPPORTED_EVENTS(CREATE_EMPTY_EVENT_CALLBACKS)
#undef CREATE_EMPTY_EVENT_CALLBACKS
};

static void DeleteLocalRef(JNIEnv* env, jobject obj) {
  if (obj != nullptr && env != nullptr) {
    env->DeleteLocalRef(obj);
  }
}

class ScopedThreadInfo {
 public:
  ScopedThreadInfo(jvmtiEnv* jvmtienv, JNIEnv* env, jthread thread)
      : jvmtienv_(jvmtienv), env_(env), free_name_(false) {
    if (thread == nullptr) {
      info_.name = const_cast<char*>("<NULLPTR>");
    } else if (jvmtienv->GetThreadInfo(thread, &info_) != JVMTI_ERROR_NONE) {
      info_.name = const_cast<char*>("<UNKNOWN THREAD>");
    } else {
      free_name_ = true;
    }
  }

  ~ScopedThreadInfo() {
    if (free_name_) {
      jvmtienv_->Deallocate(reinterpret_cast<unsigned char*>(info_.name));
    }
    DeleteLocalRef(env_, info_.thread_group);
    DeleteLocalRef(env_, info_.context_class_loader);
  }

  const char* GetName() const {
    return info_.name;
  }

 private:
  jvmtiEnv* jvmtienv_;
  JNIEnv* env_;
  bool free_name_;
  jvmtiThreadInfo info_{};
};

class ScopedClassInfo {
 public:
  ScopedClassInfo(jvmtiEnv* jvmtienv, jclass c) : jvmtienv_(jvmtienv), class_(c) {}

  ~ScopedClassInfo() {
    if (class_ != nullptr) {
      jvmtienv_->Deallocate(reinterpret_cast<unsigned char*>(name_));
      jvmtienv_->Deallocate(reinterpret_cast<unsigned char*>(generic_));
      jvmtienv_->Deallocate(reinterpret_cast<unsigned char*>(file_));
      jvmtienv_->Deallocate(reinterpret_cast<unsigned char*>(debug_ext_));
    }
  }

  bool Init(bool get_generic = true) {
    if (class_ == nullptr) {
      name_ = const_cast<char*>("<NONE>");
      generic_ = const_cast<char*>("<NONE>");
      return true;
    } else {
      jvmtiError ret1 = jvmtienv_->GetSourceFileName(class_, &file_);
      jvmtiError ret2 = jvmtienv_->GetSourceDebugExtension(class_, &debug_ext_);
      char** gen_ptr = &generic_;
      if (!get_generic) {
        generic_ = nullptr;
        gen_ptr = nullptr;
      }
      return jvmtienv_->GetClassSignature(class_, &name_, gen_ptr) == JVMTI_ERROR_NONE &&
          ret1 != JVMTI_ERROR_MUST_POSSESS_CAPABILITY &&
          ret1 != JVMTI_ERROR_INVALID_CLASS &&
          ret2 != JVMTI_ERROR_MUST_POSSESS_CAPABILITY &&
          ret2 != JVMTI_ERROR_INVALID_CLASS;
    }
  }

  jclass GetClass() const {
    return class_;
  }

  const char* GetName() const {
    return name_;
  }

  const char* GetGeneric() const {
    return generic_;
  }

  const char* GetSourceDebugExtension() const {
    if (debug_ext_ == nullptr) {
      return "<UNKNOWN_SOURCE_DEBUG_EXTENSION>";
    } else {
      return debug_ext_;
    }
  }
  const char* GetSourceFileName() const {
    if (file_ == nullptr) {
      return "<UNKNOWN_FILE>";
    } else {
      return file_;
    }
  }

 private:
  jvmtiEnv* jvmtienv_;
  jclass class_;
  char* name_ = nullptr;
  char* generic_ = nullptr;
  char* file_ = nullptr;
  char* debug_ext_ = nullptr;

  friend std::ostream& operator<<(std::ostream &os, ScopedClassInfo const& m);
};

class ScopedMethodInfo {
 public:
  ScopedMethodInfo(jvmtiEnv* jvmtienv, JNIEnv* env, jmethodID m)
      : jvmtienv_(jvmtienv), env_(env), method_(m) {}

  ~ScopedMethodInfo() {
    DeleteLocalRef(env_, declaring_class_);
    jvmtienv_->Deallocate(reinterpret_cast<unsigned char*>(name_));
    jvmtienv_->Deallocate(reinterpret_cast<unsigned char*>(signature_));
    jvmtienv_->Deallocate(reinterpret_cast<unsigned char*>(generic_));
  }

  bool Init(bool get_generic = true) {
    if (jvmtienv_->GetMethodDeclaringClass(method_, &declaring_class_) != JVMTI_ERROR_NONE) {
      return false;
    }
    class_info_.reset(new ScopedClassInfo(jvmtienv_, declaring_class_));
    jint nlines;
    jvmtiLineNumberEntry* lines;
    jvmtiError err = jvmtienv_->GetLineNumberTable(method_, &nlines, &lines);
    if (err == JVMTI_ERROR_NONE) {
      if (nlines > 0) {
        first_line_ = lines[0].line_number;
      }
      jvmtienv_->Deallocate(reinterpret_cast<unsigned char*>(lines));
    } else if (err != JVMTI_ERROR_ABSENT_INFORMATION &&
               err != JVMTI_ERROR_NATIVE_METHOD) {
      return false;
    }
    return class_info_->Init(get_generic) &&
        (jvmtienv_->GetMethodName(method_, &name_, &signature_, &generic_) == JVMTI_ERROR_NONE);
  }

  const ScopedClassInfo& GetDeclaringClassInfo() const {
    return *class_info_;
  }

  jclass GetDeclaringClass() const {
    return declaring_class_;
  }

  const char* GetName() const {
    return name_;
  }

  const char* GetSignature() const {
    return signature_;
  }

  const char* GetGeneric() const {
    return generic_;
  }

  jint GetFirstLine() const {
    return first_line_;
  }

 private:
  jvmtiEnv* jvmtienv_;
  JNIEnv* env_;
  jmethodID method_;
  jclass declaring_class_ = nullptr;
  std::unique_ptr<ScopedClassInfo> class_info_;
  char* name_ = nullptr;
  char* signature_ = nullptr;
  char* generic_ = nullptr;
  jint first_line_ = -1;

  friend std::ostream& operator<<(std::ostream &os, ScopedMethodInfo const& m);
};

std::ostream& operator<<(std::ostream &os, ScopedClassInfo const& c) {
  const char* generic = c.GetGeneric();
  if (generic != nullptr) {
    return os << c.GetName() << "<" << generic << ">" << " file: " << c.GetSourceFileName();
  } else {
    return os << c.GetName() << " file: " << c.GetSourceFileName();
  }
}

std::ostream& operator<<(std::ostream &os, ScopedMethodInfo const& m) {
  return os << m.GetDeclaringClassInfo().GetName() << "->" << m.GetName() << m.GetSignature()
            << " (source: " << m.GetDeclaringClassInfo().GetSourceFileName() << ":"
            << m.GetFirstLine() << ")";
}


class LogPrinter {
 public:
  explicit LogPrinter(jvmtiEvent event) : event_(event) {}

  template <typename ...Args> void PrintRestNoJNI(jvmtiEnv* jvmti, Args... args) {
    PrintRest(jvmti, static_cast<JNIEnv*>(nullptr), args...);
  }

  template <typename ...Args> void PrintRest(jvmtiEnv* jvmti, JNIEnv* env, Args... args);
  template <typename ...Args> void PrintRest(jvmtiEnv* jvmti,
                                             JNIEnv* env,
                                             jlongContainer l,
                                             Args... args);
  template <typename ...Args> void PrintRest(jvmtiEnv* jvmti,
                                             JNIEnv* env,
                                             jthreadContainer thr,
                                             Args... args);
  template <typename ...Args> void PrintRest(jvmtiEnv* jvmti,
                                             JNIEnv* env,
                                             jboolean i,
                                             Args... args);
  template <typename ...Args> void PrintRest(jvmtiEnv* jvmti,
                                             JNIEnv* env,
                                             jint i,
                                             Args... args);
  template <typename ...Args> void PrintRest(jvmtiEnv* jvmti,
                                             JNIEnv* env,
                                             jclass klass,
                                             Args... args);
  template <typename ...Args> void PrintRest(jvmtiEnv* jvmti,
                                             JNIEnv* env,
                                             jmethodID meth,
                                             Args... args);
  template <typename ...Args> void PrintRest(jvmtiEnv* jvmti,
                                             JNIEnv* env,
                                             jlocation loc,
                                             Args... args);
  template <typename ...Args> void PrintRest(jvmtiEnv* jvmti,
                                             JNIEnv* env,
                                             jint* ip,
                                             Args... args);
  template <typename ...Args> void PrintRest(jvmtiEnv* jvmti,
                                             JNIEnv* env,
                                             const void* loc,
                                             Args... args);
  template <typename ...Args> void PrintRest(jvmtiEnv* jvmti,
                                             JNIEnv* env,
                                             void* loc,
                                             Args... args);
  template <typename ...Args> void PrintRest(jvmtiEnv* jvmti,
                                             JNIEnv* env,
                                             void** loc,
                                             Args... args);
  template <typename ...Args> void PrintRest(jvmtiEnv* jvmti,
                                             JNIEnv* env,
                                             unsigned char** v,
                                             Args... args);
  template <typename ...Args> void PrintRest(jvmtiEnv* jvmti,
                                             JNIEnv* env,
                                             const unsigned char* v,
                                             Args... args);
  template <typename ...Args> void PrintRest(jvmtiEnv* jvmti,
                                             JNIEnv* env,
                                             const char* v,
                                             Args... args);
  template <typename ...Args> void PrintRest(jvmtiEnv* jvmti,
                                             JNIEnv* env,
                                             const jvmtiAddrLocationMap* v,
                                             Args... args);
  template <typename ...Args> void PrintRest(jvmtiEnv* jvmti,
                                             JNIEnv* env,
                                             jvalue v,
                                             Args... args);
  template <typename ...Args> void PrintRest(jvmtiEnv* jvmti,
                                             JNIEnv* env,
                                             jobject v,
                                             Args... args);

  std::string GetResult() {
    std::string out_str = stream.str();
    return start_args + out_str;
  }

 private:
  jvmtiEvent event_;
  std::string start_args;
  std::ostringstream stream;
};

// Base case
template<> void LogPrinter::PrintRest(jvmtiEnv* jvmti ATTRIBUTE_UNUSED, JNIEnv* jni) {
  if (jni == nullptr) {
    start_args = "jvmtiEnv*";
  } else {
    start_args = "jvmtiEnv*, JNIEnv*";
  }
}

template<typename ...Args>
void LogPrinter::PrintRest(jvmtiEnv* jvmti,
                           JNIEnv* jni,
                           const jvmtiAddrLocationMap* v,
                           Args... args) {
  if (v != nullptr) {
    stream << ", const jvmtiAddrLocationMap*[start_address: "
           << v->start_address << ", location: " << v->location << "]";
  } else {
    stream << ", const jvmtiAddrLocationMap*[nullptr]";
  }
  PrintRest(jvmti, jni, args...);
}

template<typename ...Args>
void LogPrinter::PrintRest(jvmtiEnv* jvmti, JNIEnv* jni, jint* v, Args... args) {
  stream << ", jint*[" << static_cast<const void*>(v) << "]";
  PrintRest(jvmti, jni, args...);
}

template<typename ...Args>
void LogPrinter::PrintRest(jvmtiEnv* jvmti, JNIEnv* jni, const void* v, Args... args) {
  stream << ", const void*[" << v << "]";
  PrintRest(jvmti, jni, args...);
}

template<typename ...Args>
void LogPrinter::PrintRest(jvmtiEnv* jvmti, JNIEnv* jni, unsigned char** v, Args... args) {
  stream << ", unsigned char**[" << static_cast<const void*>(v) << "]";
  PrintRest(jvmti, jni, args...);
}

template<typename ...Args>
void LogPrinter::PrintRest(jvmtiEnv* jvmti, JNIEnv* jni, const unsigned char* v, Args... args) {
  stream << ", const unsigned char*[" << static_cast<const void*>(v) << "]";
  PrintRest(jvmti, jni, args...);
}

template<typename ...Args>
void LogPrinter::PrintRest(jvmtiEnv* jvmti, JNIEnv* jni, const char* v, Args... args) {
  stream << ", const char*[" << v << "]";
  PrintRest(jvmti, jni, args...);
}

template<typename ...Args>
void LogPrinter::PrintRest(jvmtiEnv* jvmti, JNIEnv* jni, jvalue v ATTRIBUTE_UNUSED, Args... args) {
  stream << ", jvalue[<UNION>]";
  PrintRest(jvmti, jni, args...);
}

template<typename ...Args>
void LogPrinter::PrintRest(jvmtiEnv* jvmti, JNIEnv* jni, void** v, Args... args) {
  stream << ", void**[" << v << "]";
  PrintRest(jvmti, jni, args...);
}

template<typename ...Args>
void LogPrinter::PrintRest(jvmtiEnv* jvmti, JNIEnv* jni, void* v, Args... args) {
  stream << ", void*[" << v << "]";
  PrintRest(jvmti, jni, args...);
}

template<typename ...Args>
void LogPrinter::PrintRest(jvmtiEnv* jvmti, JNIEnv* jni, jlongContainer l, Args... args) {
  stream << ", jlong[" << l.val << ", hex: 0x" << std::hex << l.val << "]";
  PrintRest(jvmti, jni, args...);
}

template<typename ...Args>
void LogPrinter::PrintRest(jvmtiEnv* jvmti, JNIEnv* jni, jlocation l, Args... args) {
  stream << ", jlocation[" << l << ", hex: 0x" << std::hex << l << "]";
  PrintRest(jvmti, jni, args...);
}

template<typename ...Args>
void LogPrinter::PrintRest(jvmtiEnv* jvmti, JNIEnv* jni, jboolean b, Args... args) {
  stream << ", jboolean[" << (b ? "true" : "false") << "]";
  PrintRest(jvmti, jni, args...);
}

template<typename ...Args>
void LogPrinter::PrintRest(jvmtiEnv* jvmti, JNIEnv* jni, jint i, Args... args) {
  stream << ", jint[" << i << ", hex: 0x" << std::hex << i << "]";
  PrintRest(jvmti, jni, args...);
}

template<typename ...Args>
void LogPrinter::PrintRest(jvmtiEnv* jvmti, JNIEnv* jni, jobject obj, Args... args) {
  if (obj == nullptr) {
    stream << ", jobject[nullptr]";
  } else {
    jni->PushLocalFrame(1);
    jclass klass = jni->GetObjectClass(obj);
    ScopedClassInfo sci(jvmti, klass);
    if (sci.Init(event_ != JVMTI_EVENT_VM_OBJECT_ALLOC)) {
      stream << ", jobject[type: " << sci << "]";
    } else {
      stream << ", jobject[type: TYPE UNKNOWN]";
    }
    jni->PopLocalFrame(nullptr);
  }
  PrintRest(jvmti, jni, args...);
}

template<typename ...Args>
void LogPrinter::PrintRest(jvmtiEnv* jvmti, JNIEnv* jni, jthreadContainer thr, Args... args) {
  ScopedThreadInfo sti(jvmti, jni, thr.thread);
  stream << ", jthread[" << sti.GetName() << "]";
  PrintRest(jvmti, jni, args...);
}

template<typename ...Args>
void LogPrinter::PrintRest(jvmtiEnv* jvmti, JNIEnv* jni, jclass klass, Args... args) {
  ScopedClassInfo sci(jvmti, klass);
  if (sci.Init(/*get_generic=*/event_ != JVMTI_EVENT_VM_OBJECT_ALLOC)) {
    stream << ", jclass[" << sci << "]";
  } else {
    stream << ", jclass[TYPE UNKNOWN]";
  }
  PrintRest(jvmti, jni, args...);
}

template<typename ...Args>
void LogPrinter::PrintRest(jvmtiEnv* jvmti, JNIEnv* jni, jmethodID meth, Args... args) {
  ScopedMethodInfo smi(jvmti, jni, meth);
  if (smi.Init()) {
    stream << ", jmethodID[" << smi << "]";
  } else {
    stream << ", jmethodID[METHOD UNKNOWN]";
  }
  PrintRest(jvmti, jni, args...);
}

#define GENERATE_LOG_FUNCTION_JNI(name, event, args, argnames) \
    static void JNICALL log ## name  args { \
      LogPrinter printer(event); \
      printer.PrintRest argnames; \
      LOG(INFO) << "Got event " << #name << "(" << printer.GetResult() << ")"; \
    } \

#define GENERATE_LOG_FUNCTION_NO_JNI(name, event, args, argnames) \
    static void JNICALL log ## name  args { \
      LogPrinter printer(event); \
      printer.PrintRestNoJNI argnames; \
      LOG(INFO) << "Got event " << #name << "(" << printer.GetResult() << ")"; \
    } \

FOR_ALL_SUPPORTED_JNI_EVENTS(GENERATE_LOG_FUNCTION_JNI)
FOR_ALL_SUPPORTED_NO_JNI_EVENTS(GENERATE_LOG_FUNCTION_NO_JNI)
#undef GENERATE_LOG_FUNCTION

static jvmtiEventCallbacks kLogCallbacks {
#define CREATE_LOG_EVENT_CALLBACK(name, num, args, argnames) \
    .name = log ## name,
  FOR_ALL_SUPPORTED_EVENTS(CREATE_LOG_EVENT_CALLBACK)
#undef CREATE_LOG_EVENT_CALLBACK
};

static std::string EventToName(jvmtiEvent desired_event) {
#define CHECK_NAME(name, event, args, argnames) \
  if (desired_event == event) { \
    return #name; \
  }
  FOR_ALL_SUPPORTED_EVENTS(CHECK_NAME);
  LOG(FATAL) << "Unknown event " << desired_event;
  __builtin_unreachable();
#undef CHECK_NAME
}
static jvmtiEvent NameToEvent(const std::string& desired_name) {
#define CHECK_NAME(name, event, args, argnames) \
  if (desired_name == #name) { \
    return event; \
  }
  FOR_ALL_SUPPORTED_EVENTS(CHECK_NAME);
  LOG(FATAL) << "Unknown event " << desired_name;
  __builtin_unreachable();
#undef CHECK_NAME
}

#undef FOR_ALL_SUPPORTED_JNI_EVENTS
#undef FOR_ALL_SUPPORTED_NO_JNI_EVENTS
#undef FOR_ALL_SUPPORTED_EVENTS

static std::vector<jvmtiEvent> GetAllAvailableEvents(jvmtiEnv* jvmti) {
  std::vector<jvmtiEvent> out;
  jvmtiCapabilities caps{};
  jvmti->GetPotentialCapabilities(&caps);
  uint8_t caps_bytes[sizeof(caps)];
  memcpy(caps_bytes, &caps, sizeof(caps));
  for (jvmtiEvent e : kAllEvents) {
    jvmtiCapabilities req{};
    AddCapsForEvent(e, &req);
    uint8_t req_bytes[sizeof(req)];
    memcpy(req_bytes, &req, sizeof(req));
    bool good = true;
    for (size_t i = 0; i < sizeof(caps); i++) {
      if ((req_bytes[i] & caps_bytes[i]) != req_bytes[i]) {
        good = false;
        break;
      }
    }
    if (good) {
      out.push_back(e);
    } else {
      LOG(WARNING) << "Unable to get capabilities for event " << EventToName(e);
    }
  }
  return out;
}

static std::vector<jvmtiEvent> GetRequestedEventList(jvmtiEnv* jvmti, const std::string& args) {
  std::vector<jvmtiEvent> res;
  std::stringstream args_stream(args);
  std::string item;
  while (std::getline(args_stream, item, ',')) {
    if (item == "") {
      continue;
    } else if (item == "all") {
      return GetAllAvailableEvents(jvmti);
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

  std::vector<jvmtiEvent> events = GetRequestedEventList(jvmti, args);

  jvmtiCapabilities caps{};
  for (jvmtiEvent e : events) {
    AddCapsForEvent(e, &caps);
  }
  if (is_log) {
    caps.can_get_line_numbers = 1;
    caps.can_get_source_file_name = 1;
    caps.can_get_source_debug_extension = 1;
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

