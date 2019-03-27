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

#ifndef ART_RUNTIME_MIRROR_CALL_SITE_H_
#define ART_RUNTIME_MIRROR_CALL_SITE_H_

#include "mirror/method_handle_impl.h"
#include "obj_ptr.h"

namespace art {

struct CallSiteOffsets;

namespace mirror {

// C++ mirror of java.lang.invoke.CallSite
class MANAGED CallSite : public Object {
 public:
  ObjPtr<MethodHandle> GetTarget() REQUIRES_SHARED(Locks::mutator_lock_);

 private:
  static inline MemberOffset TargetOffset() {
    return MemberOffset(OFFSETOF_MEMBER(CallSite, target_));
  }

  HeapReference<mirror::MethodHandle> target_;

  friend struct art::CallSiteOffsets;  // for verifying offset information
  DISALLOW_IMPLICIT_CONSTRUCTORS(CallSite);
};

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_CALL_SITE_H_
