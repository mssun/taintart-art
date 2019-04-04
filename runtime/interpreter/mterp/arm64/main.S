%def header():
/*
 * Copyright (C) 2016 The Android Open Source Project
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

/*
  Art assembly interpreter notes:

  First validate assembly code by implementing ExecuteXXXImpl() style body (doesn't
  handle invoke, allows higher-level code to create frame & shadow frame.

  Once that's working, support direct entry code & eliminate shadow frame (and
  excess locals allocation.

  Some (hopefully) temporary ugliness.  We'll treat xFP as pointing to the
  base of the vreg array within the shadow frame.  Access the other fields,
  dex_pc_, method_ and number_of_vregs_ via negative offsets.  For now, we'll continue
  the shadow frame mechanism of double-storing object references - via xFP &
  number_of_vregs_.

 */

/*
ARM64 Runtime register usage conventions.

  r0     : w0 is 32-bit return register and x0 is 64-bit.
  r0-r7  : Argument registers.
  r8-r15 : Caller save registers (used as temporary registers).
  r16-r17: Also known as ip0-ip1, respectively. Used as scratch registers by
           the linker, by the trampolines and other stubs (the backend uses
           these as temporary registers).
  r18    : Caller save register (used as temporary register).
  r19    : Pointer to thread-local storage.
  r20-r29: Callee save registers.
  r30    : (lr) is reserved (the link register).
  rsp    : (sp) is reserved (the stack pointer).
  rzr    : (zr) is reserved (the zero register).

  Floating-point registers
  v0-v31

  v0     : s0 is return register for singles (32-bit) and d0 for doubles (64-bit).
           This is analogous to the C/C++ (hard-float) calling convention.
  v0-v7  : Floating-point argument registers in both Dalvik and C/C++ conventions.
           Also used as temporary and codegen scratch registers.

  v0-v7 and v16-v31 : trashed across C calls.
  v8-v15 : bottom 64-bits preserved across C calls (d8-d15 are preserved).

  v16-v31: Used as codegen temp/scratch.
  v8-v15 : Can be used for promotion.

  Must maintain 16-byte stack alignment.

Mterp notes:

The following registers have fixed assignments:

  reg nick      purpose
  x20  xPC       interpreted program counter, used for fetching instructions
  x21  xFP       interpreted frame pointer, used for accessing locals and args
  x22  xSELF     self (Thread) pointer
  x23  xINST     first 16-bit code unit of current instruction
  x24  xIBASE    interpreted instruction base pointer, used for computed goto
  x25  xREFS     base of object references in shadow frame  (ideally, we'll get rid of this later).
  x26  wPROFILE  jit profile hotness countdown
  x16  ip        scratch reg
  x17  ip2       scratch reg (used by macros)

Macros are provided for common operations.  They MUST NOT alter unspecified registers or condition
codes.
*/

/*
 * This is a #include, not a %include, because we want the C pre-processor
 * to expand the macros into assembler assignment statements.
 */
#include "asm_support.h"
#include "interpreter/cfi_asm_support.h"

#define MTERP_PROFILE_BRANCHES 1
#define MTERP_LOGGING 0

/* During bringup, we'll use the shadow frame model instead of xFP */
/* single-purpose registers, given names for clarity */
#define xPC      x20
#define CFI_DEX  20 // DWARF register number of the register holding dex-pc (xPC).
#define CFI_TMP  0  // DWARF register number of the first argument register (r0).
#define xFP      x21
#define xSELF    x22
#define xINST    x23
#define wINST    w23
#define xIBASE   x24
#define xREFS    x25
#define wPROFILE w26
#define xPROFILE x26
#define ip       x16
#define ip2      x17

/*
 * Instead of holding a pointer to the shadow frame, we keep xFP at the base of the vregs.  So,
 * to access other shadow frame fields, we need to use a backwards offset.  Define those here.
 */
