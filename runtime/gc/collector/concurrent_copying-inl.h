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

#ifndef ART_RUNTIME_GC_COLLECTOR_CONCURRENT_COPYING_INL_H_
#define ART_RUNTIME_GC_COLLECTOR_CONCURRENT_COPYING_INL_H_

#include "concurrent_copying.h"

#include "gc/accounting/atomic_stack.h"
#include "gc/accounting/space_bitmap-inl.h"
#include "gc/heap.h"
#include "gc/space/region_space-inl.h"
#include "gc/verification.h"
#include "lock_word.h"
#include "mirror/class.h"
#include "mirror/object-readbarrier-inl.h"

namespace art {
namespace gc {
namespace collector {

inline mirror::Object* ConcurrentCopying::MarkUnevacFromSpaceRegion(
    Thread* const self,
    mirror::Object* ref,
    accounting::ContinuousSpaceBitmap* bitmap) {
  if (use_generational_cc_ && !done_scanning_.load(std::memory_order_acquire)) {
    // Everything in the unevac space should be marked for young generation CC,
    // except for large objects.
    DCHECK(!young_gen_ || region_space_bitmap_->Test(ref) || region_space_->IsLargeObject(ref))
        << ref << " "
        << ref->GetClass<kVerifyNone, kWithoutReadBarrier>()->PrettyClass();
    // Since the mark bitmap is still filled in from last GC (or from marking phase of 2-phase CC,
    // we can not use that or else the mutator may see references to the from space. Instead, use
    // the baker pointer itself as the mark bit.
    if (ref->AtomicSetReadBarrierState(ReadBarrier::NonGrayState(), ReadBarrier::GrayState())) {
      // TODO: We don't actually need to scan this object later, we just need to clear the gray
      // bit.
      // TODO: We could also set the mark bit here for "free" since this case comes from the
      // read barrier.
      PushOntoMarkStack(self, ref);
    }
    DCHECK_EQ(ref->GetReadBarrierState(), ReadBarrier::GrayState());
    return ref;
  }
  // For the Baker-style RB, in a rare case, we could incorrectly change the object from non-gray
  // (black) to gray even though the object has already been marked through. This happens if a
  // mutator thread gets preempted before the AtomicSetReadBarrierState below, GC marks through the
  // object (changes it from non-gray (white) to gray and back to non-gray (black)), and the thread
  // runs and incorrectly changes it from non-gray (black) to gray. If this happens, the object
  // will get added to the mark stack again and get changed back to non-gray (black) after it is
  // processed.
  if (kUseBakerReadBarrier) {
    // Test the bitmap first to avoid graying an object that has already been marked through most
    // of the time.
    if (bitmap->Test(ref)) {
      return ref;
    }
  }
  // This may or may not succeed, which is ok because the object may already be gray.
  bool success = false;
  if (kUseBakerReadBarrier) {
    // GC will mark the bitmap when popping from mark stack. If only the GC is touching the bitmap
    // we can avoid an expensive CAS.
    // For the baker case, an object is marked if either the mark bit marked or the bitmap bit is
    // set.
    success = ref->AtomicSetReadBarrierState(/* expected_rb_state= */ ReadBarrier::NonGrayState(),
                                             /* rb_state= */ ReadBarrier::GrayState());
  } else {
    success = !bitmap->AtomicTestAndSet(ref);
  }
  if (success) {
    // Newly marked.
    if (kUseBakerReadBarrier) {
      DCHECK_EQ(ref->GetReadBarrierState(), ReadBarrier::GrayState());
    }
    PushOntoMarkStack(self, ref);
  }
  return ref;
}

template<bool kGrayImmuneObject>
inline mirror::Object* ConcurrentCopying::MarkImmuneSpace(Thread* const self,
                                                          mirror::Object* ref) {
  if (kUseBakerReadBarrier) {
    // The GC-running thread doesn't (need to) gray immune objects except when updating thread roots
    // in the thread flip on behalf of suspended threads (when gc_grays_immune_objects_ is
    // true). Also, a mutator doesn't (need to) gray an immune object after GC has updated all
    // immune space objects (when updated_all_immune_objects_ is true).
    if (kIsDebugBuild) {
      if (self == thread_running_gc_) {
        DCHECK(!kGrayImmuneObject ||
               updated_all_immune_objects_.load(std::memory_order_relaxed) ||
               gc_grays_immune_objects_);
      } else {
        DCHECK(kGrayImmuneObject);
      }
    }
    if (!kGrayImmuneObject || updated_all_immune_objects_.load(std::memory_order_relaxed)) {
      return ref;
    }
    // This may or may not succeed, which is ok because the object may already be gray.
    bool success =
        ref->AtomicSetReadBarrierState(/* expected_rb_state= */ ReadBarrier::NonGrayState(),
                                       /* rb_state= */ ReadBarrier::GrayState());
    if (success) {
      MutexLock mu(self, immune_gray_stack_lock_);
      immune_gray_stack_.push_back(ref);
    }
  }
  return ref;
}

template<bool kGrayImmuneObject, bool kNoUnEvac, bool kFromGCThread>
inline mirror::Object* ConcurrentCopying::Mark(Thread* const self,
                                               mirror::Object* from_ref,
                                               mirror::Object* holder,
                                               MemberOffset offset) {
  // Cannot have `kNoUnEvac` when Generational CC collection is disabled.
  DCHECK(!kNoUnEvac || use_generational_cc_);
  if (from_ref == nullptr) {
    return nullptr;
  }
  DCHECK(heap_->collector_type_ == kCollectorTypeCC);
  if (kFromGCThread) {
    DCHECK(is_active_);
    DCHECK_EQ(self, thread_running_gc_);
  } else if (UNLIKELY(kUseBakerReadBarrier && !is_active_)) {
    // In the lock word forward address state, the read barrier bits
    // in the lock word are part of the stored forwarding address and
    // invalid. This is usually OK as the from-space copy of objects
    // aren't accessed by mutators due to the to-space
    // invariant. However, during the dex2oat image writing relocation
    // and the zygote compaction, objects can be in the forward
    // address state (to store the forward/relocation addresses) and
    // they can still be accessed and the invalid read barrier bits
    // are consulted. If they look like gray but aren't really, the
    // read barriers slow path can trigger when it shouldn't. To guard
    // against this, return here if the CC collector isn't running.
    return from_ref;
  }
  DCHECK(region_space_ != nullptr) << "Read barrier slow path taken when CC isn't running?";
  if (region_space_->HasAddress(from_ref)) {
    space::RegionSpace::RegionType rtype = region_space_->GetRegionTypeUnsafe(from_ref);
    switch (rtype) {
      case space::RegionSpace::RegionType::kRegionTypeToSpace:
        // It's already marked.
        return from_ref;
      case space::RegionSpace::RegionType::kRegionTypeFromSpace: {
        mirror::Object* to_ref = GetFwdPtr(from_ref);
        if (to_ref == nullptr) {
          // It isn't marked yet. Mark it by copying it to the to-space.
          to_ref = Copy(self, from_ref, holder, offset);
        }
        // The copy should either be in a to-space region, or in the
        // non-moving space, if it could not fit in a to-space region.
        DCHECK(region_space_->IsInToSpace(to_ref) || heap_->non_moving_space_->HasAddress(to_ref))
            << "from_ref=" << from_ref << " to_ref=" << to_ref;
        return to_ref;
      }
      case space::RegionSpace::RegionType::kRegionTypeUnevacFromSpace:
        if (kNoUnEvac && use_generational_cc_ && !region_space_->IsLargeObject(from_ref)) {
          if (!kFromGCThread) {
            DCHECK(IsMarkedInUnevacFromSpace(from_ref)) << "Returning unmarked object to mutator";
          }
          return from_ref;
        }
        return MarkUnevacFromSpaceRegion(self, from_ref, region_space_bitmap_);
      default:
        // The reference is in an unused region. Remove memory protection from
        // the region space and log debugging information.
        region_space_->Unprotect();
        LOG(FATAL_WITHOUT_ABORT) << DumpHeapReference(holder, offset, from_ref);
        region_space_->DumpNonFreeRegions(LOG_STREAM(FATAL_WITHOUT_ABORT));
        heap_->GetVerification()->LogHeapCorruption(holder, offset, from_ref, /* fatal= */ true);
        UNREACHABLE();
    }
  } else {
    if (immune_spaces_.ContainsObject(from_ref)) {
      return MarkImmuneSpace<kGrayImmuneObject>(self, from_ref);
    } else {
      return MarkNonMoving(self, from_ref, holder, offset);
    }
  }
}

inline mirror::Object* ConcurrentCopying::MarkFromReadBarrier(mirror::Object* from_ref) {
  mirror::Object* ret;
  Thread* const self = Thread::Current();
  // We can get here before marking starts since we gray immune objects before the marking phase.
  if (from_ref == nullptr || !self->GetIsGcMarking()) {
    return from_ref;
  }
  // TODO: Consider removing this check when we are done investigating slow paths. b/30162165
  if (UNLIKELY(mark_from_read_barrier_measurements_)) {
    ret = MarkFromReadBarrierWithMeasurements(self, from_ref);
  } else {
    ret = Mark</*kGrayImmuneObject=*/true, /*kNoUnEvac=*/false, /*kFromGCThread=*/false>(self,
                                                                                         from_ref);
  }
  // Only set the mark bit for baker barrier.
  if (kUseBakerReadBarrier && LIKELY(!rb_mark_bit_stack_full_ && ret->AtomicSetMarkBit(0, 1))) {
    // If the mark stack is full, we may temporarily go to mark and back to unmarked. Seeing both
    // values are OK since the only race is doing an unnecessary Mark.
    if (!rb_mark_bit_stack_->AtomicPushBack(ret)) {
      // Mark stack is full, set the bit back to zero.
      CHECK(ret->AtomicSetMarkBit(1, 0));
      // Set rb_mark_bit_stack_full_, this is racy but OK since AtomicPushBack is thread safe.
      rb_mark_bit_stack_full_ = true;
    }
  }
  return ret;
}

inline mirror::Object* ConcurrentCopying::GetFwdPtr(mirror::Object* from_ref) {
  DCHECK(region_space_->IsInFromSpace(from_ref));
  LockWord lw = from_ref->GetLockWord(false);
  if (lw.GetState() == LockWord::kForwardingAddress) {
    mirror::Object* fwd_ptr = reinterpret_cast<mirror::Object*>(lw.ForwardingAddress());
    DCHECK(fwd_ptr != nullptr);
    return fwd_ptr;
  } else {
    return nullptr;
  }
}

inline bool ConcurrentCopying::IsMarkedInUnevacFromSpace(mirror::Object* from_ref) {
  // Use load-acquire on the read barrier pointer to ensure that we never see a black (non-gray)
  // read barrier state with an unmarked bit due to reordering.
  DCHECK(region_space_->IsInUnevacFromSpace(from_ref));
  if (kUseBakerReadBarrier && from_ref->GetReadBarrierStateAcquire() == ReadBarrier::GrayState()) {
    return true;
  } else if (!use_generational_cc_ || done_scanning_.load(std::memory_order_acquire)) {
    // If the card table scanning is not finished yet, then only read-barrier
    // state should be checked. Checking the mark bitmap is unreliable as there
    // may be some objects - whose corresponding card is dirty - which are
    // marked in the mark bitmap, but cannot be considered marked unless their
    // read-barrier state is set to Gray.
    //
    // Why read read-barrier state before checking done_scanning_?
    // If the read-barrier state was read *after* done_scanning_, then there
    // exists a concurrency race due to which even after the object is marked,
    // read-barrier state is checked *after* that, this function will return
    // false. The following scenario may cause the race:
    //
    // 1. Mutator thread reads done_scanning_ and upon finding it false, gets
    // suspended before reading the object's read-barrier state.
    // 2. GC thread finishes card-table scan and then sets done_scanning_ to
    // true.
    // 3. GC thread grays the object, scans it, marks in the bitmap, and then
    // changes its read-barrier state back to non-gray.
    // 4. Mutator thread resumes, reads the object's read-barrier state and
    // returns false.
    return region_space_bitmap_->Test(from_ref);
  }
  return false;
}

}  // namespace collector
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_COLLECTOR_CONCURRENT_COPYING_INL_H_
