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

#include <algorithm>
#include <ostream>

#include "compiled_method_storage.h"

#include <android-base/logging.h>

#include "base/data_hash.h"
#include "base/utils.h"
#include "compiled_method.h"
#include "linker/linker_patch.h"
#include "thread-current-inl.h"
#include "utils/dedupe_set-inl.h"
#include "utils/swap_space.h"

namespace art {

namespace {  // anonymous namespace

template <typename T>
const LengthPrefixedArray<T>* CopyArray(SwapSpace* swap_space, const ArrayRef<const T>& array) {
  DCHECK(!array.empty());
  SwapAllocator<uint8_t> allocator(swap_space);
  void* storage = allocator.allocate(LengthPrefixedArray<T>::ComputeSize(array.size()));
  LengthPrefixedArray<T>* array_copy = new(storage) LengthPrefixedArray<T>(array.size());
  std::copy(array.begin(), array.end(), array_copy->begin());
  return array_copy;
}

template <typename T>
void ReleaseArray(SwapSpace* swap_space, const LengthPrefixedArray<T>* array) {
  SwapAllocator<uint8_t> allocator(swap_space);
  size_t size = LengthPrefixedArray<T>::ComputeSize(array->size());
  array->~LengthPrefixedArray<T>();
  allocator.deallocate(const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(array)), size);
}

}  // anonymous namespace

template <typename T, typename DedupeSetType>
inline const LengthPrefixedArray<T>* CompiledMethodStorage::AllocateOrDeduplicateArray(
    const ArrayRef<const T>& data,
    DedupeSetType* dedupe_set) {
  if (data.empty()) {
    return nullptr;
  } else if (!DedupeEnabled()) {
    return CopyArray(swap_space_.get(), data);
  } else {
    return dedupe_set->Add(Thread::Current(), data);
  }
}

template <typename T>
inline void CompiledMethodStorage::ReleaseArrayIfNotDeduplicated(
    const LengthPrefixedArray<T>* array) {
  if (array != nullptr && !DedupeEnabled()) {
    ReleaseArray(swap_space_.get(), array);
  }
}

template <typename ContentType>
class CompiledMethodStorage::DedupeHashFunc {
 private:
  static constexpr bool kUseMurmur3Hash = true;

 public:
  size_t operator()(const ArrayRef<ContentType>& array) const {
    return DataHash()(array);
  }
};

template <typename T>
class CompiledMethodStorage::LengthPrefixedArrayAlloc {
 public:
  explicit LengthPrefixedArrayAlloc(SwapSpace* swap_space)
      : swap_space_(swap_space) {
  }

  const LengthPrefixedArray<T>* Copy(const ArrayRef<const T>& array) {
    return CopyArray(swap_space_, array);
  }

  void Destroy(const LengthPrefixedArray<T>* array) {
    ReleaseArray(swap_space_, array);
  }

 private:
  SwapSpace* const swap_space_;
};

class CompiledMethodStorage::ThunkMapKey {
 public:
  ThunkMapKey(linker::LinkerPatch::Type type, uint32_t custom_value1, uint32_t custom_value2)
      : type_(type), custom_value1_(custom_value1), custom_value2_(custom_value2) {}

  bool operator<(const ThunkMapKey& other) const {
    if (custom_value1_ != other.custom_value1_) {
      return custom_value1_ < other.custom_value1_;
    }
    if (custom_value2_ != other.custom_value2_) {
      return custom_value2_ < other.custom_value2_;
    }
    return type_ < other.type_;
  }

 private:
  linker::LinkerPatch::Type type_;
  uint32_t custom_value1_;
  uint32_t custom_value2_;
};

class CompiledMethodStorage::ThunkMapValue {
 public:
  ThunkMapValue(std::vector<uint8_t, SwapAllocator<uint8_t>>&& code,
                const std::string& debug_name)
      : code_(std::move(code)), debug_name_(debug_name) {}

  ArrayRef<const uint8_t> GetCode() const {
    return ArrayRef<const uint8_t>(code_);
  }

  const std::string& GetDebugName() const {
    return debug_name_;
  }

 private:
  std::vector<uint8_t, SwapAllocator<uint8_t>> code_;
  std::string debug_name_;
};

CompiledMethodStorage::CompiledMethodStorage(int swap_fd)
    : swap_space_(swap_fd == -1 ? nullptr : new SwapSpace(swap_fd, 10 * MB)),
      dedupe_enabled_(true),
      dedupe_code_("dedupe code", LengthPrefixedArrayAlloc<uint8_t>(swap_space_.get())),
      dedupe_vmap_table_("dedupe vmap table",
                         LengthPrefixedArrayAlloc<uint8_t>(swap_space_.get())),
      dedupe_cfi_info_("dedupe cfi info", LengthPrefixedArrayAlloc<uint8_t>(swap_space_.get())),
      dedupe_linker_patches_("dedupe cfi info",
                             LengthPrefixedArrayAlloc<linker::LinkerPatch>(swap_space_.get())),
      thunk_map_lock_("thunk_map_lock"),
      thunk_map_(std::less<ThunkMapKey>(), SwapAllocator<ThunkMapValueType>(swap_space_.get())) {
}

