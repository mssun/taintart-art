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

#ifndef ART_LIBARTBASE_BASE_DATA_HASH_H_
#define ART_LIBARTBASE_BASE_DATA_HASH_H_

#include "base/macros.h"

namespace art {

// Hash bytes using a relatively fast hash.
static inline size_t HashBytes(const uint8_t* data, size_t len) {
  size_t hash = 0x811c9dc5;
  for (uint32_t i = 0; i < len; ++i) {
    hash = (hash * 16777619) ^ data[i];
  }
  hash += hash << 13;
  hash ^= hash >> 7;
  hash += hash << 3;
  hash ^= hash >> 17;
  hash += hash << 5;
  return hash;
}

class DataHash {
 private:
  static constexpr bool kUseMurmur3Hash = true;

 public:
  template <class Container>
  size_t operator()(const Container& array) const {
    // Containers that provide the data() function use contiguous storage.
    const uint8_t* data = reinterpret_cast<const uint8_t*>(array.data());
    uint32_t len = sizeof(typename Container::value_type) * array.size();
    if (kUseMurmur3Hash) {
      static constexpr uint32_t c1 = 0xcc9e2d51;
      static constexpr uint32_t c2 = 0x1b873593;
      static constexpr uint32_t r1 = 15;
      static constexpr uint32_t r2 = 13;
      static constexpr uint32_t m = 5;
      static constexpr uint32_t n = 0xe6546b64;

      uint32_t hash = 0;

      const int nblocks = len / 4;
      typedef __attribute__((__aligned__(1))) uint32_t unaligned_uint32_t;
      const unaligned_uint32_t *blocks = reinterpret_cast<const uint32_t*>(data);
      int i;
      for (i = 0; i < nblocks; i++) {
        uint32_t k = blocks[i];
        k *= c1;
        k = (k << r1) | (k >> (32 - r1));
        k *= c2;

        hash ^= k;
        hash = ((hash << r2) | (hash >> (32 - r2))) * m + n;
      }

      const uint8_t *tail = reinterpret_cast<const uint8_t*>(data + nblocks * 4);
      uint32_t k1 = 0;

      switch (len & 3) {
        case 3:
          k1 ^= tail[2] << 16;
          FALLTHROUGH_INTENDED;
        case 2:
          k1 ^= tail[1] << 8;
          FALLTHROUGH_INTENDED;
        case 1:
          k1 ^= tail[0];

          k1 *= c1;
          k1 = (k1 << r1) | (k1 >> (32 - r1));
          k1 *= c2;
          hash ^= k1;
      }

      hash ^= len;
      hash ^= (hash >> 16);
      hash *= 0x85ebca6b;
      hash ^= (hash >> 13);
      hash *= 0xc2b2ae35;
      hash ^= (hash >> 16);

      return hash;
    } else {
      return HashBytes(data, len);
    }
  }
};

}  // namespace art

#endif  // ART_LIBARTBASE_BASE_DATA_HASH_H_
