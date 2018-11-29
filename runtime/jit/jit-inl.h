/*
 * Copyright 2018 The Android Open Source Project
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

#ifndef ART_RUNTIME_JIT_JIT_INL_H_
#define ART_RUNTIME_JIT_JIT_INL_H_

#include "jit/jit.h"

#include "art_method.h"
#include "base/bit_utils.h"
#include "thread.h"
#include "runtime-inl.h"

namespace art {
namespace jit {

inline bool Jit::ShouldUsePriorityThreadWeight(Thread* self) {
  return self->IsJitSensitiveThread() && Runtime::Current()->InJankPerceptibleProcessState();
}

inline void Jit::AddSamples(Thread* self,
                            ArtMethod* method,
                            uint16_t samples,
                            bool with_backedges) {
  if (Jit::ShouldUsePriorityThreadWeight(self)) {
    samples *= PriorityThreadWeight();
  }
  uint32_t old_count = method->GetCounter();
  uint32_t new_count = old_count + samples;

  // The full check is fairly expensive so we just add to hotness most of the time,
  // and we do the full check only when some of the higher bits of the count change.
  // NB: The method needs to see the transitions of the counter past the thresholds.
  uint32_t old_batch = RoundDown(old_count, kJitSamplesBatchSize);  // Clear lower bits.
  uint32_t new_batch = RoundDown(new_count, kJitSamplesBatchSize);  // Clear lower bits.
  if (UNLIKELY(old_batch == 0)) {
    // For low sample counts, we check every time (which is important for tests).
    if (!MaybeCompileMethod(self, method, old_count, new_count, with_backedges)) {
      // Tests may check that the counter is 0 for methods that we never compile.
      return;  // Ignore the samples for now and retry later.
    }
  } else if (UNLIKELY(old_batch != new_batch)) {
    // For high sample counts, we check only when we move past the batch boundary.
    if (!MaybeCompileMethod(self, method, old_batch, new_batch, with_backedges)) {
      // OSR compilation will ignore the samples if they don't have backedges.
      return;  // Ignore the samples for now and retry later.
    }
  }

  method->SetCounter(new_count);
}

}  // namespace jit
}  // namespace art

#endif  // ART_RUNTIME_JIT_JIT_INL_H_
