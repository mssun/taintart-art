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

#ifndef ART_RUNTIME_VERIFIER_METHOD_VERIFIER_H_
#define ART_RUNTIME_VERIFIER_METHOD_VERIFIER_H_

#include <memory>
#include <sstream>
#include <vector>

#include <android-base/logging.h>

#include "base/arena_allocator.h"
#include "base/macros.h"
#include "base/scoped_arena_containers.h"
#include "base/value_object.h"
#include "dex/code_item_accessors.h"
#include "dex/dex_file_types.h"
#include "dex/method_reference.h"
#include "handle.h"
#include "instruction_flags.h"
#include "reg_type_cache.h"
#include "register_line.h"
#include "verifier_enums.h"

namespace art {

class ClassLinker;
class CompilerCallbacks;
class DexFile;
class Instruction;
struct ReferenceMap2Visitor;
class Thread;
class VariableIndentationOutputStream;

namespace dex {
struct ClassDef;
struct CodeItem;
}  // namespace dex

namespace mirror {
class DexCache;
}  // namespace mirror

namespace verifier {

class MethodVerifier;
class RegisterLine;
using RegisterLineArenaUniquePtr = std::unique_ptr<RegisterLine, RegisterLineArenaDelete>;
class RegType;
struct ScopedNewLine;

// We don't need to store the register data for many instructions, because we either only need
// it at branch points (for verification) or GC points and branches (for verification +
// type-precise register analysis).
enum RegisterTrackingMode {
  kTrackRegsBranches,
  kTrackCompilerInterestPoints,
  kTrackRegsAll,
};

// A mapping from a dex pc to the register line statuses as they are immediately prior to the
// execution of that instruction.
class PcToRegisterLineTable {
 public:
  explicit PcToRegisterLineTable(ScopedArenaAllocator& allocator);
  ~PcToRegisterLineTable();

  // Initialize the RegisterTable. Every instruction address can have a different set of information
  // about what's in which register, but for verification purposes we only need to store it at
  // branch target addresses (because we merge into that).
  void Init(RegisterTrackingMode mode,
            InstructionFlags* flags,
            uint32_t insns_size,
            uint16_t registers_size,
            ScopedArenaAllocator& allocator,
            RegTypeCache* reg_types);

  bool IsInitialized() const {
    return !register_lines_.empty();
  }

  RegisterLine* GetLine(size_t idx) const {
    return register_lines_[idx].get();
  }

 private:
  ScopedArenaVector<RegisterLineArenaUniquePtr> register_lines_;

  DISALLOW_COPY_AND_ASSIGN(PcToRegisterLineTable);
};

// The verifier
class MethodVerifier {
 public:
  static MethodVerifier* VerifyMethodAndDump(Thread* self,
                                             VariableIndentationOutputStream* vios,
                                             uint32_t method_idx,
                                             const DexFile* dex_file,
                                             Handle<mirror::DexCache> dex_cache,
                                             Handle<mirror::ClassLoader> class_loader,
                                             const dex::ClassDef& class_def,
                                             const dex::CodeItem* code_item, ArtMethod* method,
                                             uint32_t method_access_flags,
                                             uint32_t api_level)
      REQUIRES_SHARED(Locks::mutator_lock_);

  const DexFile& GetDexFile() const {
    DCHECK(dex_file_ != nullptr);
    return *dex_file_;
  }

  RegTypeCache* GetRegTypeCache() {
    return &reg_types_;
  }

  // Log a verification failure.
  std::ostream& Fail(VerifyError error);

  // Log for verification information.
  ScopedNewLine LogVerifyInfo();

  // Information structure for a lock held at a certain point in time.
  struct DexLockInfo {
    // The registers aliasing the lock.
    std::set<uint32_t> dex_registers;
    // The dex PC of the monitor-enter instruction.
    uint32_t dex_pc;

    explicit DexLockInfo(uint32_t dex_pc_in) {
      dex_pc = dex_pc_in;
    }
  };
  // Fills 'monitor_enter_dex_pcs' with the dex pcs of the monitor-enter instructions corresponding
  // to the locks held at 'dex_pc' in method 'm'.
  // Note: this is the only situation where the verifier will visit quickened instructions.
  static void FindLocksAtDexPc(ArtMethod* m,
                               uint32_t dex_pc,
                               std::vector<DexLockInfo>* monitor_enter_dex_pcs,
                               uint32_t api_level)
      REQUIRES_SHARED(Locks::mutator_lock_);

  static void Init() REQUIRES_SHARED(Locks::mutator_lock_);
  static void Shutdown();

  virtual ~MethodVerifier();

  static void VisitStaticRoots(RootVisitor* visitor)
      REQUIRES_SHARED(Locks::mutator_lock_);
  void VisitRoots(RootVisitor* visitor, const RootInfo& roots)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Accessors used by the compiler via CompilerCallback
  const CodeItemDataAccessor& CodeItem() const {
    return code_item_accessor_;
  }
  RegisterLine* GetRegLine(uint32_t dex_pc);
  ALWAYS_INLINE const InstructionFlags& GetInstructionFlags(size_t index) const;

  MethodReference GetMethodReference() const;
  bool HasCheckCasts() const;
  bool HasFailures() const;
  bool HasInstructionThatWillThrow() const {
    return have_any_pending_runtime_throw_failure_;
  }

  virtual const RegType& ResolveCheckedClass(dex::TypeIndex class_idx)
      REQUIRES_SHARED(Locks::mutator_lock_) = 0;

  uint32_t GetEncounteredFailureTypes() {
    return encountered_failure_types_;
  }

