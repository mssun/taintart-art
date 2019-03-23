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

#include <android-base/logging.h>

#include <jni.h>
#include <jvmti.h>

namespace dumpjvmti {

namespace {

// Special art ti-version number. We will use this as a fallback if we cannot get a regular JVMTI
// env.
static constexpr jint kArtTiVersion = JVMTI_VERSION_1_2 | 0x40000000;

template <typename T> static void Dealloc(jvmtiEnv* env, T* t) {
  env->Deallocate(reinterpret_cast<unsigned char*>(t));
}

template <typename T, typename... Rest> static void Dealloc(jvmtiEnv* env, T* t, Rest... rs) {
  Dealloc(env, t);
  Dealloc(env, rs...);
}

static void DeallocParams(jvmtiEnv* env, jvmtiParamInfo* params, jint n_params) {
  for (jint i = 0; i < n_params; i++) {
    Dealloc(env, params[i].name);
  }
}

// The extension function to get the internal data
static jvmtiError (*GetInternalData)(jvmtiEnv* env, unsigned char** data) = nullptr;

static jint SetupJvmtiEnv(JavaVM* vm, jvmtiEnv** jvmti) {
  jint res = 0;
  res = vm->GetEnv(reinterpret_cast<void**>(jvmti), JVMTI_VERSION_1_1);

  if (res != JNI_OK || *jvmti == nullptr) {
    LOG(ERROR) << "Unable to access JVMTI, error code " << res;
    res = vm->GetEnv(reinterpret_cast<void**>(jvmti), kArtTiVersion);
    if (res != JNI_OK) {
      return res;
    }
  }

  jvmtiEnv* env = *jvmti;

  // Get the extensions.
  jint n_ext = 0;
  jvmtiExtensionFunctionInfo* infos = nullptr;
  if (env->GetExtensionFunctions(&n_ext, &infos) != JVMTI_ERROR_NONE) {
    return JNI_ERR;
  }
  for (jint i = 0; i < n_ext; i++) {
    jvmtiExtensionFunctionInfo* cur_info = &infos[i];
    if (strcmp("com.android.art.misc.get_plugin_internal_state", cur_info->id) == 0) {
      GetInternalData = reinterpret_cast<decltype(GetInternalData)>(cur_info->func);
    }
    // Cleanup the cur_info
    DeallocParams(env, cur_info->params, cur_info->param_count);
    Dealloc(env, cur_info->id, cur_info->short_description, cur_info->params, cur_info->errors);
  }
  // Cleanup the array.
  Dealloc(env, infos);
  return GetInternalData != nullptr ? JNI_OK : JNI_ERR;
}

static void CbDataDump(jvmtiEnv* jvmti) {
  unsigned char* data = nullptr;
  if (JVMTI_ERROR_NONE == GetInternalData(jvmti, &data)) {
    LOG(INFO) << data;
    Dealloc(jvmti, data);
  }
}

}  // namespace

static jint AgentStart(JavaVM* vm, char* options ATTRIBUTE_UNUSED, void* reserved ATTRIBUTE_UNUSED) {
  jvmtiEnv* jvmti = nullptr;
  if (SetupJvmtiEnv(vm, &jvmti) != JNI_OK) {
    LOG(ERROR) << "Could not get JVMTI env or ArtTiEnv!";
    return JNI_ERR;
  }
  jvmtiEventCallbacks cb{
    .DataDumpRequest = CbDataDump,
  };
  jvmti->SetEventCallbacks(&cb, sizeof(cb));
  jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_DATA_DUMP_REQUEST, nullptr);
  return JNI_OK;
}

// Late attachment (e.g. 'am attach-agent').
extern "C" JNIEXPORT jint JNICALL Agent_OnAttach(JavaVM* vm, char* options, void* reserved) {
  return AgentStart(vm, options, reserved);
}

// Early attachment
extern "C" JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM* jvm, char* options, void* reserved) {
  return AgentStart(jvm, options, reserved);
}

}  // namespace dumpjvmti
