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

#include "backtrace_helper.h"

#if defined(__linux__)

#include <sys/types.h>
#include <unistd.h>

#include "unwindstack/Unwinder.h"
#include "unwindstack/RegsGetLocal.h"

#include "thread-inl.h"

#else

// For UNUSED
#include "base/macros.h"

#endif

namespace art {

// We only really support libbacktrace on linux which is unfortunate but since this is only for
// gcstress this isn't a huge deal.
#if defined(__linux__)

struct UnwindHelper : public TLSData {
  static constexpr const char* kTlsKey = "UnwindHelper::kTlsKey";

  explicit UnwindHelper(size_t max_depth)
      : memory_(new unwindstack::MemoryLocal()),
        jit_(memory_),
        dex_(memory_),
        unwinder_(max_depth, &maps_, memory_) {
    CHECK(maps_.Parse());
    unwinder_.SetJitDebug(&jit_, unwindstack::Regs::CurrentArch());
    unwinder_.SetDexFiles(&dex_, unwindstack::Regs::CurrentArch());
    unwinder_.SetResolveNames(false);
    unwindstack::Elf::SetCachingEnabled(true);
  }

  static UnwindHelper* Get(Thread* self, size_t max_depth) {
    UnwindHelper* tls = reinterpret_cast<UnwindHelper*>(self->GetCustomTLS(kTlsKey));
    if (tls == nullptr) {
      tls = new UnwindHelper(max_depth);
      self->SetCustomTLS(kTlsKey, tls);
    }
    return tls;
  }

  unwindstack::Unwinder* Unwinder() { return &unwinder_; }

 private:
  unwindstack::LocalMaps maps_;
  std::shared_ptr<unwindstack::Memory> memory_;
  unwindstack::JitDebug jit_;
  unwindstack::DexFiles dex_;
  unwindstack::Unwinder unwinder_;
};

void BacktraceCollector::Collect() {
  unwindstack::Unwinder* unwinder = UnwindHelper::Get(Thread::Current(), max_depth_)->Unwinder();
  std::unique_ptr<unwindstack::Regs> regs(unwindstack::Regs::CreateFromLocal());
  RegsGetLocal(regs.get());
  unwinder->SetRegs(regs.get());
  unwinder->Unwind();
  num_frames_ = 0;
  if (unwinder->NumFrames() > skip_count_) {
    for (auto it = unwinder->frames().begin() + skip_count_;
         max_depth_ > num_frames_ && it != unwinder->frames().end();
         ++it) {
      out_frames_[num_frames_++] = static_cast<uintptr_t>(it->pc);
    }
  }
}

#else

#pragma clang diagnostic push
#pragma clang diagnostic warning "-W#warnings"
#warning "Backtrace collector is not implemented. GCStress cannot be used."
#pragma clang diagnostic pop

// We only have an implementation for linux. On other plaforms just return nothing. This is not
// really correct but we only use this for hashing and gcstress so it's not too big a deal.
void BacktraceCollector::Collect() {
  UNUSED(skip_count_);
  UNUSED(out_frames_);
  UNUSED(max_depth_);
  num_frames_ = 0;
}

#endif

}  // namespace art
