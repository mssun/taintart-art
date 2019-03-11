/*
 * Copyright (C) 2012 The Android Open Source Project
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

#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>

#include "garbage_collector.h"

#include "android-base/stringprintf.h"

#include "base/dumpable.h"
#include "base/histogram-inl.h"
#include "base/logging.h"  // For VLOG_IS_ON.
#include "base/mutex-inl.h"
#include "base/systrace.h"
#include "base/time_utils.h"
#include "base/utils.h"
#include "gc/accounting/heap_bitmap.h"
#include "gc/gc_pause_listener.h"
#include "gc/heap.h"
#include "gc/space/large_object_space.h"
#include "gc/space/space-inl.h"
#include "runtime.h"
#include "thread-current-inl.h"
#include "thread_list.h"

namespace art {
namespace gc {
namespace collector {

Iteration::Iteration()
    : duration_ns_(0), timings_("GC iteration timing logger", true, VLOG_IS_ON(heap)) {
  Reset(kGcCauseBackground, false);  // Reset to some place holder values.
}

void Iteration::Reset(GcCause gc_cause, bool clear_soft_references) {
  timings_.Reset();
  pause_times_.clear();
  duration_ns_ = 0;
  clear_soft_references_ = clear_soft_references;
  gc_cause_ = gc_cause;
  freed_ = ObjectBytePair();
  freed_los_ = ObjectBytePair();
  freed_bytes_revoke_ = 0;
}

uint64_t Iteration::GetEstimatedThroughput() const {
  // Add 1ms to prevent possible division by 0.
  return (static_cast<uint64_t>(freed_.bytes) * 1000) / (NsToMs(GetDurationNs()) + 1);
}

GarbageCollector::GarbageCollector(Heap* heap, const std::string& name)
    : heap_(heap),
      name_(name),
      pause_histogram_((name_ + " paused").c_str(), kPauseBucketSize, kPauseBucketCount),
      rss_histogram_((name_ + " peak-rss").c_str(), kMemBucketSize, kMemBucketCount),
      freed_bytes_histogram_((name_ + " freed-bytes").c_str(), kMemBucketSize, kMemBucketCount),
      cumulative_timings_(name),
      pause_histogram_lock_("pause histogram lock", kDefaultMutexLevel, true),
      is_transaction_active_(false) {
  ResetCumulativeStatistics();
}

void GarbageCollector::RegisterPause(uint64_t nano_length) {
  GetCurrentIteration()->pause_times_.push_back(nano_length);
}

void GarbageCollector::ResetCumulativeStatistics() {
  cumulative_timings_.Reset();
  total_thread_cpu_time_ns_ = 0u;
  total_time_ns_ = 0u;
  total_freed_objects_ = 0u;
  total_freed_bytes_ = 0;
  rss_histogram_.Reset();
  freed_bytes_histogram_.Reset();
  MutexLock mu(Thread::Current(), pause_histogram_lock_);
  pause_histogram_.Reset();
}

uint64_t GarbageCollector::ExtractRssFromMincore(
    std::list<std::pair<void*, void*>>* gc_ranges) {
  uint64_t rss = 0;
  if (gc_ranges->empty()) {
    return 0;
  }
  // mincore() is linux-specific syscall.
#if defined(__linux__)
  using range_t = std::pair<void*, void*>;
  // Sort gc_ranges
  gc_ranges->sort([](const range_t& a, const range_t& b) {
    return std::less()(a.first, b.first);
  });
  // Merge gc_ranges. It's necessary because the kernel may merge contiguous
  // regions if their properties match. This is sufficient as kernel doesn't
  // merge those adjoining ranges which differ only in name.
  size_t vec_len = 0;
  for (auto it = gc_ranges->begin(); it != gc_ranges->end(); it++) {
    auto next_it = it;
    next_it++;
    while (next_it != gc_ranges->end()) {
      if (it->second == next_it->first) {
        it->second = next_it->second;
        next_it = gc_ranges->erase(next_it);
      } else {
        break;
      }
    }
    size_t length = static_cast<uint8_t*>(it->second) - static_cast<uint8_t*>(it->first);
    // Compute max length for vector allocation later.
    vec_len = std::max(vec_len, length / kPageSize);
  }
  std::unique_ptr<unsigned char[]> vec(new unsigned char[vec_len]);
  for (const auto it : *gc_ranges) {
    size_t length = static_cast<uint8_t*>(it.second) - static_cast<uint8_t*>(it.first);
    if (mincore(it.first, length, vec.get()) == 0) {
      for (size_t i = 0; i < length / kPageSize; i++) {
        // Least significant bit represents residency of a page. Other bits are
        // reserved.
        rss += vec[i] & 0x1;
      }
    } else {
      LOG(WARNING) << "Call to mincore() on memory range [0x" << std::hex << it.first
                   << ", 0x" << it.second << std::dec << ") failed: " << strerror(errno);
    }
  }
  rss *= kPageSize;
  rss_histogram_.AddValue(rss / KB);
#endif
  return rss;
}

void GarbageCollector::Run(GcCause gc_cause, bool clear_soft_references) {
  ScopedTrace trace(android::base::StringPrintf("%s %s GC", PrettyCause(gc_cause), GetName()));
  Thread* self = Thread::Current();
  uint64_t start_time = NanoTime();
  uint64_t thread_cpu_start_time = ThreadCpuNanoTime();
  GetHeap()->CalculatePreGcWeightedAllocatedBytes();
  Iteration* current_iteration = GetCurrentIteration();
  current_iteration->Reset(gc_cause, clear_soft_references);
  // Note transaction mode is single-threaded and there's no asynchronous GC and this flag doesn't
  // change in the middle of a GC.
  is_transaction_active_ = Runtime::Current()->IsActiveTransaction();
  RunPhases();  // Run all the GC phases.
  GetHeap()->CalculatePostGcWeightedAllocatedBytes();
  // Add the current timings to the cumulative timings.
  cumulative_timings_.AddLogger(*GetTimings());
  // Update cumulative statistics with how many bytes the GC iteration freed.
  total_freed_objects_ += current_iteration->GetFreedObjects() +
      current_iteration->GetFreedLargeObjects();
  int64_t freed_bytes = current_iteration->GetFreedBytes() +
      current_iteration->GetFreedLargeObjectBytes();
  total_freed_bytes_ += freed_bytes;
  // Rounding negative freed bytes to 0 as we are not interested in such corner cases.
  freed_bytes_histogram_.AddValue(std::max<int64_t>(freed_bytes / KB, 0));
  uint64_t end_time = NanoTime();
  uint64_t thread_cpu_end_time = ThreadCpuNanoTime();
  total_thread_cpu_time_ns_ += thread_cpu_end_time - thread_cpu_start_time;
  current_iteration->SetDurationNs(end_time - start_time);
  if (Locks::mutator_lock_->IsExclusiveHeld(self)) {
    // The entire GC was paused, clear the fake pauses which might be in the pause times and add
    // the whole GC duration.
    current_iteration->pause_times_.clear();
    RegisterPause(current_iteration->GetDurationNs());
  }
  total_time_ns_ += current_iteration->GetDurationNs();
  for (uint64_t pause_time : current_iteration->GetPauseTimes()) {
    MutexLock mu(self, pause_histogram_lock_);
    pause_histogram_.AdjustAndAddValue(pause_time);
  }
  is_transaction_active_ = false;
}

void GarbageCollector::SwapBitmaps() {
  TimingLogger::ScopedTiming t(__FUNCTION__, GetTimings());
  // Swap the live and mark bitmaps for each alloc space. This is needed since sweep re-swaps
  // these bitmaps. The bitmap swapping is an optimization so that we do not need to clear the live
  // bits of dead objects in the live bitmap.
  const GcType gc_type = GetGcType();
  for (const auto& space : GetHeap()->GetContinuousSpaces()) {
    // We never allocate into zygote spaces.
    if (space->GetGcRetentionPolicy() == space::kGcRetentionPolicyAlwaysCollect ||
        (gc_type == kGcTypeFull &&
         space->GetGcRetentionPolicy() == space::kGcRetentionPolicyFullCollect)) {
      accounting::ContinuousSpaceBitmap* live_bitmap = space->GetLiveBitmap();
      accounting::ContinuousSpaceBitmap* mark_bitmap = space->GetMarkBitmap();
      if (live_bitmap != nullptr && live_bitmap != mark_bitmap) {
        heap_->GetLiveBitmap()->ReplaceBitmap(live_bitmap, mark_bitmap);
        heap_->GetMarkBitmap()->ReplaceBitmap(mark_bitmap, live_bitmap);
        CHECK(space->IsContinuousMemMapAllocSpace());
        space->AsContinuousMemMapAllocSpace()->SwapBitmaps();
      }
    }
  }
  for (const auto& disc_space : GetHeap()->GetDiscontinuousSpaces()) {
    space::LargeObjectSpace* space = disc_space->AsLargeObjectSpace();
    accounting::LargeObjectBitmap* live_set = space->GetLiveBitmap();
    accounting::LargeObjectBitmap* mark_set = space->GetMarkBitmap();
    heap_->GetLiveBitmap()->ReplaceLargeObjectBitmap(live_set, mark_set);
    heap_->GetMarkBitmap()->ReplaceLargeObjectBitmap(mark_set, live_set);
    space->SwapBitmaps();
  }
}

uint64_t GarbageCollector::GetEstimatedMeanThroughput() const {
  // Add 1ms to prevent possible division by 0.
  return (total_freed_bytes_ * 1000) / (NsToMs(GetCumulativeTimings().GetTotalNs()) + 1);
}

void GarbageCollector::ResetMeasurements() {
  {
    MutexLock mu(Thread::Current(), pause_histogram_lock_);
    pause_histogram_.Reset();
  }
  cumulative_timings_.Reset();
  rss_histogram_.Reset();
  freed_bytes_histogram_.Reset();
  total_thread_cpu_time_ns_ = 0u;
  total_time_ns_ = 0u;
  total_freed_objects_ = 0u;
  total_freed_bytes_ = 0;
}

GarbageCollector::ScopedPause::ScopedPause(GarbageCollector* collector, bool with_reporting)
    : start_time_(NanoTime()), collector_(collector), with_reporting_(with_reporting) {
  Runtime* runtime = Runtime::Current();
  runtime->GetThreadList()->SuspendAll(__FUNCTION__);
  if (with_reporting) {
    GcPauseListener* pause_listener = runtime->GetHeap()->GetGcPauseListener();
    if (pause_listener != nullptr) {
      pause_listener->StartPause();
    }
  }
}

GarbageCollector::ScopedPause::~ScopedPause() {
  collector_->RegisterPause(NanoTime() - start_time_);
  Runtime* runtime = Runtime::Current();
  if (with_reporting_) {
    GcPauseListener* pause_listener = runtime->GetHeap()->GetGcPauseListener();
    if (pause_listener != nullptr) {
      pause_listener->EndPause();
    }
  }
  runtime->GetThreadList()->ResumeAll();
}

// Returns the current GC iteration and assocated info.
Iteration* GarbageCollector::GetCurrentIteration() {
  return heap_->GetCurrentGcIteration();
}
const Iteration* GarbageCollector::GetCurrentIteration() const {
  return heap_->GetCurrentGcIteration();
}

void GarbageCollector::RecordFree(const ObjectBytePair& freed) {
  GetCurrentIteration()->freed_.Add(freed);
  heap_->RecordFree(freed.objects, freed.bytes);
}
void GarbageCollector::RecordFreeLOS(const ObjectBytePair& freed) {
  GetCurrentIteration()->freed_los_.Add(freed);
  heap_->RecordFree(freed.objects, freed.bytes);
}

uint64_t GarbageCollector::GetTotalPausedTimeNs() {
  MutexLock mu(Thread::Current(), pause_histogram_lock_);
  return pause_histogram_.AdjustedSum();
}

void GarbageCollector::DumpPerformanceInfo(std::ostream& os) {
  const CumulativeLogger& logger = GetCumulativeTimings();
  const size_t iterations = logger.GetIterations();
  if (iterations == 0) {
    return;
  }
  os << Dumpable<CumulativeLogger>(logger);
  const uint64_t total_ns = logger.GetTotalNs();
  double seconds = NsToMs(logger.GetTotalNs()) / 1000.0;
  const uint64_t freed_bytes = GetTotalFreedBytes();
  const uint64_t freed_objects = GetTotalFreedObjects();
  {
    MutexLock mu(Thread::Current(), pause_histogram_lock_);
    if (pause_histogram_.SampleSize() > 0) {
      Histogram<uint64_t>::CumulativeData cumulative_data;
      pause_histogram_.CreateHistogram(&cumulative_data);
      pause_histogram_.PrintConfidenceIntervals(os, 0.99, cumulative_data);
    }
  }
#if defined(__linux__)
  if (rss_histogram_.SampleSize() > 0) {
    os << rss_histogram_.Name()
       << ": Avg: " << PrettySize(rss_histogram_.Mean() * KB)
       << " Max: " << PrettySize(rss_histogram_.Max() * KB)
       << " Min: " << PrettySize(rss_histogram_.Min() * KB) << "\n";
    os << "Peak-rss Histogram: ";
    rss_histogram_.DumpBins(os);
    os << "\n";
  }
#endif
  if (freed_bytes_histogram_.SampleSize() > 0) {
    os << freed_bytes_histogram_.Name()
       << ": Avg: " << PrettySize(freed_bytes_histogram_.Mean() * KB)
       << " Max: " << PrettySize(freed_bytes_histogram_.Max() * KB)
       << " Min: " << PrettySize(freed_bytes_histogram_.Min() * KB) << "\n";
    os << "Freed-bytes histogram: ";
    freed_bytes_histogram_.DumpBins(os);
    os << "\n";
  }
  double cpu_seconds = NsToMs(GetTotalCpuTime()) / 1000.0;
  os << GetName() << " total time: " << PrettyDuration(total_ns)
     << " mean time: " << PrettyDuration(total_ns / iterations) << "\n"
     << GetName() << " freed: " << freed_objects
     << " objects with total size " << PrettySize(freed_bytes) << "\n"
     << GetName() << " throughput: " << freed_objects / seconds << "/s / "
     << PrettySize(freed_bytes / seconds) << "/s"
     << "  per cpu-time: "
     << static_cast<uint64_t>(freed_bytes / cpu_seconds) << "/s / "
     << PrettySize(freed_bytes / cpu_seconds) << "/s\n";
}

}  // namespace collector
}  // namespace gc
}  // namespace art
