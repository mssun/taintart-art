/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include "string_builder_append.h"

#include "base/casts.h"
#include "base/logging.h"
#include "common_throws.h"
#include "gc/heap.h"
#include "mirror/string-alloc-inl.h"
#include "obj_ptr-inl.h"
#include "runtime.h"

namespace art {

class StringBuilderAppend::Builder {
 public:
  Builder(uint32_t format, const uint32_t* args, Thread* self)
      : format_(format),
        args_(args),
        hs_(self) {}

  int32_t CalculateLengthWithFlag() REQUIRES_SHARED(Locks::mutator_lock_);

  void operator()(ObjPtr<mirror::Object> obj, size_t usable_size) const
      REQUIRES_SHARED(Locks::mutator_lock_);

 private:
  static size_t Uint64Length(uint64_t value);

  static size_t Int64Length(int64_t value) {
    uint64_t v = static_cast<uint64_t>(value);
    return (value >= 0) ? Uint64Length(v) : 1u + Uint64Length(-v);
  }

  static size_t RemainingSpace(ObjPtr<mirror::String> new_string, const uint8_t* data)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK(new_string->IsCompressed());
    DCHECK_GE(new_string->GetLength(), data - new_string->GetValueCompressed());
    return new_string->GetLength() - (data - new_string->GetValueCompressed());
  }

  static size_t RemainingSpace(ObjPtr<mirror::String> new_string, const uint16_t* data)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK(!new_string->IsCompressed());
    DCHECK_GE(new_string->GetLength(), data - new_string->GetValue());
    return new_string->GetLength() - (data - new_string->GetValue());
  }

  template <typename CharType, size_t size>
  static CharType* AppendLiteral(ObjPtr<mirror::String> new_string,
                                 CharType* data,
                                 const char (&literal)[size]) REQUIRES_SHARED(Locks::mutator_lock_);

  template <typename CharType>
  static CharType* AppendString(ObjPtr<mirror::String> new_string,
                                CharType* data,
                                ObjPtr<mirror::String> str) REQUIRES_SHARED(Locks::mutator_lock_);

  template <typename CharType>
  static CharType* AppendInt64(ObjPtr<mirror::String> new_string,
                               CharType* data,
                               int64_t value) REQUIRES_SHARED(Locks::mutator_lock_);

  template <typename CharType>
  void StoreData(ObjPtr<mirror::String> new_string, CharType* data) const
      REQUIRES_SHARED(Locks::mutator_lock_);

  static constexpr char kNull[] = "null";
  static constexpr size_t kNullLength = sizeof(kNull) - 1u;
  static constexpr char kTrue[] = "true";
  static constexpr size_t kTrueLength = sizeof(kTrue) - 1u;
  static constexpr char kFalse[] = "false";
  static constexpr size_t kFalseLength = sizeof(kFalse) - 1u;

  // The format and arguments to append.
  const uint32_t format_;
  const uint32_t* const args_;

  // References are moved to the handle scope during CalculateLengthWithFlag().
  StackHandleScope<kMaxArgs> hs_;

  // The length and flag to store when the AppendBuilder is used as a pre-fence visitor.
  int32_t length_with_flag_ = 0u;
};

inline size_t StringBuilderAppend::Builder::Uint64Length(uint64_t value)  {
  if (value == 0u) {
    return 1u;
  }
  // Calculate floor(log2(value)).
  size_t log2_value = BitSizeOf<uint64_t>() - 1u - CLZ(value);
  // Calculate an estimate of floor(log10(value)).
  //   log10(2) = 0.301029996 > 0.296875 = 19/64
  //   floor(log10(v)) == floor(log2(v) * log10(2))
  //                   >= floor(log2(v) * 19/64)
  //                   >= floor(floor(log2(v)) * 19/64)
  // This estimate is no more that one off from the actual value because log2(value) < 64 and thus
  //   log2(v) * log10(2) - log2(v) * 19/64 < 64*(log10(2) - 19/64)
  // for the first approximation and
  //   log2(v) * 19/64 - floor(log2(v)) * 19/64 < 19/64
  // for the second one. Together,
  //   64*(log10(2) - 19/64) + 19/64 = 0.56278 < 1 .
  size_t log10_value_estimate = log2_value * 19u / 64u;
  static constexpr uint64_t bounds[] = {
      UINT64_C(9),
      UINT64_C(99),
      UINT64_C(999),
      UINT64_C(9999),
      UINT64_C(99999),
      UINT64_C(999999),
      UINT64_C(9999999),
      UINT64_C(99999999),
      UINT64_C(999999999),
      UINT64_C(9999999999),
      UINT64_C(99999999999),
      UINT64_C(999999999999),
      UINT64_C(9999999999999),
      UINT64_C(99999999999999),
      UINT64_C(999999999999999),
      UINT64_C(9999999999999999),
      UINT64_C(99999999999999999),
      UINT64_C(999999999999999999),
      UINT64_C(9999999999999999999),
  };
  // Add 1 for the lowest digit, add another 1 if the estimate was too low.
  DCHECK_LT(log10_value_estimate, std::size(bounds));
  size_t adjustment = (value > bounds[log10_value_estimate]) ? 2u : 1u;
  return log10_value_estimate + adjustment;
}

