/* Copyright (C) 2018 The Android Open Source Project
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

#include "ti_logging.h"

#include "art_jvmti.h"

#include "base/mutex.h"
#include "base/strlcpy.h"
#include "thread-current-inl.h"

namespace openjdkjvmti {

jvmtiError LogUtil::GetLastError(jvmtiEnv* env, char** data) {
  if (env == nullptr || data == nullptr) {
    return ERR(INVALID_ENVIRONMENT);
  }
  ArtJvmTiEnv* tienv = ArtJvmTiEnv::AsArtJvmTiEnv(env);
  art::MutexLock mu(art::Thread::Current(), tienv->last_error_mutex_);
  if (tienv->last_error_.empty()) {
    return ERR(ABSENT_INFORMATION);
  }
  const size_t size = tienv->last_error_.size() + 1;
  char* out;
  jvmtiError err = tienv->Allocate(size, reinterpret_cast<unsigned char**>(&out));
  if (err != OK) {
    return err;
  }
  strlcpy(out, tienv->last_error_.c_str(), size);
  *data = out;
  return OK;
}

jvmtiError LogUtil::ClearLastError(jvmtiEnv* env) {
  if (env == nullptr) {
    return ERR(INVALID_ENVIRONMENT);
  }
  ArtJvmTiEnv* tienv = ArtJvmTiEnv::AsArtJvmTiEnv(env);
  art::MutexLock mu(art::Thread::Current(), tienv->last_error_mutex_);
  tienv->last_error_.clear();
  return OK;
}

}  // namespace openjdkjvmti
