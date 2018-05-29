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

#ifndef ART_OPENJDKJVMTI_TI_CLASS_LOADER_H_
#define ART_OPENJDKJVMTI_TI_CLASS_LOADER_H_

#include <string>

#include <jni.h>

#include "base/globals.h"
#include "base/mutex.h"
#include "jvmti.h"
#include "mirror/array.h"

namespace art {

class DexFile;
template <class MirrorType> class Handle;
template <class MirrorType> class ObjPtr;
class Thread;

}  // namespace art

namespace openjdkjvmti {

// Class that can redefine a single class's methods.
// TODO We should really make this be driven by an outside class so we can do multiple classes at
// the same time and have less required cleanup.
class ClassLoaderHelper {
 public:
  static bool AddToClassLoader(art::Thread* self,
                               art::Handle<art::mirror::ClassLoader> loader,
                               const art::DexFile* dex_file)
      REQUIRES_SHARED(art::Locks::mutator_lock_);

  // Finds a java.lang.DexFile object that is associated with the given ClassLoader. Each of these
  // j.l.DexFile objects holds several art::DexFile*s in it.
  // TODO This should return the actual source java.lang.DexFile object for the klass being loaded.
  static art::ObjPtr<art::mirror::Object> FindSourceDexFileObject(
      art::Thread* self, art::Handle<art::mirror::ClassLoader> loader)
      REQUIRES_SHARED(art::Locks::mutator_lock_);

  // Calls visitor on each java.lang.DexFile associated with the given loader. The visitor should
  // return true to continue on to the next DexFile or false to stop iterating.
  template<typename Visitor>
  static inline void VisitDexFileObjects(art::Thread* self,
                                         art::Handle<art::mirror::ClassLoader> loader,
                                         const Visitor& visitor)
      REQUIRES_SHARED(art::Locks::mutator_lock_);

  static art::ObjPtr<art::mirror::LongArray> GetDexFileCookie(
      art::Handle<art::mirror::Object> java_dex_file) REQUIRES_SHARED(art::Locks::mutator_lock_);

  static art::ObjPtr<art::mirror::LongArray> AllocateNewDexFileCookie(
      art::Thread* self,
      art::Handle<art::mirror::LongArray> old_dex_file_cookie,
      const art::DexFile* new_dex_file) REQUIRES_SHARED(art::Locks::mutator_lock_);

  static void UpdateJavaDexFile(art::ObjPtr<art::mirror::Object> java_dex_file,
                                art::ObjPtr<art::mirror::LongArray> new_cookie)
      REQUIRES(art::Roles::uninterruptible_) REQUIRES_SHARED(art::Locks::mutator_lock_);

 private:
  static art::ObjPtr<art::mirror::ObjectArray<art::mirror::Object>> GetDexElementList(
      art::Thread* self, art::Handle<art::mirror::ClassLoader> loader)
        REQUIRES_SHARED(art::Locks::mutator_lock_);
};

}  // namespace openjdkjvmti
#endif  // ART_OPENJDKJVMTI_TI_CLASS_LOADER_H_
