/* Copyright (C) 2017 The Android Open Source Project
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This file implements interfaces from the file jvmti.h. This implementation
 * is licensed under the same terms as the file jvmti.h.  The
 * copyright and license information for the file jvmti.h follows.
 *
 * Copyright (c) 2003, 2011, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

#include <sstream>
#include <unistd.h>

#include "ti_search.h"

#include "jni.h"

#include "art_field-inl.h"
#include "art_jvmti.h"
#include "base/enums.h"
#include "base/macros.h"
#include "base/memfd.h"
#include "base/os.h"
#include "base/unix_file/fd_file.h"
#include "class_linker.h"
#include "dex/art_dex_file_loader.h"
#include "dex/dex_file.h"
#include "dex/dex_file_loader.h"
#include "jni/jni_internal.h"
#include "mirror/class-inl.h"
#include "mirror/object.h"
#include "mirror/string.h"
#include "nativehelper/scoped_local_ref.h"
#include "obj_ptr-inl.h"
#include "runtime.h"
#include "runtime_callbacks.h"
#include "scoped_thread_state_change-inl.h"
#include "thread-current-inl.h"
#include "thread_list.h"
#include "ti_logging.h"
#include "ti_phase.h"
#include "well_known_classes.h"

namespace openjdkjvmti {

static std::vector<std::string> gSystemOnloadSegments;

static art::ObjPtr<art::mirror::Object> GetSystemProperties(art::Thread* self,
                                                            art::ClassLinker* class_linker)
    REQUIRES_SHARED(art::Locks::mutator_lock_) {
  art::ObjPtr<art::mirror::Class> system_class =
      class_linker->LookupClass(self, "Ljava/lang/System;", nullptr);
  DCHECK(system_class != nullptr);
  DCHECK(system_class->IsInitialized());

  art::ArtField* props_field =
      system_class->FindDeclaredStaticField("props", "Ljava/util/Properties;");
  DCHECK(props_field != nullptr);

  art::ObjPtr<art::mirror::Object> props_obj = props_field->GetObject(system_class);
  DCHECK(props_obj != nullptr);

  return props_obj;
}

static void Update() REQUIRES_SHARED(art::Locks::mutator_lock_) {
  if (gSystemOnloadSegments.empty()) {
    return;
  }

  // In the on-load phase we have to modify java.class.path to influence the system classloader.
  // As this is an unmodifiable system property, we have to access the "defaults" field.
  art::ClassLinker* class_linker = art::Runtime::Current()->GetClassLinker();
  DCHECK(class_linker != nullptr);
  art::Thread* self = art::Thread::Current();

  // Prepare: collect classes, fields and methods.
  art::ObjPtr<art::mirror::Class> properties_class =
      class_linker->LookupClass(self, "Ljava/util/Properties;", nullptr);
  DCHECK(properties_class != nullptr);

  ScopedLocalRef<jobject> defaults_jobj(self->GetJniEnv(), nullptr);
  {
    art::ObjPtr<art::mirror::Object> props_obj = GetSystemProperties(self, class_linker);

    art::ArtField* defaults_field =
        properties_class->FindDeclaredInstanceField("defaults", "Ljava/util/Properties;");
    DCHECK(defaults_field != nullptr);

    art::ObjPtr<art::mirror::Object> defaults_obj = defaults_field->GetObject(props_obj);
    DCHECK(defaults_obj != nullptr);
    defaults_jobj.reset(self->GetJniEnv()->AddLocalReference<jobject>(defaults_obj));
  }

  art::ArtMethod* get_property =
      properties_class->FindClassMethod(
          "getProperty",
          "(Ljava/lang/String;)Ljava/lang/String;",
          art::kRuntimePointerSize);
  DCHECK(get_property != nullptr);
  DCHECK(!get_property->IsDirect());
  DCHECK(get_property->GetDeclaringClass() == properties_class);
  art::ArtMethod* set_property =
      properties_class->FindClassMethod(
          "setProperty",
          "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/Object;",
          art::kRuntimePointerSize);
  DCHECK(set_property != nullptr);
  DCHECK(!set_property->IsDirect());
  DCHECK(set_property->GetDeclaringClass() == properties_class);

  // This is an allocation. Do this late to avoid the need for handles.
  ScopedLocalRef<jobject> cp_jobj(self->GetJniEnv(), nullptr);
  {
    art::ObjPtr<art::mirror::Object> cp_key =
        art::mirror::String::AllocFromModifiedUtf8(self, "java.class.path");
    if (cp_key == nullptr) {
      self->AssertPendingOOMException();
      self->ClearException();
      return;
    }
    cp_jobj.reset(self->GetJniEnv()->AddLocalReference<jobject>(cp_key));
  }

  // OK, now get the current value.
  std::string str_value;
  {
    ScopedLocalRef<jobject> old_value(self->GetJniEnv(),
                                      self->GetJniEnv()->CallObjectMethod(
                                          defaults_jobj.get(),
                                          art::jni::EncodeArtMethod(get_property),
                                          cp_jobj.get()));
    DCHECK(old_value.get() != nullptr);

    str_value = self->DecodeJObject(old_value.get())->AsString()->ToModifiedUtf8();
    self->GetJniEnv()->DeleteLocalRef(old_value.release());
  }

  // Update the value by appending the new segments.
  for (const std::string& segment : gSystemOnloadSegments) {
    if (!str_value.empty()) {
      str_value += ":";
    }
    str_value += segment;
  }
  gSystemOnloadSegments.clear();

  // Create the new value object.
  ScopedLocalRef<jobject> new_val_jobj(self->GetJniEnv(), nullptr);
  {
    art::ObjPtr<art::mirror::Object> new_value =
        art::mirror::String::AllocFromModifiedUtf8(self, str_value.c_str());
    if (new_value == nullptr) {
      self->AssertPendingOOMException();
      self->ClearException();
      return;
    }

    new_val_jobj.reset(self->GetJniEnv()->AddLocalReference<jobject>(new_value));
  }

  // Write to the defaults.
  ScopedLocalRef<jobject> res_obj(self->GetJniEnv(),
                                  self->GetJniEnv()->CallObjectMethod(defaults_jobj.get(),
                                      art::jni::EncodeArtMethod(set_property),
                                      cp_jobj.get(),
                                      new_val_jobj.get()));
  if (self->IsExceptionPending()) {
    self->ClearException();
    return;
  }
}

struct SearchCallback : public art::RuntimePhaseCallback {
  void NextRuntimePhase(RuntimePhase phase) override REQUIRES_SHARED(art::Locks::mutator_lock_) {
    if (phase == RuntimePhase::kStart) {
      // It's time to update the system properties.
      Update();
    }
  }
};

static SearchCallback gSearchCallback;

void SearchUtil::Register() {
  art::Runtime* runtime = art::Runtime::Current();

  art::ScopedThreadStateChange stsc(art::Thread::Current(),
                                    art::ThreadState::kWaitingForDebuggerToAttach);
  art::ScopedSuspendAll ssa("Add search callback");
  runtime->GetRuntimeCallbacks()->AddRuntimePhaseCallback(&gSearchCallback);
}

void SearchUtil::Unregister() {
  art::ScopedThreadStateChange stsc(art::Thread::Current(),
                                    art::ThreadState::kWaitingForDebuggerToAttach);
  art::ScopedSuspendAll ssa("Remove search callback");
  art::Runtime* runtime = art::Runtime::Current();
  runtime->GetRuntimeCallbacks()->RemoveRuntimePhaseCallback(&gSearchCallback);
}

jvmtiError SearchUtil::AddToBootstrapClassLoaderSearch(jvmtiEnv* env,
                                                       const char* segment) {
  art::Runtime* current = art::Runtime::Current();
  if (current == nullptr) {
    return ERR(WRONG_PHASE);
  }
  if (current->GetClassLinker() == nullptr) {
    return ERR(WRONG_PHASE);
  }
  if (segment == nullptr) {
    return ERR(NULL_POINTER);
  }

  std::string error_msg;
  std::vector<std::unique_ptr<const art::DexFile>> dex_files;
  const art::ArtDexFileLoader dex_file_loader;
  if (!dex_file_loader.Open(segment,
                            segment,
                            /* verify= */ true,
                            /* verify_checksum= */ true,
                            &error_msg,
                            &dex_files)) {
    JVMTI_LOG(WARNING, env) << "Could not open " << segment << " for boot classpath extension: "
                            << error_msg;
    return ERR(ILLEGAL_ARGUMENT);
  }

  art::ScopedObjectAccess soa(art::Thread::Current());
  for (std::unique_ptr<const art::DexFile>& dex_file : dex_files) {
    current->GetClassLinker()->AppendToBootClassPath(art::Thread::Current(), *dex_file.release());
  }

  return ERR(NONE);
}

