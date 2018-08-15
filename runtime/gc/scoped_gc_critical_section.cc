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

#include "scoped_gc_critical_section.h"

#include "gc/collector_type.h"
#include "gc/heap.h"
#include "runtime.h"
#include "thread-current-inl.h"

namespace art {
namespace gc {

const char* GCCriticalSection::Enter(GcCause cause, CollectorType type) {
  Runtime::Current()->GetHeap()->StartGC(self_, cause, type);
  if (self_ != nullptr) {
    return self_->StartAssertNoThreadSuspension(section_name_);
  } else {
    // Workaround to avoid having to mark the whole function as NO_THREAD_SAFETY_ANALYSIS.
    auto kludge = []() ACQUIRE(Roles::uninterruptible_) NO_THREAD_SAFETY_ANALYSIS {};
    kludge();
    return nullptr;
  }
}

void GCCriticalSection::Exit(const char* old_cause) {
  if (self_ != nullptr) {
    self_->EndAssertNoThreadSuspension(old_cause);
  } else {
    // Workaround to avoid having to mark the whole function as NO_THREAD_SAFETY_ANALYSIS.
    auto kludge = []() RELEASE(Roles::uninterruptible_) NO_THREAD_SAFETY_ANALYSIS {};
    kludge();
  }
  Runtime::Current()->GetHeap()->FinishGC(self_, collector::kGcTypeNone);
}

ScopedGCCriticalSection::ScopedGCCriticalSection(Thread* self,
                                                 GcCause cause,
                                                 CollectorType collector_type)
    : critical_section_(self, "ScopedGCCriticalSection") {
  old_no_suspend_reason_ = critical_section_.Enter(cause, collector_type);
}

ScopedGCCriticalSection::~ScopedGCCriticalSection() {
  critical_section_.Exit(old_no_suspend_reason_);
}

}  // namespace gc
}  // namespace art
