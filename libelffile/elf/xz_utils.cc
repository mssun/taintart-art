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

#include "xz_utils.h"

#include <vector>
#include <mutex>

#include "base/array_ref.h"
#include "base/bit_utils.h"
#include "base/leb128.h"
#include "dwarf/writer.h"

// liblzma.
#include "7zCrc.h"
#include "Xz.h"
#include "XzCrc64.h"
#include "XzEnc.h"

namespace art {

constexpr size_t kChunkSize = 16 * KB;

static void XzInitCrc() {
  static std::once_flag crc_initialized;
  std::call_once(crc_initialized, []() {
    CrcGenerateTable();
    Crc64GenerateTable();
  });
}

void XzCompress(ArrayRef<const uint8_t> src, std::vector<uint8_t>* dst, int level) {
  // Configure the compression library.
  XzInitCrc();
  CLzma2EncProps lzma2Props;
  Lzma2EncProps_Init(&lzma2Props);
  lzma2Props.lzmaProps.level = level;
  lzma2Props.lzmaProps.reduceSize = src.size();  // Size of data that will be compressed.
  lzma2Props.blockSize = kChunkSize;
  Lzma2EncProps_Normalize(&lzma2Props);
  CXzProps props;
  XzProps_Init(&props);
  props.lzma2Props = lzma2Props;
  // Implement the required interface for communication (written in C so no virtual methods).
  struct XzCallbacks : public ISeqInStream, public ISeqOutStream, public ICompressProgress {
    static SRes ReadImpl(const ISeqInStream* p, void* buf, size_t* size) {
      auto* ctx = static_cast<XzCallbacks*>(const_cast<ISeqInStream*>(p));
      *size = std::min(*size, ctx->src_.size() - ctx->src_pos_);
      memcpy(buf, ctx->src_.data() + ctx->src_pos_, *size);
      ctx->src_pos_ += *size;
      return SZ_OK;
    }
    static size_t WriteImpl(const ISeqOutStream* p, const void* buf, size_t size) {
      auto* ctx = static_cast<const XzCallbacks*>(p);
      const uint8_t* buffer = reinterpret_cast<const uint8_t*>(buf);
      ctx->dst_->insert(ctx->dst_->end(), buffer, buffer + size);
      return size;
    }
    static SRes ProgressImpl(const ICompressProgress* , UInt64, UInt64) {
      return SZ_OK;
    }
    size_t src_pos_;
    ArrayRef<const uint8_t> src_;
    std::vector<uint8_t>* dst_;
  };
  XzCallbacks callbacks;
  callbacks.Read = XzCallbacks::ReadImpl;
  callbacks.Write = XzCallbacks::WriteImpl;
  callbacks.Progress = XzCallbacks::ProgressImpl;
  callbacks.src_pos_ = 0;
  callbacks.src_ = src;
  callbacks.dst_ = dst;
  // Compress.
  SRes res = Xz_Encode(&callbacks, &callbacks, &props, &callbacks);
  CHECK_EQ(res, SZ_OK);

  // Decompress the data back and check that we get the original.
  if (kIsDebugBuild) {
    std::vector<uint8_t> decompressed;
    XzDecompress(ArrayRef<const uint8_t>(*dst), &decompressed);
    DCHECK_EQ(decompressed.size(), src.size());
    DCHECK_EQ(memcmp(decompressed.data(), src.data(), src.size()), 0);
  }
}

void XzDecompress(ArrayRef<const uint8_t> src, std::vector<uint8_t>* dst) {
  XzInitCrc();
  std::unique_ptr<CXzUnpacker> state(new CXzUnpacker());
  ISzAlloc alloc;
  alloc.Alloc = [](ISzAllocPtr, size_t size) { return malloc(size); };
  alloc.Free = [](ISzAllocPtr, void* ptr) { return free(ptr); };
  XzUnpacker_Construct(state.get(), &alloc);

  size_t src_offset = 0;
  size_t dst_offset = 0;
  ECoderStatus status;
  do {
    dst->resize(RoundUp(dst_offset + kPageSize / 4, kPageSize));
    size_t src_remaining = src.size() - src_offset;
    size_t dst_remaining = dst->size() - dst_offset;
    int return_val = XzUnpacker_Code(state.get(),
                                     dst->data() + dst_offset,
                                     &dst_remaining,
                                     src.data() + src_offset,
                                     &src_remaining,
                                     true,
                                     CODER_FINISH_ANY,
                                     &status);
    CHECK_EQ(return_val, SZ_OK);
    src_offset += src_remaining;
    dst_offset += dst_remaining;
  } while (status == CODER_STATUS_NOT_FINISHED);
  CHECK_EQ(src_offset, src.size());
  CHECK(XzUnpacker_IsStreamWasFinished(state.get()));
  XzUnpacker_Free(state.get());
  dst->resize(dst_offset);
}

}  // namespace art
