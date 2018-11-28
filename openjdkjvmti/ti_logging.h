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

#ifndef ART_OPENJDKJVMTI_TI_LOGGING_H_
#define ART_OPENJDKJVMTI_TI_LOGGING_H_

#include "art_jvmti.h"

#include <ostream>
#include <sstream>

#include <android-base/logging.h>
#include <android-base/macros.h>

#include "base/mutex.h"
#include "thread-current-inl.h"

namespace openjdkjvmti {

// NB Uses implementation details of android-base/logging.h.
#define JVMTI_LOG(severity, env)                             \
  ::openjdkjvmti::JvmtiLogMessage((env),                     \
                                  __FILE__,                  \
                                  __LINE__,                  \
                                  ::android::base::DEFAULT,  \
                                  SEVERITY_LAMBDA(severity), \
                                  _LOG_TAG_INTERNAL,         \
                                  -1)

class JvmtiLogMessage {
 public:
  JvmtiLogMessage(jvmtiEnv* env,
                  const char* file,
                  unsigned int line,
                  android::base::LogId id,
                  android::base::LogSeverity severity,
                  const char* tag,
                  int error)
      : env_(ArtJvmTiEnv::AsArtJvmTiEnv(env)),
        real_log_(file, line, id, severity, tag, error),
        real_log_stream_(real_log_.stream()) {
    DCHECK(env_ != nullptr);
  }

  ~JvmtiLogMessage() {
    art::MutexLock mu(art::Thread::Current(), env_->last_error_mutex_);
    env_->last_error_ = save_stream_.str();
  }

  template<typename T>
  JvmtiLogMessage& operator<<(T t) {
    (real_log_stream_ << t);
    (save_stream_ << t);
    return *this;
  }

 private:
  ArtJvmTiEnv* env_;
  android::base::LogMessage real_log_;
  // Lifetime of real_log_stream_ is lifetime of real_log_.
  std::ostream& real_log_stream_;
  std::ostringstream save_stream_;

  DISALLOW_COPY_AND_ASSIGN(JvmtiLogMessage);
};

class LogUtil {
 public:
  static jvmtiError ClearLastError(jvmtiEnv* env);
  static jvmtiError GetLastError(jvmtiEnv* env, char** data);
};

}  // namespace openjdkjvmti

#endif  // ART_OPENJDKJVMTI_TI_LOGGING_H_
