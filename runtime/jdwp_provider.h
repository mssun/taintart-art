/*
 * Copyright 2017 The Android Open Source Project
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

#ifndef ART_RUNTIME_JDWP_PROVIDER_H_
#define ART_RUNTIME_JDWP_PROVIDER_H_

#include <ios>

#include "base/globals.h"

namespace art {

enum class JdwpProvider {
  kNone,
  // Special value only used to denote that no explicit choice has been made by the user. This
  // should not be used and one should always call CanonicalizeJdwpProvider which will remove this
  // value before using a JdwpProvider value.
  kUnset,
  kInternal,
  kAdbConnection,

  // The current default provider. Used if you run -XjdwpProvider:default
  kDefaultJdwpProvider = kAdbConnection,

  // What we should use as provider with no options and debuggable. On host we always want to be
  // none since there is no adbd on host.
  kUnsetDebuggable = kIsTargetBuild ? kDefaultJdwpProvider : kNone,
  // What we should use as provider with no options and non-debuggable
  kUnsetNonDebuggable = kNone,
};

inline JdwpProvider CanonicalizeJdwpProvider(JdwpProvider p, bool debuggable) {
  if (p != JdwpProvider::kUnset) {
    return p;
  }
  if (debuggable) {
    return JdwpProvider::kUnsetDebuggable;
  }
  return JdwpProvider::kUnsetNonDebuggable;
}

std::ostream& operator<<(std::ostream& os, const JdwpProvider& rhs);

}  // namespace art
#endif  // ART_RUNTIME_JDWP_PROVIDER_H_
