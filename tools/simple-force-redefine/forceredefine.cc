// Copyright (C) 2019 The Android Open Source Project
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

#include "__mutex_base"
#include <cstddef>
#include <fcntl.h>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <unistd.h>
#include <unordered_set>

#include <android-base/logging.h>
#include <android-base/macros.h>

#include <nativehelper/scoped_local_ref.h>

#include <jni.h>
#include <jvmti.h>

// Slicer's headers have code that triggers these warnings. b/65298177
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wsign-compare"
#include <slicer/code_ir.h>
#include <slicer/dex_bytecode.h>
#include <slicer/dex_ir.h>
#include <slicer/dex_ir_builder.h>
#include <slicer/reader.h>
#include <slicer/writer.h>
#pragma clang diagnostic pop

namespace forceredefine {

namespace {

struct AgentInfo {
  std::fstream stream;
  std::unordered_set<std::string> classes;
  std::mutex mutex;
};

// Converts a class name to a type descriptor
// (ex. "java.lang.String" to "Ljava/lang/String;")
std::string classNameToDescriptor(const char* className) {
  std::stringstream ss;
  ss << "L";
  for (auto p = className; *p != '\0'; ++p) {
    ss << (*p == '.' ? '/' : *p);
  }
  ss << ";";
  return ss.str();
}

// Converts a descriptor (Lthis/style/of/name;) to a jni-FindClass style Fully-qualified class name
// (this/style/of/name).
std::string DescriptorToFQCN(const std::string& descriptor) {
  return descriptor.substr(1, descriptor.size() - 2);
}

static AgentInfo* GetAgentInfo(jvmtiEnv* jvmti) {
  AgentInfo* ai = nullptr;
  CHECK_EQ(jvmti->GetEnvironmentLocalStorage(reinterpret_cast<void**>(&ai)), JVMTI_ERROR_NONE);
  CHECK(ai != nullptr);
  return ai;
}

class JvmtiAllocator : public dex::Writer::Allocator {
 public:
  explicit JvmtiAllocator(jvmtiEnv* jvmti) : jvmti_(jvmti) {}
  void* Allocate(size_t size) override {
    unsigned char* res = nullptr;
    jvmti_->Allocate(size, &res);
    return res;
  }
  void Free(void* ptr) override {
    jvmti_->Deallocate(reinterpret_cast<unsigned char*>(ptr));
  }

