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

#ifndef ART_COMPILER_DEX_DEX_TO_DEX_COMPILER_H_
#define ART_COMPILER_DEX_DEX_TO_DEX_COMPILER_H_

#include <set>
#include <unordered_map>
#include <unordered_set>

#include "base/bit_vector.h"
#include "dex/dex_file.h"
#include "dex/invoke_type.h"
#include "handle.h"
#include "method_reference.h"
#include "quicken_info.h"

namespace art {

class CompiledMethod;
class CompilerDriver;
class DexCompilationUnit;

namespace mirror {
class ClassLoader;
}  // namespace mirror

namespace optimizer {

class DexToDexCompiler {
 public:
  enum class CompilationLevel {
    kDontDexToDexCompile,   // Only meaning wrt image time interpretation.
    kOptimize               // Perform peep-hole optimizations.
  };

  explicit DexToDexCompiler(CompilerDriver* driver);

  CompiledMethod* CompileMethod(const DexFile::CodeItem* code_item,
                                uint32_t access_flags,
                                InvokeType invoke_type,
                                uint16_t class_def_idx,
                                uint32_t method_idx,
                                Handle<mirror::ClassLoader> class_loader,
                                const DexFile& dex_file,
                                const CompilationLevel compilation_level) WARN_UNUSED;

  void MarkForCompilation(Thread* self,
                          const MethodReference& method_ref,
                          const DexFile::CodeItem* code_item);

  void ClearState();

  CompilerDriver* GetDriver() {
    return driver_;
  }

  bool ShouldCompileMethod(const MethodReference& ref);

  size_t NumUniqueCodeItems(Thread* self) const;

 private:
  // Holds the state for compiling a single method.
  struct CompilationState {
    struct QuickenedInfo {
      QuickenedInfo(uint32_t pc, uint16_t index) : dex_pc(pc), dex_member_index(index) {}

      uint32_t dex_pc;
      uint16_t dex_member_index;
    };

    CompilationState(DexToDexCompiler* compiler,
                     const DexCompilationUnit& unit,
                     const CompilationLevel compilation_level,
                     const std::vector<uint8_t>* quicken_data);

    const std::vector<QuickenedInfo>& GetQuickenedInfo() const {
      return quickened_info_;
    }

    // Returns the quickening info, or an empty array if it was not quickened.
    // If already_quickened is true, then don't change anything but still return what the quicken
    // data would have been.
    std::vector<uint8_t> Compile();

    const DexFile& GetDexFile() const;

    // Compiles a RETURN-VOID into a RETURN-VOID-BARRIER within a constructor where
    // a barrier is required.
    void CompileReturnVoid(Instruction* inst, uint32_t dex_pc);

    // Compiles a CHECK-CAST into 2 NOP instructions if it is known to be safe. In
    // this case, returns the second NOP instruction pointer. Otherwise, returns
    // the given "inst".
    Instruction* CompileCheckCast(Instruction* inst, uint32_t dex_pc);

    // Compiles a field access into a quick field access.
    // The field index is replaced by an offset within an Object where we can read
    // from / write to this field. Therefore, this does not involve any resolution
    // at runtime.
    // Since the field index is encoded with 16 bits, we can replace it only if the
    // field offset can be encoded with 16 bits too.
    void CompileInstanceFieldAccess(Instruction* inst, uint32_t dex_pc,
                                    Instruction::Code new_opcode, bool is_put);

    // Compiles a virtual method invocation into a quick virtual method invocation.
    // The method index is replaced by the vtable index where the corresponding
    // executable can be found. Therefore, this does not involve any resolution
    // at runtime.
    // Since the method index is encoded with 16 bits, we can replace it only if the
    // vtable index can be encoded with 16 bits too.
    void CompileInvokeVirtual(Instruction* inst, uint32_t dex_pc,
                              Instruction::Code new_opcode, bool is_range);

    // Return the next index.
    uint16_t NextIndex();

    // Returns the dequickened index if an instruction is quickened, otherwise return index.
    uint16_t GetIndexForInstruction(const Instruction* inst, uint32_t index);

    DexToDexCompiler* const compiler_;
    CompilerDriver& driver_;
    const DexCompilationUnit& unit_;
    const CompilationLevel compilation_level_;

    // Filled by the compiler when quickening, in order to encode that information
    // in the .oat file. The runtime will use that information to get to the original
    // opcodes.
    std::vector<QuickenedInfo> quickened_info_;

    // If the code item was already quickened previously.
    const bool already_quickened_;
    const QuickenInfoTable existing_quicken_info_;
    uint32_t quicken_index_ = 0u;

    DISALLOW_COPY_AND_ASSIGN(CompilationState);
  };

  struct QuickenState {
    std::vector<MethodReference> methods_;
    std::vector<uint8_t> quicken_data_;
  };

  BitVector* GetOrAddBitVectorForDex(const DexFile* dex_file) REQUIRES(lock_);

  CompilerDriver* const driver_;

  // State for adding methods (should this be in its own class?).
  const DexFile* active_dex_file_ = nullptr;
  BitVector* active_bit_vector_ = nullptr;

  // Lock that guards duplicate code items and the bitmap.
  mutable Mutex lock_;
  // Record what method references are going to get quickened.
  std::unordered_map<const DexFile*, BitVector> should_quicken_;
  // Record what code items are already seen to detect when multiple methods have the same code
  // item.
  std::unordered_set<const DexFile::CodeItem*> seen_code_items_ GUARDED_BY(lock_);
  // Guarded by lock_ during writing, accessed without a lock during quickening.
  // This is safe because no thread is adding to the shared code items during the quickening phase.
  std::unordered_set<const DexFile::CodeItem*> shared_code_items_;
  std::unordered_set<const DexFile::CodeItem*> blacklisted_code_items_ GUARDED_BY(lock_);
  std::unordered_map<const DexFile::CodeItem*, QuickenState> shared_code_item_quicken_info_
      GUARDED_BY(lock_);
};

std::ostream& operator<<(std::ostream& os, const DexToDexCompiler::CompilationLevel& rhs);

}  // namespace optimizer

}  // namespace art

#endif  // ART_COMPILER_DEX_DEX_TO_DEX_COMPILER_H_
