/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "class_root.h"

namespace art {

const char* GetClassRootDescriptor(ClassRoot class_root) {
  static const char* class_roots_descriptors[] = {
#define CLASS_ROOT_DESCRIPTOR(name, descriptor, mirror_type) descriptor,
      CLASS_ROOT_LIST(CLASS_ROOT_DESCRIPTOR)
#undef CLASS_ROOT_DESCRIPTOR
  };
  static_assert(arraysize(class_roots_descriptors) == static_cast<size_t>(ClassRoot::kMax),
                "Mismatch between class descriptors and class-root enum");

  DCHECK_LT(static_cast<uint32_t>(class_root), static_cast<uint32_t>(ClassRoot::kMax));
  const char* descriptor = class_roots_descriptors[static_cast<size_t>(class_root)];
  CHECK(descriptor != nullptr);
  return descriptor;
}

}  // namespace art