 private:
  jvmtiEnv* jvmti_;
};

static void Transform(std::shared_ptr<ir::DexFile> ir) {
  std::unique_ptr<ir::Builder> builder;
  for (auto& method : ir->encoded_methods) {
    // Do not look into abstract/bridge/native/synthetic methods.
    if ((method->access_flags &
         (dex::kAccAbstract | dex::kAccBridge | dex::kAccNative | dex::kAccSynthetic)) != 0) {
      continue;
    }

    struct AddNopVisitor : public lir::Visitor {
      explicit AddNopVisitor(lir::CodeIr* cir) : cir_(cir) {}

      bool Visit(lir::Bytecode* bc) override {
        if (seen_first_inst) {
          return false;
        }
        seen_first_inst = true;
        auto new_inst = cir_->Alloc<lir::Bytecode>();
        new_inst->opcode = dex::OP_NOP;
        cir_->instructions.InsertBefore(bc, new_inst);
        return true;
      }

      lir::CodeIr* cir_;
      bool seen_first_inst = false;
    };

    lir::CodeIr c(method.get(), ir);
    AddNopVisitor visitor(&c);
    for (auto it = c.instructions.begin(); it != c.instructions.end(); ++it) {
      lir::Instruction* fi = *it;
      if (fi->Accept(&visitor)) {
        break;
      }
    }
    c.Assemble();
  }
}

static void CbClassFileLoadHook(jvmtiEnv* jvmti,
                                JNIEnv* env ATTRIBUTE_UNUSED,
                                jclass classBeingRedefined ATTRIBUTE_UNUSED,
                                jobject loader ATTRIBUTE_UNUSED,
                                const char* name,
                                jobject protectionDomain ATTRIBUTE_UNUSED,
                                jint classDataLen,
                                const unsigned char* classData,
                                jint* newClassDataLen,
                                unsigned char** newClassData) {
  std::string desc(classNameToDescriptor(name));
  std::string fqcn(DescriptorToFQCN(desc));
  AgentInfo* ai = GetAgentInfo(jvmti);
  {
    std::lock_guard<std::mutex> mu(ai->mutex);
    if (ai->classes.find(fqcn) == ai->classes.end()) {
      return;
    }
  }
  LOG(INFO) << "Got CFLH for " << name << " on env " << static_cast<void*>(jvmti);
  JvmtiAllocator allocator(jvmti);
  dex::Reader reader(classData, classDataLen);
  dex::u4 index = reader.FindClassIndex(desc.c_str());
  reader.CreateClassIr(index);
  std::shared_ptr<ir::DexFile> ir(reader.GetIr());
  Transform(ir);
  dex::Writer writer(ir);
  size_t new_size;
  *newClassData = writer.CreateImage(&allocator, &new_size);
  *newClassDataLen = new_size;
}

static jclass FindClass(jvmtiEnv* jvmti, JNIEnv* env, const std::string& name) {
  jclass res = env->FindClass(name.c_str());
  if (res != nullptr) {
    return res;
  }
  ScopedLocalRef<jthrowable> exc(env, env->ExceptionOccurred());
  env->ExceptionClear();
  // Try to find it in other classloaders.
  env->PushLocalFrame(1 << 18);
  do {
    jint cnt;
    jclass* klasses;
    if (jvmti->GetLoadedClasses(&cnt, &klasses) != JVMTI_ERROR_NONE) {
      LOG(ERROR) << "Unable to get loaded classes!";
      break;
    }
    for (jint i = 0; i < cnt; i++) {
      char* sig;
      if (jvmti->GetClassSignature(klasses[i], &sig, nullptr) != JVMTI_ERROR_NONE) {
        continue;
      }
      if (sig[0] == 'L' && DescriptorToFQCN(sig) == name) {
        res = klasses[i];
        break;
      }
    }
    jvmti->Deallocate(reinterpret_cast<unsigned char*>(klasses));
  } while (false);
  res = reinterpret_cast<jclass>(env->PopLocalFrame(res));
  if (res == nullptr && exc.get() != nullptr) {
    env->Throw(exc.get());
  }
  return res;
}

static void RedefineClass(jvmtiEnv* jvmti, JNIEnv* env, const std::string& klass_name) {
  jclass klass = nullptr;
  if ((klass = FindClass(jvmti, env, klass_name)) == nullptr) {
    LOG(WARNING) << "Failed to find class for " << klass_name;
    env->ExceptionDescribe();
    env->ExceptionClear();
    return;
  }
  jvmti->RetransformClasses(1, &klass);
  env->DeleteLocalRef(klass);
}

static void AgentMain(jvmtiEnv* jvmti, JNIEnv* jni, void* arg ATTRIBUTE_UNUSED) {
  AgentInfo* ai = GetAgentInfo(jvmti);
  std::string klass_name;
  jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, nullptr);
  // TODO Replace this with something that can read from a fifo and ignore the 'EOF's.
  while (std::getline(ai->stream, klass_name, '\n')) {
    LOG(INFO) << "Redefining class " << klass_name << " with " << static_cast<void*>(jvmti);
    {
      std::lock_guard<std::mutex> mu(ai->mutex);
      ai->classes.insert(klass_name);
    }
    RedefineClass(jvmti, jni, klass_name);
  }
}

static void CbVmInit(jvmtiEnv* jvmti, JNIEnv* env, jthread thr ATTRIBUTE_UNUSED) {
  // Create a Thread object.
  ScopedLocalRef<jobject> thread_name(env, env->NewStringUTF("Agent Thread"));
  if (thread_name.get() == nullptr) {
    env->ExceptionDescribe();
    env->ExceptionClear();
    return;
  }
  ScopedLocalRef<jclass> thread_klass(env, env->FindClass("java/lang/Thread"));
  if (thread_klass.get() == nullptr) {
    env->ExceptionDescribe();
    env->ExceptionClear();
    return;
  }
  ScopedLocalRef<jobject> thread(env, env->AllocObject(thread_klass.get()));
  if (thread.get() == nullptr) {
    env->ExceptionDescribe();
    env->ExceptionClear();
    return;
  }

  env->CallNonvirtualVoidMethod(
      thread.get(),
      thread_klass.get(),
      env->GetMethodID(thread_klass.get(), "<init>", "(Ljava/lang/String;)V"),
      thread_name.get());
  env->CallVoidMethod(thread.get(), env->GetMethodID(thread_klass.get(), "setPriority", "(I)V"), 1);
  env->CallVoidMethod(
      thread.get(), env->GetMethodID(thread_klass.get(), "setDaemon", "(Z)V"), JNI_TRUE);

  jvmti->RunAgentThread(thread.get(), AgentMain, nullptr, JVMTI_THREAD_MIN_PRIORITY);
}

}  // namespace

template <bool kIsOnLoad>
static jint AgentStart(JavaVM* vm, char* options, void* reserved ATTRIBUTE_UNUSED) {
  jvmtiEnv* jvmti = nullptr;

  if (vm->GetEnv(reinterpret_cast<void**>(&jvmti), JVMTI_VERSION_1_1) != JNI_OK ||
      jvmti == nullptr) {
    LOG(ERROR) << "unable to obtain JVMTI env.";
    return JNI_ERR;
  }
  std::string sopts(options);
  AgentInfo* ai = new AgentInfo;
  ai->stream.open(options, std::ios_base::in);
  if (!ai->stream.is_open()) {
    PLOG(ERROR) << "Could not open file " << options << " for triggering class-reload";
    return JNI_ERR;
  }

  jvmtiCapabilities caps{
    .can_retransform_classes = 1,
  };
  if (jvmti->AddCapabilities(&caps) != JVMTI_ERROR_NONE) {
    LOG(ERROR) << "Unable to get retransform_classes capability!";
    return JNI_ERR;
  }
  jvmtiEventCallbacks cb{
    .ClassFileLoadHook = CbClassFileLoadHook,
    .VMInit = CbVmInit,
  };
  jvmti->SetEventCallbacks(&cb, sizeof(cb));
  jvmti->SetEnvironmentLocalStorage(reinterpret_cast<void*>(ai));
  if (kIsOnLoad) {
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_INIT, nullptr);
  } else {
    JNIEnv* jni = nullptr;
    vm->GetEnv(reinterpret_cast<void**>(&jni), JNI_VERSION_1_2);
    jthread thr;
    jvmti->GetCurrentThread(&thr);
    CbVmInit(jvmti, jni, thr);
  }
  return JNI_OK;
}

// Late attachment (e.g. 'am attach-agent').
extern "C" JNIEXPORT jint JNICALL Agent_OnAttach(JavaVM* vm, char* options, void* reserved) {
  return AgentStart<false>(vm, options, reserved);
}

// Early attachment
extern "C" JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM* jvm, char* options, void* reserved) {
  return AgentStart<true>(jvm, options, reserved);
}

}  // namespace forceredefine
