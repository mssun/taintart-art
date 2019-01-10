/*
 * Copyright (C) 2011 The Android Open Source Project
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
#ifndef ART_RUNTIME_MIRROR_STRING_INL_H_
#define ART_RUNTIME_MIRROR_STRING_INL_H_

#include "string.h"

#include "android-base/stringprintf.h"

#include "class-inl.h"
#include "common_throws.h"
#include "dex/utf.h"
#include "runtime_globals.h"

namespace art {
namespace mirror {

inline uint32_t String::ClassSize(PointerSize pointer_size) {
#ifdef USE_D8_DESUGAR
  // Two lambdas in CharSequence:
  //   lambda$chars$0$CharSequence
  //   lambda$codePoints$1$CharSequence
  // which were virtual functions in standalone desugar, becomes
  // direct functions with D8 desugaring.
  uint32_t vtable_entries = Object::kVTableLength + 54;
#else
  uint32_t vtable_entries = Object::kVTableLength + 56;
#endif
  return Class::ComputeClassSize(true, vtable_entries, 0, 0, 0, 1, 2, pointer_size);
}

inline uint16_t String::CharAt(int32_t index) {
  int32_t count = GetLength();
  if (UNLIKELY((index < 0) || (index >= count))) {
    ThrowStringIndexOutOfBoundsException(index, count);
    return 0;
  }
  if (IsCompressed()) {
    return GetValueCompressed()[index];
  } else {
    return GetValue()[index];
  }
}

template <typename MemoryType>
int32_t String::FastIndexOf(MemoryType* chars, int32_t ch, int32_t start) {
  const MemoryType* p = chars + start;
  const MemoryType* end = chars + GetLength();
  while (p < end) {
    if (*p++ == ch) {
      return (p - 1) - chars;
    }
  }
  return -1;
}

inline int32_t String::GetHashCode() {
  int32_t result = GetField32(OFFSET_OF_OBJECT_MEMBER(String, hash_code_));
  if (UNLIKELY(result == 0)) {
    result = ComputeHashCode();
  }
  if (kIsDebugBuild) {
    if (IsCompressed()) {
      DCHECK(result != 0 || ComputeUtf16Hash(GetValueCompressed(), GetLength()) == 0)
          << ToModifiedUtf8() << " " << result;
    } else {
      DCHECK(result != 0 || ComputeUtf16Hash(GetValue(), GetLength()) == 0)
          << ToModifiedUtf8() << " " << result;
    }
  }
  return result;
}

template<typename MemoryType>
inline bool String::AllASCII(const MemoryType* chars, const int length) {
  static_assert(std::is_unsigned<MemoryType>::value, "Expecting unsigned MemoryType");
  for (int i = 0; i < length; ++i) {
    if (!IsASCII(chars[i])) {
      return false;
    }
  }
  return true;
}

inline bool String::DexFileStringAllASCII(const char* chars, const int length) {
  // For strings from the dex file we just need to check that
  // the terminating character is at the right position.
  DCHECK_EQ(AllASCII(reinterpret_cast<const uint8_t*>(chars), length), chars[length] == 0);
  return chars[length] == 0;
}

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_STRING_INL_H_