jvmtiError SearchUtil::AddToDexClassLoaderInMemory(jvmtiEnv* jvmti_env,
                                                   jobject classloader,
                                                   const char* dex_bytes,
                                                   jint dex_bytes_length) {
  if (jvmti_env == nullptr) {
    return ERR(INVALID_ENVIRONMENT);
  } else if (art::Thread::Current() == nullptr) {
    return ERR(UNATTACHED_THREAD);
  } else if (classloader == nullptr) {
    return ERR(NULL_POINTER);
  } else if (dex_bytes == nullptr) {
    return ERR(NULL_POINTER);
  } else if (dex_bytes_length <= 0) {
    return ERR(ILLEGAL_ARGUMENT);
  }

  jvmtiPhase phase = PhaseUtil::GetPhaseUnchecked();

  // TODO We really should try to support doing this during the ON_LOAD phase.
  if (phase != jvmtiPhase::JVMTI_PHASE_LIVE) {
    JVMTI_LOG(INFO, jvmti_env) << "Cannot add buffers to classpath during ON_LOAD phase to "
                               << "prevent file-descriptor leaking.";
    return ERR(WRONG_PHASE);
  }

  // We have java APIs for adding files to the classpath, we might as well use them. It simplifies a
  // lot of code as well.

  // Create a memfd
  art::File file(art::memfd_create("JVMTI InMemory Added dex file", 0), /*check-usage*/true);
  if (file.Fd() < 0) {
    char* reason = strerror(errno);
    JVMTI_LOG(ERROR, jvmti_env) << "Unable to create memfd due to " << reason;
    return ERR(INTERNAL);
  }
  // Fill it with the buffer.
  if (!file.WriteFully(dex_bytes, dex_bytes_length) || file.Flush() != 0) {
    JVMTI_LOG(ERROR, jvmti_env) << "Failed to write to memfd!";
    return ERR(INTERNAL);
  }
  // Get the filename in procfs.
  std::ostringstream oss;
  oss << "/proc/self/fd/" << file.Fd();
  std::string seg(oss.str());
  // Use common code.

  jvmtiError result = AddToDexClassLoader(jvmti_env, classloader, seg.c_str());
  // We have either loaded the dex file and have a new MemMap pointing to the same pages or loading
  // has failed and the memory isn't needed anymore. Either way we can close the memfd we created
  // and return.
  if (file.Close() != 0) {
    JVMTI_LOG(WARNING, jvmti_env) << "Failed to close memfd!";
  }
  return result;
}

