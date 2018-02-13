/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include <inttypes.h>

#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>
#include <vector>

#include "android-base/stringprintf.h"
#include "jni.h"
#include "jvmti.h"
#include "scoped_local_ref.h"

#include "dex/code_item_accessors-inl.h"
#include "dex/dex_file_loader.h"
#include "dex/dex_file.h"
#include "dex/dex_file_loader.h"
#include "dex/dex_instruction.h"

// Test infrastructure
#include "jvmti_helper.h"
#include "test_env.h"

namespace art {
namespace Test983SourceTransformVerify {

constexpr bool kSkipInitialLoad = true;

static void Println(JNIEnv* env, std::ostringstream msg_stream) {
  std::string msg = msg_stream.str();
  ScopedLocalRef<jclass> test_klass(env, env->FindClass("art/Test983"));
  jmethodID println_method = env->GetStaticMethodID(test_klass.get(),
                                                    "doPrintln",
                                                    "(Ljava/lang/String;)V");
  ScopedLocalRef<jstring> data(env, env->NewStringUTF(msg.c_str()));
  env->CallStaticVoidMethod(test_klass.get(), println_method, data.get());
}

// The hook we are using.
void JNICALL CheckDexFileHook(jvmtiEnv* jvmti_env ATTRIBUTE_UNUSED,
                              JNIEnv* env,
                              jclass class_being_redefined,
                              jobject loader ATTRIBUTE_UNUSED,
                              const char* name,
                              jobject protection_domain ATTRIBUTE_UNUSED,
                              jint class_data_len,
                              const unsigned char* class_data,
                              jint* new_class_data_len ATTRIBUTE_UNUSED,
                              unsigned char** new_class_data ATTRIBUTE_UNUSED) {
  if (kSkipInitialLoad && class_being_redefined == nullptr) {
    // Something got loaded concurrently. Just ignore it for now. To make sure the test is
    // repeatable we only care about things that come from RetransformClasses.
    return;
  }
  Println(env, std::ostringstream() << "Dex file hook for " << name);
  if (IsJVM()) {
    return;
  }

  // Due to b/72402467 the class_data_len might just be an estimate.
  CHECK_GE(static_cast<size_t>(class_data_len), sizeof(DexFile::Header));
  const DexFile::Header* header = reinterpret_cast<const DexFile::Header*>(class_data);
  uint32_t header_file_size = header->file_size_;
  CHECK_LE(static_cast<jint>(header_file_size), class_data_len);
  class_data_len = static_cast<jint>(header_file_size);

  const DexFileLoader dex_file_loader;
  std::string error;
  std::unique_ptr<const DexFile> dex(dex_file_loader.Open(class_data,
                                                          class_data_len,
                                                          "fake_location.dex",
                                                          /*location_checksum*/ 0,
                                                          /*oat_dex_file*/ nullptr,
                                                          /*verify*/ true,
                                                          /*verify_checksum*/ true,
                                                          &error));
  if (dex.get() == nullptr) {
    Println(env, std::ostringstream() << "Failed to verify dex file for "
                                      << name << " because " << error);
    return;
  }
  for (uint32_t i = 0; i < dex->NumClassDefs(); i++) {
    const DexFile::ClassDef& def = dex->GetClassDef(i);
    const uint8_t* data_item = dex->GetClassData(def);
    if (data_item == nullptr) {
      continue;
    }
    for (ClassDataItemIterator it(*dex, data_item); it.HasNext(); it.Next()) {
      if (!it.IsAtMethod() || it.GetMethodCodeItem() == nullptr) {
        continue;
      }
      for (const DexInstructionPcPair& pair :
          art::CodeItemInstructionAccessor(*dex, it.GetMethodCodeItem())) {
        const Instruction& inst = pair.Inst();
        int forbiden_flags = (Instruction::kVerifyError | Instruction::kVerifyRuntimeOnly);
        if (inst.Opcode() == Instruction::RETURN_VOID_NO_BARRIER ||
            (inst.GetVerifyExtraFlags() & forbiden_flags) != 0) {
          Println(env, std::ostringstream() << "Unexpected instruction found in "
                                            << dex->PrettyMethod(it.GetMemberIndex())
                                            << " [Dex PC: 0x" << std::hex << pair.DexPc()
                                            << std::dec << "] : " << inst.DumpString(dex.get()));
          continue;
        }
      }
    }
  }
}

// Get all capabilities except those related to retransformation.
extern "C" JNIEXPORT void JNICALL Java_art_Test983_setupLoadHook(JNIEnv* env, jclass) {
  jvmtiEventCallbacks cb;
  memset(&cb, 0, sizeof(cb));
  cb.ClassFileLoadHook = CheckDexFileHook;
  JvmtiErrorToException(env, jvmti_env, jvmti_env->SetEventCallbacks(&cb, sizeof(cb)));
}

}  // namespace Test983SourceTransformVerify
}  // namespace art
