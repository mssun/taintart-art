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
#include <iomanip>
#include <iostream>
#include <istream>
#include <jni.h>
#include <jvmti.h>
#include <memory>
#include <sstream>
#include <string.h>
#include <string>
#include <vector>

namespace fieldnull {

#define CHECK_JVMTI(x) CHECK_EQ((x), JVMTI_ERROR_NONE)

// Special art ti-version number. We will use this as a fallback if we cannot get a regular JVMTI
// env.
static constexpr jint kArtTiVersion = JVMTI_VERSION_1_2 | 0x40000000;

static JavaVM* java_vm = nullptr;

// Field is "Lclass/name/here;.field_name:Lfield/type/here;"
static std::pair<jclass, jfieldID> SplitField(JNIEnv* env, const std::string& field_id) {
  CHECK_EQ(field_id[0], 'L');
  env->PushLocalFrame(1);
  std::istringstream is(field_id);
  std::string class_name;
  std::string field_name;
  std::string field_type;

  std::getline(is, class_name, '.');
  std::getline(is, field_name, ':');
  std::getline(is, field_type, '\0');

  jclass klass = reinterpret_cast<jclass>(
      env->NewGlobalRef(env->FindClass(class_name.substr(1, class_name.size() - 2).c_str())));
  jfieldID field = env->GetFieldID(klass, field_name.c_str(), field_type.c_str());
  CHECK(klass != nullptr);
  CHECK(field != nullptr);
  LOG(INFO) << "listing field " << field_id;
  env->PopLocalFrame(nullptr);
  return std::make_pair(klass, field);
}

static std::vector<std::pair<jclass, jfieldID>> GetRequestedFields(JNIEnv* env,
                                                                   const std::string& args) {
  std::vector<std::pair<jclass, jfieldID>> res;
  std::stringstream args_stream(args);
  std::string item;
  while (std::getline(args_stream, item, ',')) {
    if (item == "") {
      continue;
    }
    res.push_back(SplitField(env, item));
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

struct RequestList {
  std::vector<std::pair<jclass, jfieldID>> fields_;
};

static void DataDumpRequestCb(jvmtiEnv* jvmti) {
  JNIEnv* env = nullptr;
  CHECK_EQ(java_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6), JNI_OK);
  LOG(INFO) << "Dumping counts of null fields.";
  LOG(INFO) << "\t" << "Field name"
            << "\t" << "null count"
            << "\t" << "total count";
  RequestList* list;
  CHECK_JVMTI(jvmti->GetEnvironmentLocalStorage(reinterpret_cast<void**>(&list)));
  for (std::pair<jclass, jfieldID>& p : list->fields_) {
    jclass klass = p.first;
    jfieldID field = p.second;
    // Make sure all instances of the class are tagged with the klass ptr value. Since this is a
    // global ref it's guaranteed to be unique.
    CHECK_JVMTI(jvmti->IterateOverInstancesOfClass(
        p.first,
        // We need to do this to all objects every time since we might be looking for multiple
        // fields in classes that are subtypes of each other.
        JVMTI_HEAP_OBJECT_EITHER,
        /* class_tag, size, tag_ptr, user_data*/
        [](jlong, jlong, jlong* tag_ptr, void* klass) -> jvmtiIterationControl {
          *tag_ptr = static_cast<jlong>(reinterpret_cast<intptr_t>(klass));
          return JVMTI_ITERATION_CONTINUE;
        },
        klass));
    jobject* obj_list;
    jint obj_len;
    jlong tag = static_cast<jlong>(reinterpret_cast<intptr_t>(klass));
    CHECK_JVMTI(jvmti->GetObjectsWithTags(1, &tag, &obj_len, &obj_list, nullptr));

    uint64_t null_cnt = 0;
    for (jint i = 0; i < obj_len; i++) {
      if (env->GetObjectField(obj_list[i], field) == nullptr) {
        null_cnt++;
      }
    }

    char* field_name;
    char* field_sig;
    char* class_name;
    CHECK_JVMTI(jvmti->GetFieldName(klass, field, &field_name, &field_sig, nullptr));
    CHECK_JVMTI(jvmti->GetClassSignature(klass, &class_name, nullptr));
    LOG(INFO) << "\t" << class_name << "." << field_name << ":" << field_sig
              << "\t" << null_cnt
              << "\t" << obj_len;
    CHECK_JVMTI(jvmti->Deallocate(reinterpret_cast<unsigned char*>(field_name)));
    CHECK_JVMTI(jvmti->Deallocate(reinterpret_cast<unsigned char*>(field_sig)));
    CHECK_JVMTI(jvmti->Deallocate(reinterpret_cast<unsigned char*>(class_name)));
  }
}

static void VMDeathCb(jvmtiEnv* jvmti, JNIEnv* env ATTRIBUTE_UNUSED) {
  DataDumpRequestCb(jvmti);
  RequestList* list = nullptr;
  CHECK_JVMTI(jvmti->GetEnvironmentLocalStorage(reinterpret_cast<void**>(&list)));
  delete list;
}

static void CreateFieldList(jvmtiEnv* jvmti, JNIEnv* env, const std::string& args) {
  RequestList* list = nullptr;
  CHECK_JVMTI(jvmti->Allocate(sizeof(*list), reinterpret_cast<unsigned char**>(&list)));
  new (list) RequestList { .fields_ = GetRequestedFields(env, args), };
  CHECK_JVMTI(jvmti->SetEnvironmentLocalStorage(list));
}

static void VMInitCb(jvmtiEnv* jvmti, JNIEnv* env, jobject thr ATTRIBUTE_UNUSED) {
  char* args = nullptr;
  CHECK_JVMTI(jvmti->GetEnvironmentLocalStorage(reinterpret_cast<void**>(&args)));
  CHECK_JVMTI(jvmti->SetEnvironmentLocalStorage(nullptr));
  CreateFieldList(jvmti, env, args);
  CHECK_JVMTI(jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_DEATH, nullptr));
  CHECK_JVMTI(jvmti->SetEventNotificationMode(JVMTI_ENABLE,
                                              JVMTI_EVENT_DATA_DUMP_REQUEST,
                                              nullptr));
  CHECK_JVMTI(jvmti->Deallocate(reinterpret_cast<unsigned char*>(args)));
}

static jint AgentStart(JavaVM* vm, char* options, bool is_onload) {
  android::base::InitLogging(/* argv= */nullptr);
  java_vm = vm;
  jvmtiEnv* jvmti = nullptr;
  if (SetupJvmtiEnv(vm, &jvmti) != JNI_OK) {
    LOG(ERROR) << "Could not get JVMTI env or ArtTiEnv!";
    return JNI_ERR;
  }
  jvmtiCapabilities caps { .can_tag_objects = 1, };
  CHECK_JVMTI(jvmti->AddCapabilities(&caps));
  jvmtiEventCallbacks cb {
    .VMInit = VMInitCb,
    .DataDumpRequest = DataDumpRequestCb,
    .VMDeath = VMDeathCb,
  };
  CHECK_JVMTI(jvmti->SetEventCallbacks(&cb, sizeof(cb)));
  if (is_onload) {
    unsigned char* ptr = nullptr;
    CHECK_JVMTI(jvmti->Allocate(strlen(options) + 1, &ptr));
    strcpy(reinterpret_cast<char*>(ptr), options);
    CHECK_JVMTI(jvmti->SetEnvironmentLocalStorage(ptr));
    CHECK_JVMTI(jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_INIT, nullptr));
  } else {
    JNIEnv* env = nullptr;
    CHECK_EQ(vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6), JNI_OK);
    CreateFieldList(jvmti, env, options);
    CHECK_JVMTI(jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_DEATH, nullptr));
    CHECK_JVMTI(jvmti->SetEventNotificationMode(JVMTI_ENABLE,
                                                JVMTI_EVENT_DATA_DUMP_REQUEST,
                                                nullptr));
  }
  return JNI_OK;
}

// Late attachment (e.g. 'am attach-agent').
extern "C" JNIEXPORT jint JNICALL Agent_OnAttach(JavaVM *vm,
                                                 char* options,
                                                 void* reserved ATTRIBUTE_UNUSED) {
  return AgentStart(vm, options, /*is_onload=*/false);
}

// Early attachment
extern "C" JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM* jvm,
                                               char* options,
                                               void* reserved ATTRIBUTE_UNUSED) {
  return AgentStart(jvm, options, /*is_onload=*/true);
}

}  // namespace fieldnull

