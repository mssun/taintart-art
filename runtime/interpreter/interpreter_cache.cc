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

#include "interpreter_cache.h"
#include "thread-inl.h"

namespace art {

void InterpreterCache::Clear(Thread* owning_thread) {
  DCHECK(owning_thread->GetInterpreterCache() == this);
  DCHECK(owning_thread == Thread::Current() || owning_thread->IsSuspended());
  data_.fill(Entry{});
}

bool InterpreterCache::IsCalledFromOwningThread() {
  return Thread::Current()->GetInterpreterCache() == this;
}

}  // namespace art