#define OFF_FP(a) (a - SHADOWFRAME_VREGS_OFFSET)
#define OFF_FP_NUMBER_OF_VREGS OFF_FP(SHADOWFRAME_NUMBER_OF_VREGS_OFFSET)
#define OFF_FP_DEX_PC OFF_FP(SHADOWFRAME_DEX_PC_OFFSET)
#define OFF_FP_LINK OFF_FP(SHADOWFRAME_LINK_OFFSET)
#define OFF_FP_METHOD OFF_FP(SHADOWFRAME_METHOD_OFFSET)
#define OFF_FP_RESULT_REGISTER OFF_FP(SHADOWFRAME_RESULT_REGISTER_OFFSET)
#define OFF_FP_DEX_PC_PTR OFF_FP(SHADOWFRAME_DEX_PC_PTR_OFFSET)
#define OFF_FP_DEX_INSTRUCTIONS OFF_FP(SHADOWFRAME_DEX_INSTRUCTIONS_OFFSET)
#define OFF_FP_SHADOWFRAME OFF_FP(0)

/*
 * "export" the PC to dex_pc field in the shadow frame, f/b/o future exception objects.  Must
 * be done *before* something throws.
 *
 * It's okay to do this more than once.
 *
 * NOTE: the fast interpreter keeps track of dex pc as a direct pointer to the mapped
 * dex byte codes.  However, the rest of the runtime expects dex pc to be an instruction
 * offset into the code_items_[] array.  For effiency, we will "export" the
 * current dex pc as a direct pointer using the EXPORT_PC macro, and rely on GetDexPC
 * to convert to a dex pc when needed.
 */