CompiledMethodStorage::~CompiledMethodStorage() {
  // All done by member destructors.
}

void CompiledMethodStorage::DumpMemoryUsage(std::ostream& os, bool extended) const {
  if (swap_space_.get() != nullptr) {
    const size_t swap_size = swap_space_->GetSize();
    os << " swap=" << PrettySize(swap_size) << " (" << swap_size << "B)";
  }
  if (extended) {
    Thread* self = Thread::Current();
    os << "\nCode dedupe: " << dedupe_code_.DumpStats(self);
    os << "\nVmap table dedupe: " << dedupe_vmap_table_.DumpStats(self);
    os << "\nCFI info dedupe: " << dedupe_cfi_info_.DumpStats(self);
  }
}

const LengthPrefixedArray<uint8_t>* CompiledMethodStorage::DeduplicateCode(
    const ArrayRef<const uint8_t>& code) {
  return AllocateOrDeduplicateArray(code, &dedupe_code_);
}

void CompiledMethodStorage::ReleaseCode(const LengthPrefixedArray<uint8_t>* code) {
  ReleaseArrayIfNotDeduplicated(code);
}

const LengthPrefixedArray<uint8_t>* CompiledMethodStorage::DeduplicateVMapTable(
    const ArrayRef<const uint8_t>& table) {
  return AllocateOrDeduplicateArray(table, &dedupe_vmap_table_);
}

void CompiledMethodStorage::ReleaseVMapTable(const LengthPrefixedArray<uint8_t>* table) {
  ReleaseArrayIfNotDeduplicated(table);
}

const LengthPrefixedArray<uint8_t>* CompiledMethodStorage::DeduplicateCFIInfo(
    const ArrayRef<const uint8_t>& cfi_info) {
  return AllocateOrDeduplicateArray(cfi_info, &dedupe_cfi_info_);
}

void CompiledMethodStorage::ReleaseCFIInfo(const LengthPrefixedArray<uint8_t>* cfi_info) {
  ReleaseArrayIfNotDeduplicated(cfi_info);
}

const LengthPrefixedArray<linker::LinkerPatch>* CompiledMethodStorage::DeduplicateLinkerPatches(
    const ArrayRef<const linker::LinkerPatch>& linker_patches) {
  return AllocateOrDeduplicateArray(linker_patches, &dedupe_linker_patches_);
}

void CompiledMethodStorage::ReleaseLinkerPatches(
    const LengthPrefixedArray<linker::LinkerPatch>* linker_patches) {
  ReleaseArrayIfNotDeduplicated(linker_patches);
}

CompiledMethodStorage::ThunkMapKey CompiledMethodStorage::GetThunkMapKey(
    const linker::LinkerPatch& linker_patch) {
  uint32_t custom_value1 = 0u;
  uint32_t custom_value2 = 0u;
  switch (linker_patch.GetType()) {
    case linker::LinkerPatch::Type::kBakerReadBarrierBranch:
      custom_value1 = linker_patch.GetBakerCustomValue1();
      custom_value2 = linker_patch.GetBakerCustomValue2();
      break;
    case linker::LinkerPatch::Type::kCallRelative:
      // No custom values.
      break;
    default:
      LOG(FATAL) << "Unexpected patch type: " << linker_patch.GetType();
      UNREACHABLE();
  }
  return ThunkMapKey(linker_patch.GetType(), custom_value1, custom_value2);
}

ArrayRef<const uint8_t> CompiledMethodStorage::GetThunkCode(const linker::LinkerPatch& linker_patch,
                                                            /*out*/ std::string* debug_name) {
  ThunkMapKey key = GetThunkMapKey(linker_patch);
  MutexLock lock(Thread::Current(), thunk_map_lock_);
  auto it = thunk_map_.find(key);
  if (it != thunk_map_.end()) {
    const ThunkMapValue& value = it->second;
    if (debug_name != nullptr) {
      *debug_name = value.GetDebugName();
    }
    return value.GetCode();
  } else {
    if (debug_name != nullptr) {
      *debug_name = std::string();
    }
    return ArrayRef<const uint8_t>();
  }
}

void CompiledMethodStorage::SetThunkCode(const linker::LinkerPatch& linker_patch,
                                         ArrayRef<const uint8_t> code,
                                         const std::string& debug_name) {
  DCHECK(!code.empty());
  ThunkMapKey key = GetThunkMapKey(linker_patch);
  std::vector<uint8_t, SwapAllocator<uint8_t>> code_copy(
      code.begin(), code.end(), SwapAllocator<uint8_t>(swap_space_.get()));
  ThunkMapValue value(std::move(code_copy), debug_name);
  MutexLock lock(Thread::Current(), thunk_map_lock_);
  // Note: Multiple threads can try and compile the same thunk, so this may not create a new entry.
  thunk_map_.emplace(key, std::move(value));
}

}  // namespace art
