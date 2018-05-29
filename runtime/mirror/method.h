/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef ART_RUNTIME_MIRROR_METHOD_H_
#define ART_RUNTIME_MIRROR_METHOD_H_

#include "executable.h"

namespace art {
namespace mirror {

class Class;

// C++ mirror of java.lang.reflect.Method.
class MANAGED Method : public Executable {
 public:
  template <PointerSize kPointerSize, bool kTransactionActive>
  static Method* CreateFromArtMethod(Thread* self, ArtMethod* method)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Roles::uninterruptible_);

 private:
  DISALLOW_COPY_AND_ASSIGN(Method);
};

// C++ mirror of java.lang.reflect.Constructor.
class MANAGED Constructor: public Executable {
 public:
  template <PointerSize kPointerSize, bool kTransactionActive>
  static Constructor* CreateFromArtMethod(Thread* self, ArtMethod* method)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Roles::uninterruptible_);

 private:
  DISALLOW_COPY_AND_ASSIGN(Constructor);
};

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_METHOD_H_