.macro EXPORT_PC
    str  xPC, [xFP, #OFF_FP_DEX_PC_PTR]
.endm

/*
 * Fetch the next instruction from xPC into wINST.  Does not advance xPC.
 */
.macro FETCH_INST
    ldrh    wINST, [xPC]
.endm

/*
 * Fetch the next instruction from the specified offset.  Advances xPC
 * to point to the next instruction.  "_count" is in 16-bit code units.
 *
 * Because of the limited size of immediate constants on ARM, this is only
 * suitable for small forward movements (i.e. don't try to implement "goto"
 * with this).
 *
 * This must come AFTER anything that can throw an exception, or the
 * exception catch may miss.  (This also implies that it must come after
 * EXPORT_PC.)
 */
.macro FETCH_ADVANCE_INST count
    ldrh    wINST, [xPC, #((\count)*2)]!
.endm

/*
 * The operation performed here is similar to FETCH_ADVANCE_INST, except the
 * src and dest registers are parameterized (not hard-wired to xPC and xINST).
 */
.macro PREFETCH_ADVANCE_INST dreg, sreg, count
    ldrh    \dreg, [\sreg, #((\count)*2)]!
.endm

/*
 * Similar to FETCH_ADVANCE_INST, but does not update xPC.  Used to load
 * xINST ahead of possible exception point.  Be sure to manually advance xPC
 * later.
 */
.macro PREFETCH_INST count
    ldrh    wINST, [xPC, #((\count)*2)]
.endm

/* Advance xPC by some number of code units. */
.macro ADVANCE count
  add  xPC, xPC, #((\count)*2)
.endm

/*
 * Fetch the next instruction from an offset specified by _reg and advance xPC.
 * xPC to point to the next instruction.  "_reg" must specify the distance
 * in bytes, *not* 16-bit code units, and may be a signed value.  Must not set flags.
 *
 */
.macro FETCH_ADVANCE_INST_RB reg
    add     xPC, xPC, \reg, sxtw
    ldrh    wINST, [xPC]
.endm

/*
 * Fetch a half-word code unit from an offset past the current PC.  The
 * "_count" value is in 16-bit code units.  Does not advance xPC.
 *
 * The "_S" variant works the same but treats the value as signed.
 */
.macro FETCH reg, count
    ldrh    \reg, [xPC, #((\count)*2)]
.endm

.macro FETCH_S reg, count
    ldrsh   \reg, [xPC, #((\count)*2)]
.endm

/*
 * Fetch one byte from an offset past the current PC.  Pass in the same
 * "_count" as you would for FETCH, and an additional 0/1 indicating which
 * byte of the halfword you want (lo/hi).
 */
.macro FETCH_B reg, count, byte
    ldrb     \reg, [xPC, #((\count)*2+(\byte))]
.endm

/*
 * Put the instruction's opcode field into the specified register.
 */
.macro GET_INST_OPCODE reg
    and     \reg, xINST, #255
.endm

/*
 * Put the prefetched instruction's opcode field into the specified register.
 */
.macro GET_PREFETCHED_OPCODE oreg, ireg
    and     \oreg, \ireg, #255
.endm

/*
 * Begin executing the opcode in _reg.  Clobbers reg
 */

.macro GOTO_OPCODE reg
    add     \reg, xIBASE, \reg, lsl #${handler_size_bits}
    br      \reg
.endm
.macro GOTO_OPCODE_BASE base,reg
    add     \reg, \base, \reg, lsl #${handler_size_bits}
    br      \reg
.endm

/*
 * Get/set the 32-bit value from a Dalvik register.
 */
.macro GET_VREG reg, vreg
    ldr     \reg, [xFP, \vreg, uxtw #2]
.endm
.macro SET_VREG reg, vreg
    str     \reg, [xFP, \vreg, uxtw #2]
    str     wzr, [xREFS, \vreg, uxtw #2]
.endm
.macro SET_VREG_OBJECT reg, vreg, tmpreg
    str     \reg, [xFP, \vreg, uxtw #2]
    str     \reg, [xREFS, \vreg, uxtw #2]
.endm
.macro SET_VREG_FLOAT reg, vreg
    str     \reg, [xFP, \vreg, uxtw #2]
    str     wzr, [xREFS, \vreg, uxtw #2]
.endm

/*
 * Get/set the 64-bit value from a Dalvik register.
 */
.macro GET_VREG_WIDE reg, vreg
    add     ip2, xFP, \vreg, uxtw #2
    ldr     \reg, [ip2]
.endm
.macro SET_VREG_WIDE reg, vreg
    add     ip2, xFP, \vreg, uxtw #2
    str     \reg, [ip2]
    add     ip2, xREFS, \vreg, uxtw #2
    str     xzr, [ip2]
.endm
.macro GET_VREG_DOUBLE reg, vreg
    add     ip2, xFP, \vreg, uxtw #2
    ldr     \reg, [ip2]
.endm
.macro SET_VREG_DOUBLE reg, vreg
    add     ip2, xFP, \vreg, uxtw #2
    str     \reg, [ip2]
    add     ip2, xREFS, \vreg, uxtw #2
    str     xzr, [ip2]
.endm

/*
 * Get the 32-bit value from a Dalvik register and sign-extend to 64-bit.
 * Used to avoid an extra instruction in int-to-long.
 */
.macro GET_VREG_S reg, vreg
    ldrsw   \reg, [xFP, \vreg, uxtw #2]
.endm

/*
 * Convert a virtual register index into an address.
 */
.macro VREG_INDEX_TO_ADDR reg, vreg
    add     \reg, xFP, \vreg, uxtw #2   /* WARNING: handle shadow frame vreg zero if store */
.endm

/*
 * Refresh handler table.
 */
.macro REFRESH_IBASE
  ldr     xIBASE, [xSELF, #THREAD_CURRENT_IBASE_OFFSET]
.endm

/*
 * Save two registers to the stack.
 */
.macro SAVE_TWO_REGS reg1, reg2, offset
    stp \reg1, \reg2, [sp, #(\offset)]
    .cfi_rel_offset \reg1, (\offset)
    .cfi_rel_offset \reg2, (\offset) + 8
.endm

/*
 * Restore two registers from the stack.
 */
.macro RESTORE_TWO_REGS reg1, reg2, offset
    ldp \reg1, \reg2, [sp, #(\offset)]
    .cfi_restore \reg1
    .cfi_restore \reg2
.endm

/*
 * Increase frame size and save two registers to the bottom of the stack.
 */
.macro SAVE_TWO_REGS_INCREASE_FRAME reg1, reg2, frame_adjustment
    stp \reg1, \reg2, [sp, #-(\frame_adjustment)]!
    .cfi_adjust_cfa_offset (\frame_adjustment)
    .cfi_rel_offset \reg1, 0
    .cfi_rel_offset \reg2, 8
.endm

/*
 * Restore two registers from the bottom of the stack and decrease frame size.
 */
.macro RESTORE_TWO_REGS_DECREASE_FRAME reg1, reg2, frame_adjustment
    ldp \reg1, \reg2, [sp], #(\frame_adjustment)
    .cfi_restore \reg1
    .cfi_restore \reg2
    .cfi_adjust_cfa_offset -(\frame_adjustment)
.endm

/*
 * function support macros.
 */
.macro ENTRY name
    .type \name, #function
    .hidden \name  // Hide this as a global symbol, so we do not incur plt calls.
    .global \name
    /* Cache alignment for function entry */
    .balign 16
\name:
.endm

.macro END name
    .size \name, .-\name
.endm

// Macro to unpoison (negate) the reference for heap poisoning.
.macro UNPOISON_HEAP_REF rRef
#ifdef USE_HEAP_POISONING
    neg \rRef, \rRef
#endif  // USE_HEAP_POISONING
.endm

%def entry():
/*
 * Copyright (C) 2016 The Android Open Source Project
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

    .text

/*
 * Interpreter entry point.
 * On entry:
 *  x0  Thread* self/
 *  x1  insns_
 *  x2  ShadowFrame
 *  x3  JValue* result_register
 *
 */
ENTRY ExecuteMterpImpl
    .cfi_startproc
    SAVE_TWO_REGS_INCREASE_FRAME xPROFILE, x27, 80
    SAVE_TWO_REGS                xIBASE, xREFS, 16
    SAVE_TWO_REGS                xSELF, xINST, 32
    SAVE_TWO_REGS                xPC, xFP, 48
    SAVE_TWO_REGS                fp, lr, 64
    add     fp, sp, #64

    /* Remember the return register */
    str     x3, [x2, #SHADOWFRAME_RESULT_REGISTER_OFFSET]

    /* Remember the dex instruction pointer */
    str     x1, [x2, #SHADOWFRAME_DEX_INSTRUCTIONS_OFFSET]

    /* set up "named" registers */
    mov     xSELF, x0
    ldr     w0, [x2, #SHADOWFRAME_NUMBER_OF_VREGS_OFFSET]
    add     xFP, x2, #SHADOWFRAME_VREGS_OFFSET     // point to vregs.
    add     xREFS, xFP, w0, uxtw #2                // point to reference array in shadow frame
    ldr     w0, [x2, #SHADOWFRAME_DEX_PC_OFFSET]   // Get starting dex_pc.
    add     xPC, x1, w0, uxtw #1                   // Create direct pointer to 1st dex opcode
    CFI_DEFINE_DEX_PC_WITH_OFFSET(CFI_TMP, CFI_DEX, 0)
    EXPORT_PC

    /* Starting ibase */
    ldr     xIBASE, [xSELF, #THREAD_CURRENT_IBASE_OFFSET]

    /* Set up for backwards branches & osr profiling */
    ldr     x0, [xFP, #OFF_FP_METHOD]
    add     x1, xFP, #OFF_FP_SHADOWFRAME
    mov     x2, xSELF
    bl      MterpSetUpHotnessCountdown
    mov     wPROFILE, w0                // Starting hotness countdown to xPROFILE

    /* start executing the instruction at rPC */
    FETCH_INST                          // load wINST from rPC
    GET_INST_OPCODE ip                  // extract opcode from wINST
    GOTO_OPCODE ip                      // jump to next instruction
    /* NOTE: no fallthrough */
    // cfi info continues, and covers the whole mterp implementation.
    END ExecuteMterpImpl

%def dchecks_before_helper():
    // Call C++ to do debug checks and return to the handler using tail call.
    .extern MterpCheckBefore
    mov    x0, xSELF
    add    x1, xFP, #OFF_FP_SHADOWFRAME
    mov    x2, xPC
    b      MterpCheckBefore     // (self, shadow_frame, dex_pc_ptr) Note: tail call.

%def opcode_pre():
%  add_helper(dchecks_before_helper, "mterp_dchecks_before_helper")
    #if !defined(NDEBUG)
    bl     mterp_dchecks_before_helper
    #endif

%def footer():
    .cfi_endproc
    END MterpHelpers

%def fallback():
/* Transfer stub to alternate interpreter */
    b    MterpFallback


%def helpers():
    ENTRY MterpHelpers
/*
 * ===========================================================================
 *  Common subroutines and data
 * ===========================================================================
 */

/*
 * We've detected a condition that will result in an exception, but the exception
 * has not yet been thrown.  Just bail out to the reference interpreter to deal with it.
 * TUNING: for consistency, we may want to just go ahead and handle these here.
 */
common_errDivideByZero:
    EXPORT_PC
#if MTERP_LOGGING
    mov  x0, xSELF
    add  x1, xFP, #OFF_FP_SHADOWFRAME
    bl MterpLogDivideByZeroException
#endif
    b MterpCommonFallback

common_errArrayIndex:
    EXPORT_PC
#if MTERP_LOGGING
    mov  x0, xSELF
    add  x1, xFP, #OFF_FP_SHADOWFRAME
    bl MterpLogArrayIndexException
#endif
    b MterpCommonFallback

common_errNegativeArraySize:
    EXPORT_PC
#if MTERP_LOGGING
    mov  x0, xSELF
    add  x1, xFP, #OFF_FP_SHADOWFRAME
    bl MterpLogNegativeArraySizeException
#endif
    b MterpCommonFallback

common_errNoSuchMethod:
    EXPORT_PC
#if MTERP_LOGGING
    mov  x0, xSELF
    add  x1, xFP, #OFF_FP_SHADOWFRAME
    bl MterpLogNoSuchMethodException
#endif
    b MterpCommonFallback

common_errNullObject:
    EXPORT_PC
#if MTERP_LOGGING
    mov  x0, xSELF
    add  x1, xFP, #OFF_FP_SHADOWFRAME
    bl MterpLogNullObjectException
#endif
    b MterpCommonFallback

common_exceptionThrown:
    EXPORT_PC
#if MTERP_LOGGING
    mov  x0, xSELF
    add  x1, xFP, #OFF_FP_SHADOWFRAME
    bl MterpLogExceptionThrownException
#endif
    b MterpCommonFallback

MterpSuspendFallback:
    EXPORT_PC
#if MTERP_LOGGING
    mov  x0, xSELF
    add  x1, xFP, #OFF_FP_SHADOWFRAME
    ldr  x2, [xSELF, #THREAD_FLAGS_OFFSET]
    bl MterpLogSuspendFallback
#endif
    b MterpCommonFallback

/*
 * If we're here, something is out of the ordinary.  If there is a pending
 * exception, handle it.  Otherwise, roll back and retry with the reference
 * interpreter.
 */
MterpPossibleException:
    ldr     x0, [xSELF, #THREAD_EXCEPTION_OFFSET]
    cbz     x0, MterpFallback                       // If not, fall back to reference interpreter.
    /* intentional fallthrough - handle pending exception. */
/*
 * On return from a runtime helper routine, we've found a pending exception.
 * Can we handle it here - or need to bail out to caller?
 *
 */
MterpException:
    mov     x0, xSELF
    add     x1, xFP, #OFF_FP_SHADOWFRAME
    bl      MterpHandleException                    // (self, shadow_frame)
    cbz     w0, MterpExceptionReturn                // no local catch, back to caller.
    ldr     x0, [xFP, #OFF_FP_DEX_INSTRUCTIONS]
    ldr     w1, [xFP, #OFF_FP_DEX_PC]
    ldr     xIBASE, [xSELF, #THREAD_CURRENT_IBASE_OFFSET]
    add     xPC, x0, x1, lsl #1                     // generate new dex_pc_ptr
    /* Do we need to switch interpreters? */
    ldr     w0, [xSELF, #THREAD_USE_MTERP_OFFSET]
    cbz     w0, MterpFallback
    /* resume execution at catch block */
    EXPORT_PC
    FETCH_INST
    GET_INST_OPCODE ip
    GOTO_OPCODE ip
    /* NOTE: no fallthrough */
/*
 * Common handling for branches with support for Jit profiling.
 * On entry:
 *    wINST          <= signed offset
 *    wPROFILE       <= signed hotness countdown (expanded to 32 bits)
 *    condition bits <= set to establish sign of offset (use "NoFlags" entry if not)
 *
 * We have quite a few different cases for branch profiling, OSR detection and
 * suspend check support here.
 *
 * Taken backward branches:
 *    If profiling active, do hotness countdown and report if we hit zero.
 *    If in osr check mode, see if our target is a compiled loop header entry and do OSR if so.
 *    Is there a pending suspend request?  If so, suspend.
 *
 * Taken forward branches and not-taken backward branches:
 *    If in osr check mode, see if our target is a compiled loop header entry and do OSR if so.
 *
 * Our most common case is expected to be a taken backward branch with active jit profiling,
 * but no full OSR check and no pending suspend request.
 * Next most common case is not-taken branch with no full OSR check.
 *
 */
MterpCommonTakenBranchNoFlags:
    cmp     wINST, #0
    b.gt    .L_forward_branch           // don't add forward branches to hotness
    tbnz    wPROFILE, #31, .L_no_count_backwards  // go if negative
    subs    wPROFILE, wPROFILE, #1      // countdown
    b.eq    .L_add_batch                // counted down to zero - report
.L_resume_backward_branch:
    ldr     lr, [xSELF, #THREAD_FLAGS_OFFSET]
    add     w2, wINST, wINST            // w2<- byte offset
    FETCH_ADVANCE_INST_RB w2            // update rPC, load wINST
    REFRESH_IBASE
    ands    lr, lr, #THREAD_SUSPEND_OR_CHECKPOINT_REQUEST
    b.ne    .L_suspend_request_pending
    GET_INST_OPCODE ip                  // extract opcode from wINST
    GOTO_OPCODE ip                      // jump to next instruction

.L_suspend_request_pending:
    EXPORT_PC
    mov     x0, xSELF
    bl      MterpSuspendCheck           // (self)
    cbnz    x0, MterpFallback
    REFRESH_IBASE                       // might have changed during suspend
    GET_INST_OPCODE ip                  // extract opcode from wINST
    GOTO_OPCODE ip                      // jump to next instruction

.L_no_count_backwards:
    cmp     wPROFILE, #JIT_CHECK_OSR    // possible OSR re-entry?
    b.ne    .L_resume_backward_branch
    mov     x0, xSELF
    add     x1, xFP, #OFF_FP_SHADOWFRAME
    mov     x2, xINST
    EXPORT_PC
    bl      MterpMaybeDoOnStackReplacement  // (self, shadow_frame, offset)
    cbnz    x0, MterpOnStackReplacement
    b       .L_resume_backward_branch

.L_forward_branch:
    cmp     wPROFILE, #JIT_CHECK_OSR    // possible OSR re-entry?
    b.eq    .L_check_osr_forward
.L_resume_forward_branch:
    add     w2, wINST, wINST            // w2<- byte offset
    FETCH_ADVANCE_INST_RB w2            // update rPC, load wINST
    GET_INST_OPCODE ip                  // extract opcode from wINST
    GOTO_OPCODE ip                      // jump to next instruction

.L_check_osr_forward:
    mov     x0, xSELF
    add     x1, xFP, #OFF_FP_SHADOWFRAME
    mov     x2, xINST
    EXPORT_PC
    bl      MterpMaybeDoOnStackReplacement  // (self, shadow_frame, offset)
    cbnz    x0, MterpOnStackReplacement
    b       .L_resume_forward_branch

.L_add_batch:
    add     x1, xFP, #OFF_FP_SHADOWFRAME
    strh    wPROFILE, [x1, #SHADOWFRAME_HOTNESS_COUNTDOWN_OFFSET]
    ldr     x0, [xFP, #OFF_FP_METHOD]
    mov     x2, xSELF
    bl      MterpAddHotnessBatch        // (method, shadow_frame, self)
    mov     wPROFILE, w0                // restore new hotness countdown to wPROFILE
    b       .L_no_count_backwards

/*
 * Entered from the conditional branch handlers when OSR check request active on
 * not-taken path.  All Dalvik not-taken conditional branch offsets are 2.
 */
.L_check_not_taken_osr:
    mov     x0, xSELF
    add     x1, xFP, #OFF_FP_SHADOWFRAME
    mov     x2, #2
    EXPORT_PC
    bl      MterpMaybeDoOnStackReplacement  // (self, shadow_frame, offset)
    cbnz    x0, MterpOnStackReplacement
    FETCH_ADVANCE_INST 2
    GET_INST_OPCODE ip                  // extract opcode from wINST
    GOTO_OPCODE ip                      // jump to next instruction

/*
 * Check for suspend check request.  Assumes wINST already loaded, xPC advanced and
 * still needs to get the opcode and branch to it, and flags are in lr.
 */
MterpCheckSuspendAndContinue:
    ldr     xIBASE, [xSELF, #THREAD_CURRENT_IBASE_OFFSET]  // refresh xIBASE
    ands    w7, w7, #THREAD_SUSPEND_OR_CHECKPOINT_REQUEST
    b.ne    check1
    GET_INST_OPCODE ip                  // extract opcode from wINST
    GOTO_OPCODE ip                      // jump to next instruction
check1:
    EXPORT_PC
    mov     x0, xSELF
    bl      MterpSuspendCheck           // (self)
    cbnz    x0, MterpFallback           // Something in the environment changed, switch interpreters
    GET_INST_OPCODE ip                  // extract opcode from wINST
    GOTO_OPCODE ip                      // jump to next instruction

/*
 * On-stack replacement has happened, and now we've returned from the compiled method.
 */
MterpOnStackReplacement:
#if MTERP_LOGGING
    mov  x0, xSELF
    add  x1, xFP, #OFF_FP_SHADOWFRAME
    sxtw x2, wINST
    bl MterpLogOSR
#endif
    mov  x0, #1                         // Signal normal return
    b    MterpDone

/*
 * Bail out to reference interpreter.
 */
MterpFallback:
    EXPORT_PC
#if MTERP_LOGGING
    mov  x0, xSELF
    add  x1, xFP, #OFF_FP_SHADOWFRAME
    bl MterpLogFallback
#endif
MterpCommonFallback:
    mov     x0, #0                                  // signal retry with reference interpreter.
    b       MterpDone

/*
 * We pushed some registers on the stack in ExecuteMterpImpl, then saved
 * SP and LR.  Here we restore SP, restore the registers, and then restore
 * LR to PC.
 *
 * On entry:
 *  uint32_t* xFP  (should still be live, pointer to base of vregs)
 */
MterpExceptionReturn:
    mov     x0, #1                                  // signal return to caller.
    b MterpDone
MterpReturn:
    ldr     x2, [xFP, #OFF_FP_RESULT_REGISTER]
    str     x0, [x2]
    mov     x0, #1                                  // signal return to caller.
MterpDone:
/*
 * At this point, we expect wPROFILE to be non-zero.  If negative, hotness is disabled or we're
 * checking for OSR.  If greater than zero, we might have unreported hotness to register
 * (the difference between the ending wPROFILE and the cached hotness counter).  wPROFILE
 * should only reach zero immediately after a hotness decrement, and is then reset to either
 * a negative special state or the new non-zero countdown value.
 */
    cmp     wPROFILE, #0
    bgt     MterpProfileActive                      // if > 0, we may have some counts to report.
    .cfi_remember_state
    RESTORE_TWO_REGS                fp, lr, 64
    RESTORE_TWO_REGS                xPC, xFP, 48
    RESTORE_TWO_REGS                xSELF, xINST, 32
    RESTORE_TWO_REGS                xIBASE, xREFS, 16
    RESTORE_TWO_REGS_DECREASE_FRAME xPROFILE, x27, 80
    ret
    .cfi_restore_state                              // Reset unwind info so following code unwinds.
    .cfi_def_cfa_offset 80                          // workaround for clang bug: 31975598

MterpProfileActive:
    mov     xINST, x0                               // stash return value
    /* Report cached hotness counts */
    ldr     x0, [xFP, #OFF_FP_METHOD]
    add     x1, xFP, #OFF_FP_SHADOWFRAME
    mov     x2, xSELF
    strh    wPROFILE, [x1, #SHADOWFRAME_HOTNESS_COUNTDOWN_OFFSET]
    bl      MterpAddHotnessBatch                    // (method, shadow_frame, self)
    mov     x0, xINST                               // restore return value
    RESTORE_TWO_REGS                fp, lr, 64
    RESTORE_TWO_REGS                xPC, xFP, 48
    RESTORE_TWO_REGS                xSELF, xINST, 32
    RESTORE_TWO_REGS                xIBASE, xREFS, 16
    RESTORE_TWO_REGS_DECREASE_FRAME xPROFILE, x27, 80
    ret


%def instruction_end():

    .type artMterpAsmInstructionEnd, #object
    .hidden artMterpAsmInstructionEnd
    .global artMterpAsmInstructionEnd
artMterpAsmInstructionEnd:

%def instruction_start():

    .type artMterpAsmInstructionStart, #object
    .hidden artMterpAsmInstructionStart
    .global artMterpAsmInstructionStart
artMterpAsmInstructionStart = .L_op_nop
    .text

%def opcode_start():
    ENTRY mterp_${opcode}
%def opcode_end():
    END mterp_${opcode}
%def helper_start(name):
    ENTRY ${name}
%def helper_end(name):
    END ${name}
