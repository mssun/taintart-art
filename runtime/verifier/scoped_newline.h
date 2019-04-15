/*
 * Copyright (C) 2019 The Android Open Source Project
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

#ifndef ART_RUNTIME_VERIFIER_SCOPED_NEWLINE_H_
#define ART_RUNTIME_VERIFIER_SCOPED_NEWLINE_H_

#include <ostream>

#include <android-base/logging.h>

namespace art {
namespace verifier {

// RAII to inject a newline after a message.
struct ScopedNewLine {
  explicit ScopedNewLine(std::ostream& os) : stream(os) {}

  ScopedNewLine(ScopedNewLine&& other) : stream(other.stream), active(other.active) {
    other.active = false;
  }

  ScopedNewLine(ScopedNewLine&) = delete;
  ScopedNewLine& operator=(ScopedNewLine&) = delete;

  ~ScopedNewLine() {
    if (active) {
      stream << std::endl;
    }
  }

  template <class T>
  ScopedNewLine& operator<<(const T& t) {
    DCHECK(active);
    stream << t;
    return *this;
  }

  ScopedNewLine& operator<<(std::ostream& (*f)(std::ostream&)) {
    DCHECK(active);
    stream << f;
    return *this;
  }

  std::ostream& stream;
  bool active = true;
};

}  // namespace verifier
}  // namespace art

#endif  // ART_RUNTIME_VERIFIER_SCOPED_NEWLINE_H_
