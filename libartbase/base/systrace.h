/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef ART_LIBARTBASE_BASE_SYSTRACE_H_
#define ART_LIBARTBASE_BASE_SYSTRACE_H_

#include <sstream>
#include <string>

#include "android-base/stringprintf.h"
#include "macros.h"
#include "palette/palette.h"

namespace art {

inline bool ATraceEnabled() {
  int enabled = 0;
  if (UNLIKELY(PaletteTraceEnabled(&enabled) == PaletteStatus::kOkay && enabled != 0)) {
    return true;
  } else {
    return false;
  }
}

inline void ATraceBegin(const char* name) {
  PaletteTraceBegin(name);
}

inline void ATraceEnd() {
  PaletteTraceEnd();
}

inline void ATraceIntegerValue(const char* name, int32_t value) {
  PaletteTraceIntegerValue(name, value);
}

class ScopedTrace {
 public:
  explicit ScopedTrace(const char* name) {
    ATraceBegin(name);
  }
  template <typename Fn>
  explicit ScopedTrace(Fn fn) {
    if (UNLIKELY(ATraceEnabled())) {
      ATraceBegin(fn().c_str());
    }
  }

  explicit ScopedTrace(const std::string& name) : ScopedTrace(name.c_str()) {}

  ~ScopedTrace() {
    ATraceEnd();
  }
};

// Helper for the SCOPED_TRACE macro. Do not use directly.
class ScopedTraceNoStart {
 public:
  ScopedTraceNoStart() {
  }

  ~ScopedTraceNoStart() {
    ATraceEnd();
  }

  // Message helper for the macro. Do not use directly.
  class ScopedTraceMessageHelper {
   public:
    ScopedTraceMessageHelper() {
    }
    ~ScopedTraceMessageHelper() {
      ATraceBegin(buffer_.str().c_str());
    }

    std::ostream& stream() {
      return buffer_;
    }

   private:
    std::ostringstream buffer_;
  };
};

#define SCOPED_TRACE \
  ::art::ScopedTraceNoStart APPEND_TOKENS_AFTER_EVAL(trace, __LINE__) ; \
  (ATraceEnabled()) && ::art::ScopedTraceNoStart::ScopedTraceMessageHelper().stream()

}  // namespace art

#endif  // ART_LIBARTBASE_BASE_SYSTRACE_H_
