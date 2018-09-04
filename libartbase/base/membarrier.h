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

#ifndef ART_LIBARTBASE_BASE_MEMBARRIER_H_
#define ART_LIBARTBASE_BASE_MEMBARRIER_H_

namespace art {
  // Command types for the linux membarrier system call. Different Linux installation may include
  // different subsets of these commands (at the same codepoints).
  //
  // Hardcoding these values is temporary until bionic and prebuilts glibc have an up to date
  // linux/membarrier.h. The order and values follow the current linux definitions.
  enum class MembarrierCommand : int  {
    // MEMBARRIER_CMD_QUERY
    kQuery = 0,
    // MEMBARRIER_CMD_GLOBAL
    kGlobal = (1 << 0),
    // MEMBARRIER_CMD_GLOBAL_EXPEDITED
    kGlobalExpedited = (1 << 1),
    // MEMBARRIER_CMD_REGISTER_GLOBAL_EXPEDITED
    kRegisterGlobalExpedited = (1 << 2),
    // MEMBARRIER_CMD_PRIVATE_EXPEDITED
    kPrivateExpedited = (1 << 3),
    // MEMBARRIER_CMD_REGISTER_PRIVATE_EXPEDITED
    kRegisterPrivateExpedited = (1 << 4),
    // MEMBARRIER_CMD_REGISTER_PRIVATE_EXPEDITED_SYNC_CORE
    kPrivateExpeditedSyncCore = (1 << 5),
    // MEMBARRIER_CMD_REGISTER_PRIVATE_EXPEDITED_SYNC_CORE
    kRegisterPrivateExpeditedSyncCore = (1 << 6)
  };

  // Call membarrier(2) if available on platform and return result. This method can fail if the
  // command is not supported by the kernel. The underlying system call is linux specific.
  int membarrier(MembarrierCommand command);

}  // namespace art

#endif  // ART_LIBARTBASE_BASE_MEMBARRIER_H_
