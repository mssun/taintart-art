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

#include "monitor_objects_stack_visitor.h"

#include "obj_ptr-inl.h"
#include "read_barrier-inl.h"
#include "thread-current-inl.h"

namespace art {

bool MonitorObjectsStackVisitor::VisitFrame() {
  ArtMethod* m = GetMethod();
  if (m->IsRuntimeMethod()) {
    return true;
  }

  VisitMethodResult vmrEntry = StartMethod(m, frame_count);
  switch (vmrEntry) {
    case VisitMethodResult::kContinueMethod:
      break;
    case VisitMethodResult::kSkipMethod:
      return true;
    case VisitMethodResult::kEndStackWalk:
      return false;
  }

  if (frame_count == 0) {
    // Top frame, check for blocked state.

    ObjPtr<mirror::Object> monitor_object;
    uint32_t lock_owner_tid;
    ThreadState state = Monitor::FetchState(GetThread(),
                                            &monitor_object,
                                            &lock_owner_tid);
    switch (state) {
      case kWaiting:
      case kTimedWaiting:
        VisitWaitingObject(monitor_object, state);
        break;
      case kSleeping:
        VisitSleepingObject(monitor_object);
        break;

      case kBlocked:
      case kWaitingForLockInflation:
        VisitBlockedOnObject(monitor_object, state, lock_owner_tid);
        break;

      default:
        break;
    }
  }

  if (dump_locks) {
    // Visit locks, but do not abort on errors. This could trigger a nested abort.
    // Skip visiting locks if dump_locks is false as it would cause a bad_mutexes_held in
    // RegTypeCache::RegTypeCache due to thread_list_lock.
    Monitor::VisitLocks(this, VisitLockedObject, this, false);
  }

  ++frame_count;

  VisitMethodResult vmrExit = EndMethod(m);
  switch (vmrExit) {
    case VisitMethodResult::kContinueMethod:
    case VisitMethodResult::kSkipMethod:
      return true;

    case VisitMethodResult::kEndStackWalk:
      return false;
  }
  LOG(FATAL) << "Unreachable";
  UNREACHABLE();
}

void MonitorObjectsStackVisitor::VisitLockedObject(ObjPtr<mirror::Object> o, void* context) {
  MonitorObjectsStackVisitor* self = reinterpret_cast<MonitorObjectsStackVisitor*>(context);
  if (o != nullptr) {
    if (kUseReadBarrier && Thread::Current()->GetIsGcMarking()) {
      // We may call Thread::Dump() in the middle of the CC thread flip and this thread's stack
      // may have not been flipped yet and "o" may be a from-space (stale) ref, in which case the
      // IdentityHashCode call below will crash. So explicitly mark/forward it here.
      o = ReadBarrier::Mark(o.Ptr());
    }
  }
  self->VisitLockedObject(o);
}

}  // namespace art
