/*
 * Copyright (C) 2014 The Android Open Source Project
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

#ifndef ART_RUNTIME_MONITOR_OBJECTS_STACK_VISITOR_H_
#define ART_RUNTIME_MONITOR_OBJECTS_STACK_VISITOR_H_

#include <android-base/logging.h>

#include "art_method.h"
#include "base/locks.h"
#include "monitor.h"
#include "stack.h"
#include "thread.h"
#include "thread_state.h"

namespace art {

namespace mirror {
class Object;
}

class Context;

class MonitorObjectsStackVisitor : public StackVisitor {
 public:
  MonitorObjectsStackVisitor(Thread* thread_in,
                             Context* context,
                             bool check_suspended = true,
                             bool dump_locks_in = true)
      REQUIRES_SHARED(Locks::mutator_lock_)
      : StackVisitor(thread_in,
                     context,
                     StackVisitor::StackWalkKind::kIncludeInlinedFrames,
                     check_suspended),
        frame_count(0u),
        dump_locks(dump_locks_in) {}

  enum class VisitMethodResult {
    kContinueMethod,
    kSkipMethod,
    kEndStackWalk,
  };

  bool VisitFrame() final REQUIRES_SHARED(Locks::mutator_lock_);

 protected:
  virtual VisitMethodResult StartMethod(ArtMethod* m, size_t frame_nr)
      REQUIRES_SHARED(Locks::mutator_lock_) = 0;
  virtual VisitMethodResult EndMethod(ArtMethod* m)
      REQUIRES_SHARED(Locks::mutator_lock_) = 0;

  virtual void VisitWaitingObject(ObjPtr<mirror::Object> obj, ThreadState state)
      REQUIRES_SHARED(Locks::mutator_lock_) = 0;
  virtual void VisitSleepingObject(ObjPtr<mirror::Object> obj)
      REQUIRES_SHARED(Locks::mutator_lock_) = 0;
  virtual void VisitBlockedOnObject(ObjPtr<mirror::Object> obj,
                                    ThreadState state,
                                    uint32_t owner_tid)
      REQUIRES_SHARED(Locks::mutator_lock_) = 0;
  virtual void VisitLockedObject(ObjPtr<mirror::Object> obj)
      REQUIRES_SHARED(Locks::mutator_lock_) = 0;

  size_t frame_count;

 private:
  static void VisitLockedObject(ObjPtr<mirror::Object> o, void* context)
      REQUIRES_SHARED(Locks::mutator_lock_);

  const bool dump_locks;
};

}  // namespace art

#endif  // ART_RUNTIME_MONITOR_OBJECTS_STACK_VISITOR_H_