jvmtiError SearchUtil::AddToDexClassLoader(jvmtiEnv* jvmti_env,
                                           jobject classloader,
                                           const char* segment) {
  if (jvmti_env == nullptr) {
    return ERR(INVALID_ENVIRONMENT);
  } else if (art::Thread::Current() == nullptr) {
    return ERR(UNATTACHED_THREAD);
  } else if (classloader == nullptr) {
    return ERR(NULL_POINTER);
  } else if (segment == nullptr) {
    return ERR(NULL_POINTER);
  }

  jvmtiPhase phase = PhaseUtil::GetPhaseUnchecked();

  // TODO We really should try to support doing this during the ON_LOAD phase.
  if (phase != jvmtiPhase::JVMTI_PHASE_LIVE) {
    JVMTI_LOG(INFO, jvmti_env) << "Cannot add to classpath of arbitrary classloaders during "
                               << "ON_LOAD phase.";
    return ERR(WRONG_PHASE);
  }

  // We'll use BaseDexClassLoader.addDexPath, as it takes care of array resizing etc. As a downside,
  // exceptions are swallowed.

  art::Thread* self = art::Thread::Current();
  JNIEnv* env = self->GetJniEnv();
  if (!env->IsInstanceOf(classloader, art::WellKnownClasses::dalvik_system_BaseDexClassLoader)) {
    JVMTI_LOG(ERROR, jvmti_env) << "Unable to add " << segment << " to non BaseDexClassLoader!";
    return ERR(CLASS_LOADER_UNSUPPORTED);
  }

  jmethodID add_dex_path_id = env->GetMethodID(
      art::WellKnownClasses::dalvik_system_BaseDexClassLoader,
      "addDexPath",
      "(Ljava/lang/String;)V");
  if (add_dex_path_id == nullptr) {
    return ERR(INTERNAL);
  }

  ScopedLocalRef<jstring> dex_path(env, env->NewStringUTF(segment));
  if (dex_path.get() == nullptr) {
    return ERR(INTERNAL);
  }
  env->CallVoidMethod(classloader, add_dex_path_id, dex_path.get());

  if (env->ExceptionCheck()) {
    {
      art::ScopedObjectAccess soa(self);
      JVMTI_LOG(ERROR, jvmti_env) << "Failed to add " << segment << " to classloader. Error was "
                                  << self->GetException()->Dump();
    }
    env->ExceptionClear();
    return ERR(ILLEGAL_ARGUMENT);
  }
  return OK;
}

jvmtiError SearchUtil::AddToSystemClassLoaderSearch(jvmtiEnv* jvmti_env, const char* segment) {
  if (segment == nullptr) {
    return ERR(NULL_POINTER);
  }

  jvmtiPhase phase = PhaseUtil::GetPhaseUnchecked();

  if (phase == jvmtiPhase::JVMTI_PHASE_ONLOAD) {
    // We could try and see whether it is a valid path. We could also try to allocate Java
    // objects to avoid later OOME.
    gSystemOnloadSegments.push_back(segment);
    return ERR(NONE);
  } else if (phase != jvmtiPhase::JVMTI_PHASE_LIVE) {
    return ERR(WRONG_PHASE);
  }

  jobject loader = art::Runtime::Current()->GetSystemClassLoader();
  if (loader == nullptr) {
    return ERR(INTERNAL);
  }

  art::Thread* self = art::Thread::Current();
  JNIEnv* env = self->GetJniEnv();
  if (!env->IsInstanceOf(loader, art::WellKnownClasses::dalvik_system_BaseDexClassLoader)) {
    return ERR(INTERNAL);
  }

  return AddToDexClassLoader(jvmti_env, loader, segment);
}

}  // namespace openjdkjvmti