template <typename CharType, size_t size>
inline CharType* StringBuilderAppend::Builder::AppendLiteral(ObjPtr<mirror::String> new_string,
                                                             CharType* data,
                                                             const char (&literal)[size]) {
  static_assert(size >= 2, "We need something to append.");

  // Literals are zero-terminated.
  constexpr size_t length = size - 1u;
  DCHECK_EQ(literal[length], '\0');

  DCHECK_LE(length, RemainingSpace(new_string, data));
  for (size_t i = 0; i != length; ++i) {
    data[i] = literal[i];
  }
  return data + length;
}

template <typename CharType>
inline CharType* StringBuilderAppend::Builder::AppendString(ObjPtr<mirror::String> new_string,
                                                            CharType* data,
                                                            ObjPtr<mirror::String> str) {
  size_t length = dchecked_integral_cast<size_t>(str->GetLength());
  DCHECK_LE(length, RemainingSpace(new_string, data));
  if (sizeof(CharType) == sizeof(uint8_t) || str->IsCompressed()) {
    DCHECK(str->IsCompressed());
    const uint8_t* value = str->GetValueCompressed();
    for (size_t i = 0; i != length; ++i) {
      data[i] = value[i];
    }
  } else {
    const uint16_t* value = str->GetValue();
    for (size_t i = 0; i != length; ++i) {
      data[i] = dchecked_integral_cast<CharType>(value[i]);
    }
  }
  return data + length;
}

template <typename CharType>
inline CharType* StringBuilderAppend::Builder::AppendInt64(ObjPtr<mirror::String> new_string,
                                                           CharType* data,
                                                           int64_t value) {
  DCHECK_GE(RemainingSpace(new_string, data), Int64Length(value));
  uint64_t v = static_cast<uint64_t>(value);
  if (value < 0) {
    *data = '-';
    ++data;
    v = -v;
  }
  size_t length = Uint64Length(v);
  // Write the digits from the end, do not write the most significant digit
  // in the loop to avoid an unnecessary division.
  for (size_t i = 1; i != length; ++i) {
    uint64_t digit = v % UINT64_C(10);
    v /= UINT64_C(10);
    data[length - i] = '0' + static_cast<char>(digit);
  }
  DCHECK_LE(v, 10u);
  *data = '0' + static_cast<char>(v);
  return data + length;
}

inline int32_t StringBuilderAppend::Builder::CalculateLengthWithFlag() {
  static_assert(static_cast<size_t>(Argument::kEnd) == 0u, "kEnd must be 0.");
  bool compressible = mirror::kUseStringCompression;
  uint64_t length = 0u;
  const uint32_t* current_arg = args_;
  for (uint32_t f = format_; f != 0u; f >>= kBitsPerArg) {
    DCHECK_LE(f & kArgMask, static_cast<uint32_t>(Argument::kLast));
    switch (static_cast<Argument>(f & kArgMask)) {
      case Argument::kString: {
        Handle<mirror::String> str =
            hs_.NewHandle(reinterpret_cast32<mirror::String*>(*current_arg));
        if (str != nullptr) {
          length += str->GetLength();
          compressible = compressible && str->IsCompressed();
        } else {
          length += kNullLength;
        }
        break;
      }
      case Argument::kBoolean: {
        length += (*current_arg != 0u) ? kTrueLength : kFalseLength;
        break;
      }
      case Argument::kChar: {
        length += 1u;
        compressible = compressible &&
            mirror::String::IsASCII(reinterpret_cast<const uint16_t*>(current_arg)[0]);
        break;
      }
      case Argument::kInt: {
        length += Int64Length(static_cast<int32_t>(*current_arg));
        break;
      }
      case Argument::kLong: {
        current_arg = AlignUp(current_arg, sizeof(int64_t));
        length += Int64Length(*reinterpret_cast<const int64_t*>(current_arg));
        ++current_arg;  // Skip the low word, let the common code skip the high word.
        break;
      }

      case Argument::kStringBuilder:
      case Argument::kCharArray:
      case Argument::kObject:
      case Argument::kFloat:
      case Argument::kDouble:
        LOG(FATAL) << "Unimplemented arg format: 0x" << std::hex
            << (f & kArgMask) << " full format: 0x" << std::hex << format_;
        UNREACHABLE();
      default:
        LOG(FATAL) << "Unexpected arg format: 0x" << std::hex
            << (f & kArgMask) << " full format: 0x" << std::hex << format_;
        UNREACHABLE();
    }
    ++current_arg;
    DCHECK_LE(hs_.NumberOfReferences(), kMaxArgs);
  }

  if (length > std::numeric_limits<int32_t>::max()) {
    // We cannot allocate memory for the entire result.
    hs_.Self()->ThrowNewException("Ljava/lang/OutOfMemoryError;",
                                  "Out of memory for StringBuilder append.");
    return -1;
  }

  length_with_flag_ = mirror::String::GetFlaggedCount(length, compressible);
  return length_with_flag_;
}