 protected:
  MethodVerifier(Thread* self,
                 const DexFile* dex_file,
                 const dex::CodeItem* code_item,
                 uint32_t dex_method_idx,
                 bool can_load_classes,
                 bool allow_thread_suspension,
                 bool allow_soft_failures)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Verification result for method(s). Includes a (maximum) failure kind, and (the union of)
  // all failure types.
  struct FailureData : ValueObject {
    FailureKind kind = FailureKind::kNoFailure;
    uint32_t types = 0U;

    // Merge src into this. Uses the most severe failure kind, and the union of types.
    void Merge(const FailureData& src);
  };

  /*
   * Perform verification on a single method.
   *
   * We do this in three passes:
   *  (1) Walk through all code units, determining instruction locations,
   *      widths, and other characteristics.
   *  (2) Walk through all code units, performing static checks on
   *      operands.
   *  (3) Iterate through the method, checking type safety and looking
   *      for code flow problems.
   */
  static FailureData VerifyMethod(Thread* self,
                                  uint32_t method_idx,
                                  const DexFile* dex_file,
                                  Handle<mirror::DexCache> dex_cache,
                                  Handle<mirror::ClassLoader> class_loader,
                                  const dex::ClassDef& class_def_idx,
                                  const dex::CodeItem* code_item,
                                  ArtMethod* method,
                                  uint32_t method_access_flags,
                                  CompilerCallbacks* callbacks,
                                  bool allow_soft_failures,
                                  HardFailLogMode log_level,
                                  bool need_precise_constants,
                                  uint32_t api_level,
                                  std::string* hard_failure_msg)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template <bool kVerifierDebug>
  static FailureData VerifyMethod(Thread* self,
                                  uint32_t method_idx,
                                  const DexFile* dex_file,
                                  Handle<mirror::DexCache> dex_cache,
                                  Handle<mirror::ClassLoader> class_loader,
                                  const dex::ClassDef& class_def_idx,
                                  const dex::CodeItem* code_item,
                                  ArtMethod* method,
                                  uint32_t method_access_flags,
                                  CompilerCallbacks* callbacks,
                                  bool allow_soft_failures,
                                  HardFailLogMode log_level,
                                  bool need_precise_constants,
                                  uint32_t api_level,
                                  std::string* hard_failure_msg)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // For VerifierDepsTest. TODO: Refactor.

  // Run verification on the method. Returns true if verification completes and false if the input
  // has an irrecoverable corruption.
  virtual bool Verify() REQUIRES_SHARED(Locks::mutator_lock_) = 0;
  static MethodVerifier* CreateVerifier(Thread* self,
                                        const DexFile* dex_file,
                                        Handle<mirror::DexCache> dex_cache,
                                        Handle<mirror::ClassLoader> class_loader,
                                        const dex::ClassDef& class_def,
                                        const dex::CodeItem* code_item,
                                        uint32_t method_idx,
                                        ArtMethod* method,
                                        uint32_t access_flags,
                                        bool can_load_classes,
                                        bool allow_soft_failures,
                                        bool need_precise_constants,
                                        bool verify_to_dump,
                                        bool allow_thread_suspension,
                                        uint32_t api_level)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // The thread we're verifying on.
  Thread* const self_;

  // Arena allocator.
  ArenaStack arena_stack_;
  ScopedArenaAllocator allocator_;

  RegTypeCache reg_types_;

  PcToRegisterLineTable reg_table_;

  // Storage for the register status we're currently working on.
  RegisterLineArenaUniquePtr work_line_;

  // The address of the instruction we're currently working on, note that this is in 2 byte
  // quantities
  uint32_t work_insn_idx_;

  // Storage for the register status we're saving for later.
  RegisterLineArenaUniquePtr saved_line_;

  const uint32_t dex_method_idx_;  // The method we're working on.
  const DexFile* const dex_file_;  // The dex file containing the method.
  const CodeItemDataAccessor code_item_accessor_;

  // Instruction widths and flags, one entry per code unit.
  // Owned, but not unique_ptr since insn_flags_ are allocated in arenas.
  ArenaUniquePtr<InstructionFlags[]> insn_flags_;

  // The types of any error that occurs.
  std::vector<VerifyError> failures_;
  // Error messages associated with failures.
  std::vector<std::ostringstream*> failure_messages_;
  // Is there a pending hard failure?
  bool have_pending_hard_failure_;
  // Is there a pending runtime throw failure? A runtime throw failure is when an instruction
  // would fail at runtime throwing an exception. Such an instruction causes the following code
  // to be unreachable. This is set by Fail and used to ensure we don't process unreachable
  // instructions that would hard fail the verification.
  // Note: this flag is reset after processing each instruction.
  bool have_pending_runtime_throw_failure_;
  // Is there a pending experimental failure?
  bool have_pending_experimental_failure_;

  // A version of the above that is not reset and thus captures if there were *any* throw failures.
  bool have_any_pending_runtime_throw_failure_;

  // Info message log use primarily for verifier diagnostics.
  std::ostringstream info_messages_;

  // Bitset of the encountered failure types. Bits are according to the values in VerifyError.
  uint32_t encountered_failure_types_;

  const bool can_load_classes_;

  // Converts soft failures to hard failures when false. Only false when the compiler isn't
  // running and the verifier is called from the class linker.
  const bool allow_soft_failures_;

  // Indicates the method being verified contains at least one check-cast or aput-object
  // instruction. Aput-object operations implicitly check for array-store exceptions, similar to
  // check-cast.
  bool has_check_casts_;

  // Link, for the method verifier root linked list.
  MethodVerifier* link_;

  friend class art::Thread;
  friend class ClassVerifier;
  friend class VerifierDepsTest;

  DISALLOW_COPY_AND_ASSIGN(MethodVerifier);
};

}  // namespace verifier
}  // namespace art

#endif  // ART_RUNTIME_VERIFIER_METHOD_VERIFIER_H_
