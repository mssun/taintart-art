/*
 * Copyright 2019 The Android Open Source Project
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

#include "jni.h"

#include "gc/heap.h"
#include "gc/space/image_space.h"
#include "gc/space/space-inl.h"
#include "gc/space/region_space.h"
#include "image.h"
#include "mirror/class.h"
#include "runtime.h"
#include "scoped_thread_state_change-inl.h"

namespace art {

namespace {

extern "C" JNIEXPORT jint JNICALL Java_Main_getRegionSize(JNIEnv*, jclass) {
  return gc::space::RegionSpace::kRegionSize;
}

extern "C" JNIEXPORT jint JNICALL Java_Main_checkAppImageSectionSize(JNIEnv*, jclass, jclass c) {
  ScopedObjectAccess soa(Thread::Current());
  ObjPtr<mirror::Class> klass_ptr = soa.Decode<mirror::Class>(c);
  for (auto* space : Runtime::Current()->GetHeap()->GetContinuousSpaces()) {
    if (space->IsImageSpace()) {
      auto* image_space = space->AsImageSpace();
      const auto& image_header = image_space->GetImageHeader();
      if (image_header.IsAppImage() && image_space->HasAddress(klass_ptr.Ptr())) {
        return image_header.GetObjectsSection().Size();
      }
    }
  }
  return 0;
}

}  // namespace

}  // namespace art