template <typename CharType>
inline void StringBuilderAppend::Builder::StoreData(ObjPtr<mirror::String> new_string,
                                                    CharType* data) const {
  size_t handle_index = 0u;
  const uint32_t* current_arg = args_;
  for (uint32_t f = format_; f != 0u; f >>= kBitsPerArg) {
    DCHECK_LE(f & kArgMask, static_cast<uint32_t>(Argument::kLast));
    switch (static_cast<Argument>(f & kArgMask)) {
      case Argument::kString: {
        ObjPtr<mirror::String> str =
            ObjPtr<mirror::String>::DownCast(hs_.GetReference(handle_index));
        ++handle_index;
        if (str != nullptr) {
          data = AppendString(new_string, data, str);
        } else {
          data = AppendLiteral(new_string, data, kNull);
        }
        break;
      }
      case Argument::kBoolean: {
        if (*current_arg != 0u) {
          data = AppendLiteral(new_string, data, kTrue);
        } else {
          data = AppendLiteral(new_string, data, kFalse);
        }
        break;
      }
      case Argument::kChar: {
        DCHECK_GE(RemainingSpace(new_string, data), 1u);
        *data = *reinterpret_cast<const CharType*>(current_arg);
        ++data;
        break;
      }
      case Argument::kInt: {
        data = AppendInt64(new_string, data, static_cast<int32_t>(*current_arg));
        break;
      }
      case Argument::kLong: {
        current_arg = AlignUp(current_arg, sizeof(int64_t));
        data = AppendInt64(new_string, data, *reinterpret_cast<const int64_t*>(current_arg));
        ++current_arg;  // Skip the low word, let the common code skip the high word.
        break;
      }

      case Argument::kStringBuilder:
      case Argument::kCharArray:
      case Argument::kFloat:
      case Argument::kDouble:
        LOG(FATAL) << "Unimplemented arg format: 0x" << std::hex
            << (f & kArgMask) << " full format: 0x" << std::hex << format_;
        UNREACHABLE();
      default:
        LOG(FATAL) << "Unexpected arg format: 0x" << std::hex
            << (f & kArgMask) << " full format: 0x" << std::hex << format_;
        UNREACHABLE();
    }
    ++current_arg;
    DCHECK_LE(handle_index, hs_.NumberOfReferences());
  }
  DCHECK_EQ(RemainingSpace(new_string, data), 0u) << std::hex << format_;
}

inline void StringBuilderAppend::Builder::operator()(ObjPtr<mirror::Object> obj,
                                                     size_t usable_size ATTRIBUTE_UNUSED) const {
  ObjPtr<mirror::String> new_string = ObjPtr<mirror::String>::DownCast(obj);
  new_string->SetCount(length_with_flag_);
  if (mirror::String::IsCompressed(length_with_flag_)) {
    StoreData(new_string, new_string->GetValueCompressed());
  } else {
    StoreData(new_string, new_string->GetValue());
  }
}

ObjPtr<mirror::String> StringBuilderAppend::AppendF(uint32_t format,
                                                    const uint32_t* args,
                                                    Thread* self) {
  Builder builder(format, args, self);
  self->AssertNoPendingException();
  int32_t length_with_flag = builder.CalculateLengthWithFlag();
  if (self->IsExceptionPending()) {
    return nullptr;
  }
  gc::AllocatorType allocator_type = Runtime::Current()->GetHeap()->GetCurrentAllocator();
  ObjPtr<mirror::String> result = mirror::String::Alloc</*kIsInstrumented=*/ true>(
      self, length_with_flag, allocator_type, builder);

  return result;
}

}  // namespace art
