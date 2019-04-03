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

  Some (hopefully) temporary ugliness.  We'll treat rFP as pointing to the
  base of the vreg array within the shadow frame.  Access the other fields,
  dex_pc_, method_ and number_of_vregs_ via negative offsets.  For now, we'll continue
  the shadow frame mechanism of double-storing object references - via rFP &
  number_of_vregs_.

 */

/*
ARM EABI general notes:

r0-r3 hold first 4 args to a method; they are not preserved across method calls
r4-r8 are available for general use
r9 is given special treatment in some situations, but not for us
r10 (sl) seems to be generally available
r11 (fp) is used by gcc (unless -fomit-frame-pointer is set)
r12 (ip) is scratch -- not preserved across method calls
r13 (sp) should be managed carefully in case a signal arrives
r14 (lr) must be preserved
r15 (pc) can be tinkered with directly

r0 holds returns of <= 4 bytes
r0-r1 hold returns of 8 bytes, low word in r0

Callee must save/restore r4+ (except r12) if it modifies them.  If VFP
is present, registers s16-s31 (a/k/a d8-d15, a/k/a q4-q7) must be preserved,
s0-s15 (d0-d7, q0-a3) do not need to be.

Stack is "full descending".  Only the arguments that don't fit in the first 4
registers are placed on the stack.  "sp" points at the first stacked argument
(i.e. the 5th arg).

VFP: single-precision results in s0, double-precision results in d0.

In the EABI, "sp" must be 64-bit aligned on entry to a function, and any
64-bit quantities (long long, double) must be 64-bit aligned.
*/

/*
Mterp and ARM notes:

The following registers have fixed assignments:

  reg nick      purpose
  r4  rPC       interpreted program counter, used for fetching instructions
  r5  rFP       interpreted frame pointer, used for accessing locals and args
  r6  rSELF     self (Thread) pointer
  r7  rINST     first 16-bit code unit of current instruction
  r8  rIBASE    interpreted instruction base pointer, used for computed goto
  r10 rPROFILE  branch profiling countdown
  r11 rREFS     base of object references in shadow frame  (ideally, we'll get rid of this later).

Macros are provided for common operations.  Each macro MUST emit only
one instruction to make instruction-counting easier.  They MUST NOT alter
unspecified registers or condition codes.
*/

/*
 * This is a #include, not a %include, because we want the C pre-processor
 * to expand the macros into assembler assignment statements.
 */
#include "asm_support.h"
#include "interpreter/cfi_asm_support.h"

#define MTERP_PROFILE_BRANCHES 1
#define MTERP_LOGGING 0

/* During bringup, we'll use the shadow frame model instead of rFP */
/* single-purpose registers, given names for clarity */
#define rPC      r4
#define CFI_DEX  4  // DWARF register number of the register holding dex-pc (xPC).
#define CFI_TMP  0  // DWARF register number of the first argument register (r0).
#define rFP      r5
#define rSELF    r6
#define rINST    r7
#define rIBASE   r8
#define rPROFILE r10
#define rREFS    r11

/*
 * Instead of holding a pointer to the shadow frame, we keep rFP at the base of the vregs.  So,
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
    str  rPC, [rFP, #OFF_FP_DEX_PC_PTR]
.endm

.macro EXPORT_DEX_PC tmp
    ldr  \tmp, [rFP, #OFF_FP_DEX_INSTRUCTIONS]
    str  rPC, [rFP, #OFF_FP_DEX_PC_PTR]
    sub  \tmp, rPC, \tmp
    asr  \tmp, #1
    str  \tmp, [rFP, #OFF_FP_DEX_PC]
.endm

/*
 * Fetch the next instruction from rPC into rINST.  Does not advance rPC.
 */
.macro FETCH_INST
    ldrh    rINST, [rPC]
.endm

/*
 * Fetch the next instruction from the specified offset.  Advances rPC
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
    ldrh    rINST, [rPC, #((\count)*2)]!
.endm

/*
 * The operation performed here is similar to FETCH_ADVANCE_INST, except the
 * src and dest registers are parameterized (not hard-wired to rPC and rINST).
 */
.macro PREFETCH_ADVANCE_INST dreg, sreg, count
    ldrh    \dreg, [\sreg, #((\count)*2)]!
.endm

/*
 * Similar to FETCH_ADVANCE_INST, but does not update rPC.  Used to load
 * rINST ahead of possible exception point.  Be sure to manually advance rPC
 * later.
 */
.macro PREFETCH_INST count
    ldrh    rINST, [rPC, #((\count)*2)]
.endm

/* Advance rPC by some number of code units. */
.macro ADVANCE count
  add  rPC, #((\count)*2)
.endm

/*
 * Fetch the next instruction from an offset specified by _reg.  Updates
 * rPC to point to the next instruction.  "_reg" must specify the distance
 * in bytes, *not* 16-bit code units, and may be a signed value.
 *
 * We want to write "ldrh rINST, [rPC, _reg, lsl #1]!", but some of the
 * bits that hold the shift distance are used for the half/byte/sign flags.
 * In some cases we can pre-double _reg for free, so we require a byte offset
 * here.
 */
.macro FETCH_ADVANCE_INST_RB reg
    ldrh    rINST, [rPC, \reg]!
.endm

/*
 * Fetch a half-word code unit from an offset past the current PC.  The
 * "_count" value is in 16-bit code units.  Does not advance rPC.
 *
 * The "_S" variant works the same but treats the value as signed.
 */
.macro FETCH reg, count
    ldrh    \reg, [rPC, #((\count)*2)]
.endm

.macro FETCH_S reg, count
    ldrsh   \reg, [rPC, #((\count)*2)]
.endm

/*
 * Fetch one byte from an offset past the current PC.  Pass in the same
 * "_count" as you would for FETCH, and an additional 0/1 indicating which
 * byte of the halfword you want (lo/hi).
 */
.macro FETCH_B reg, count, byte
    ldrb     \reg, [rPC, #((\count)*2+(\byte))]
.endm

/*
 * Put the instruction's opcode field into the specified register.
 */
.macro GET_INST_OPCODE reg
    and     \reg, rINST, #255
.endm

/*
 * Put the prefetched instruction's opcode field into the specified register.
 */
.macro GET_PREFETCHED_OPCODE oreg, ireg
    and     \oreg, \ireg, #255
.endm

/*
 * Begin executing the opcode in _reg.  Because this only jumps within the
 * interpreter, we don't have to worry about pre-ARMv5 THUMB interwork.
 */
.macro GOTO_OPCODE reg
    add     pc, rIBASE, \reg, lsl #${handler_size_bits}
.endm
.macro GOTO_OPCODE_BASE base,reg
    add     pc, \base, \reg, lsl #${handler_size_bits}
.endm

/*
 * Get/set the 32-bit value from a Dalvik register.
 */
.macro GET_VREG reg, vreg
    ldr     \reg, [rFP, \vreg, lsl #2]
.endm
.macro SET_VREG reg, vreg
    str     \reg, [rFP, \vreg, lsl #2]
    mov     \reg, #0
    str     \reg, [rREFS, \vreg, lsl #2]
.endm
.macro SET_VREG_WIDE regLo, regHi, vreg
    add     ip, rFP, \vreg, lsl #2
    strd    \regLo, \regHi, [ip]
    mov     \regLo, #0
    mov     \regHi, #0
    add     ip, rREFS, \vreg, lsl #2
    strd    \regLo, \regHi, [ip]
.endm
.macro SET_VREG_OBJECT reg, vreg, tmpreg
    str     \reg, [rFP, \vreg, lsl #2]
    str     \reg, [rREFS, \vreg, lsl #2]
.endm
.macro SET_VREG_SHADOW reg, vreg
    str     \reg, [rREFS, \vreg, lsl #2]
.endm
.macro SET_VREG_FLOAT reg, vreg, tmpreg
    add     \tmpreg, rFP, \vreg, lsl #2
    fsts    \reg, [\tmpreg]
    mov     \tmpreg, #0
    str     \tmpreg, [rREFS, \vreg, lsl #2]
.endm

/*
 * Clear the corresponding shadow regs for a vreg pair
 */
.macro CLEAR_SHADOW_PAIR vreg, tmp1, tmp2
    mov     \tmp1, #0
    add     \tmp2, \vreg, #1
    SET_VREG_SHADOW \tmp1, \vreg
    SET_VREG_SHADOW \tmp1, \tmp2
.endm

/*
 * Convert a virtual register index into an address.
 */
.macro VREG_INDEX_TO_ADDR reg, vreg
    add     \reg, rFP, \vreg, lsl #2   /* WARNING/FIXME: handle shadow frame vreg zero if store */
.endm

.macro GET_VREG_WIDE_BY_ADDR reg0, reg1, addr
    ldmia \addr, {\reg0, \reg1}
.endm
.macro SET_VREG_WIDE_BY_ADDR reg0, reg1, addr
    stmia \addr, {\reg0, \reg1}
.endm
.macro GET_VREG_FLOAT_BY_ADDR reg, addr
    flds \reg, [\addr]
.endm
.macro SET_VREG_FLOAT_BY_ADDR reg, addr
    fsts \reg, [\addr]
.endm
.macro GET_VREG_DOUBLE_BY_ADDR reg, addr
    fldd \reg, [\addr]
.endm
.macro SET_VREG_DOUBLE_BY_ADDR reg, addr
    fstd \reg, [\addr]
.endm

/*
 * Refresh handler table.
 */
.macro REFRESH_IBASE
  ldr     rIBASE, [rSELF, #THREAD_CURRENT_IBASE_OFFSET]
.endm

/*
 * function support macros.
 */
.macro ENTRY name
    .arm
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
    rsb \rRef, \rRef, #0
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
/*
 * Interpreter entry point.
 */

    .text
    .align  2

/*
 * On entry:
 *  r0  Thread* self/
 *  r1  insns_
 *  r2  ShadowFrame
 *  r3  JValue* result_register
 *
 */

ENTRY ExecuteMterpImpl
    .cfi_startproc
    stmfd   sp!, {r3-r10,fp,lr}         @ save 10 regs, (r3 just to align 64)
    .cfi_adjust_cfa_offset 40
    .cfi_rel_offset r3, 0
    .cfi_rel_offset r4, 4
    .cfi_rel_offset r5, 8
    .cfi_rel_offset r6, 12
    .cfi_rel_offset r7, 16
    .cfi_rel_offset r8, 20
    .cfi_rel_offset r9, 24
    .cfi_rel_offset r10, 28
    .cfi_rel_offset fp, 32
    .cfi_rel_offset lr, 36

    /* Remember the return register */
    str     r3, [r2, #SHADOWFRAME_RESULT_REGISTER_OFFSET]

    /* Remember the dex instruction pointer */
    str     r1, [r2, #SHADOWFRAME_DEX_INSTRUCTIONS_OFFSET]

    /* set up "named" registers */
    mov     rSELF, r0
    ldr     r0, [r2, #SHADOWFRAME_NUMBER_OF_VREGS_OFFSET]
    add     rFP, r2, #SHADOWFRAME_VREGS_OFFSET     @ point to vregs.
    VREG_INDEX_TO_ADDR rREFS, r0                   @ point to reference array in shadow frame
    ldr     r0, [r2, #SHADOWFRAME_DEX_PC_OFFSET]   @ Get starting dex_pc.
    add     rPC, r1, r0, lsl #1                    @ Create direct pointer to 1st dex opcode
    CFI_DEFINE_DEX_PC_WITH_OFFSET(CFI_TMP, CFI_DEX, 0)
    EXPORT_PC

    /* Starting ibase */
    ldr     rIBASE, [rSELF, #THREAD_CURRENT_IBASE_OFFSET]

    /* Set up for backwards branches & osr profiling */
    ldr     r0, [rFP, #OFF_FP_METHOD]
    add     r1, rFP, #OFF_FP_SHADOWFRAME
    mov     r2, rSELF
    bl      MterpSetUpHotnessCountdown
    mov     rPROFILE, r0                @ Starting hotness countdown to rPROFILE

    /* start executing the instruction at rPC */
    FETCH_INST                          @ load rINST from rPC
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    GOTO_OPCODE ip                      @ jump to next instruction
    /* NOTE: no fallthrough */
    // cfi info continues, and covers the whole mterp implementation.
    END ExecuteMterpImpl

%def dchecks_before_helper():
    // Call C++ to do debug checks and return to the handler using tail call.
    .extern MterpCheckBefore
    mov    r0, rSELF
    add    r1, rFP, #OFF_FP_SHADOWFRAME
    mov    r2, rPC
    b      MterpCheckBefore     @ (self, shadow_frame, dex_pc_ptr)  @ Tail call.

%def opcode_pre():
%  add_helper(dchecks_before_helper, "mterp_dchecks_before_helper")
    #if !defined(NDEBUG)
    bl     mterp_dchecks_before_helper
    #endif

%def fallback():
/* Transfer stub to alternate interpreter */
    b    MterpFallback


%def helpers():
    ENTRY MterpHelpers

%def footer():
/*
 * ===========================================================================
 *  Common subroutines and data
 * ===========================================================================
 */

    .text
    .align  2

/*
 * We've detected a condition that will result in an exception, but the exception
 * has not yet been thrown.  Just bail out to the reference interpreter to deal with it.
 * TUNING: for consistency, we may want to just go ahead and handle these here.
 */
common_errDivideByZero:
    EXPORT_PC
#if MTERP_LOGGING
    mov  r0, rSELF
    add  r1, rFP, #OFF_FP_SHADOWFRAME
    bl MterpLogDivideByZeroException
#endif
    b MterpCommonFallback

common_errArrayIndex:
    EXPORT_PC
#if MTERP_LOGGING
    mov  r0, rSELF
    add  r1, rFP, #OFF_FP_SHADOWFRAME
    bl MterpLogArrayIndexException
#endif
    b MterpCommonFallback

common_errNegativeArraySize:
    EXPORT_PC
#if MTERP_LOGGING
    mov  r0, rSELF
    add  r1, rFP, #OFF_FP_SHADOWFRAME
    bl MterpLogNegativeArraySizeException
#endif
    b MterpCommonFallback

common_errNoSuchMethod:
    EXPORT_PC
#if MTERP_LOGGING
    mov  r0, rSELF
    add  r1, rFP, #OFF_FP_SHADOWFRAME
    bl MterpLogNoSuchMethodException
#endif
    b MterpCommonFallback

common_errNullObject:
    EXPORT_PC
#if MTERP_LOGGING
    mov  r0, rSELF
    add  r1, rFP, #OFF_FP_SHADOWFRAME
    bl MterpLogNullObjectException
#endif
    b MterpCommonFallback

common_exceptionThrown:
    EXPORT_PC
#if MTERP_LOGGING
    mov  r0, rSELF
    add  r1, rFP, #OFF_FP_SHADOWFRAME
    bl MterpLogExceptionThrownException
#endif
    b MterpCommonFallback

MterpSuspendFallback:
    EXPORT_PC
#if MTERP_LOGGING
    mov  r0, rSELF
    add  r1, rFP, #OFF_FP_SHADOWFRAME
    ldr  r2, [rSELF, #THREAD_FLAGS_OFFSET]
    bl MterpLogSuspendFallback
#endif
    b MterpCommonFallback

/*
 * If we're here, something is out of the ordinary.  If there is a pending
 * exception, handle it.  Otherwise, roll back and retry with the reference
 * interpreter.
 */
MterpPossibleException:
    ldr     r0, [rSELF, #THREAD_EXCEPTION_OFFSET]
    cmp     r0, #0                                  @ Exception pending?
    beq     MterpFallback                           @ If not, fall back to reference interpreter.
    /* intentional fallthrough - handle pending exception. */
/*
 * On return from a runtime helper routine, we've found a pending exception.
 * Can we handle it here - or need to bail out to caller?
 *
 */
MterpException:
    mov     r0, rSELF
    add     r1, rFP, #OFF_FP_SHADOWFRAME
    bl      MterpHandleException                    @ (self, shadow_frame)
    cmp     r0, #0
    beq     MterpExceptionReturn                    @ no local catch, back to caller.
    ldr     r0, [rFP, #OFF_FP_DEX_INSTRUCTIONS]
    ldr     r1, [rFP, #OFF_FP_DEX_PC]
    ldr     rIBASE, [rSELF, #THREAD_CURRENT_IBASE_OFFSET]
    add     rPC, r0, r1, lsl #1                     @ generate new dex_pc_ptr
    /* Do we need to switch interpreters? */
    ldr     r0, [rSELF, #THREAD_USE_MTERP_OFFSET]
    cmp     r0, #0
    beq     MterpFallback
    /* resume execution at catch block */
    EXPORT_PC
    FETCH_INST
    GET_INST_OPCODE ip
    GOTO_OPCODE ip
    /* NOTE: no fallthrough */

/*
 * Common handling for branches with support for Jit profiling.
 * On entry:
 *    rINST          <= signed offset
 *    rPROFILE       <= signed hotness countdown (expanded to 32 bits)
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
    cmp     rINST, #0
MterpCommonTakenBranch:
    bgt     .L_forward_branch           @ don't add forward branches to hotness
/*
 * We need to subtract 1 from positive values and we should not see 0 here,
 * so we may use the result of the comparison with -1.
 */
#if JIT_CHECK_OSR != -1
#  error "JIT_CHECK_OSR must be -1."
#endif
    cmp     rPROFILE, #JIT_CHECK_OSR
    beq     .L_osr_check
    subsgt  rPROFILE, #1
    beq     .L_add_batch                @ counted down to zero - report
.L_resume_backward_branch:
    ldr     lr, [rSELF, #THREAD_FLAGS_OFFSET]
    REFRESH_IBASE
    add     r2, rINST, rINST            @ r2<- byte offset
    FETCH_ADVANCE_INST_RB r2            @ update rPC, load rINST
    ands    lr, #THREAD_SUSPEND_OR_CHECKPOINT_REQUEST
    bne     .L_suspend_request_pending
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    GOTO_OPCODE ip                      @ jump to next instruction

.L_suspend_request_pending:
    EXPORT_PC
    mov     r0, rSELF
    bl      MterpSuspendCheck           @ (self)
    cmp     r0, #0
    bne     MterpFallback
    REFRESH_IBASE                       @ might have changed during suspend
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    GOTO_OPCODE ip                      @ jump to next instruction

.L_no_count_backwards:
    cmp     rPROFILE, #JIT_CHECK_OSR    @ possible OSR re-entry?
    bne     .L_resume_backward_branch
.L_osr_check:
    mov     r0, rSELF
    add     r1, rFP, #OFF_FP_SHADOWFRAME
    mov     r2, rINST
    EXPORT_PC
    bl      MterpMaybeDoOnStackReplacement  @ (self, shadow_frame, offset)
    cmp     r0, #0
    bne     MterpOnStackReplacement
    b       .L_resume_backward_branch

.L_forward_branch:
    cmp     rPROFILE, #JIT_CHECK_OSR @ possible OSR re-entry?
    beq     .L_check_osr_forward
.L_resume_forward_branch:
    add     r2, rINST, rINST            @ r2<- byte offset
    FETCH_ADVANCE_INST_RB r2            @ update rPC, load rINST
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    GOTO_OPCODE ip                      @ jump to next instruction

.L_check_osr_forward:
    mov     r0, rSELF
    add     r1, rFP, #OFF_FP_SHADOWFRAME
    mov     r2, rINST
    EXPORT_PC
    bl      MterpMaybeDoOnStackReplacement  @ (self, shadow_frame, offset)
    cmp     r0, #0
    bne     MterpOnStackReplacement
    b       .L_resume_forward_branch

.L_add_batch:
    add     r1, rFP, #OFF_FP_SHADOWFRAME
    strh    rPROFILE, [r1, #SHADOWFRAME_HOTNESS_COUNTDOWN_OFFSET]
    ldr     r0, [rFP, #OFF_FP_METHOD]
    mov     r2, rSELF
    bl      MterpAddHotnessBatch        @ (method, shadow_frame, self)
    mov     rPROFILE, r0                @ restore new hotness countdown to rPROFILE
    b       .L_no_count_backwards

/*
 * Entered from the conditional branch handlers when OSR check request active on
 * not-taken path.  All Dalvik not-taken conditional branch offsets are 2.
 */
.L_check_not_taken_osr:
    mov     r0, rSELF
    add     r1, rFP, #OFF_FP_SHADOWFRAME
    mov     r2, #2
    EXPORT_PC
    bl      MterpMaybeDoOnStackReplacement  @ (self, shadow_frame, offset)
    cmp     r0, #0
    bne     MterpOnStackReplacement
    FETCH_ADVANCE_INST 2
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    GOTO_OPCODE ip                      @ jump to next instruction

/*
 * On-stack replacement has happened, and now we've returned from the compiled method.
 */
MterpOnStackReplacement:
#if MTERP_LOGGING
    mov r0, rSELF
    add r1, rFP, #OFF_FP_SHADOWFRAME
    mov r2, rINST
    bl MterpLogOSR
#endif
    mov r0, #1                          @ Signal normal return
    b MterpDone

/*
 * Bail out to reference interpreter.
 */
MterpFallback:
    EXPORT_PC
#if MTERP_LOGGING
    mov  r0, rSELF
    add  r1, rFP, #OFF_FP_SHADOWFRAME
    bl MterpLogFallback
#endif
MterpCommonFallback:
    mov     r0, #0                                  @ signal retry with reference interpreter.
    b       MterpDone

/*
 * We pushed some registers on the stack in ExecuteMterpImpl, then saved
 * SP and LR.  Here we restore SP, restore the registers, and then restore
 * LR to PC.
 *
 * On entry:
 *  uint32_t* rFP  (should still be live, pointer to base of vregs)
 */
MterpExceptionReturn:
    mov     r0, #1                                  @ signal return to caller.
    b MterpDone
MterpReturn:
    ldr     r2, [rFP, #OFF_FP_RESULT_REGISTER]
    str     r0, [r2]
    str     r1, [r2, #4]
    mov     r0, #1                                  @ signal return to caller.
MterpDone:
/*
 * At this point, we expect rPROFILE to be non-zero.  If negative, hotness is disabled or we're
 * checking for OSR.  If greater than zero, we might have unreported hotness to register
 * (the difference between the ending rPROFILE and the cached hotness counter).  rPROFILE
 * should only reach zero immediately after a hotness decrement, and is then reset to either
 * a negative special state or the new non-zero countdown value.
 */
    cmp     rPROFILE, #0
    bgt     MterpProfileActive                      @ if > 0, we may have some counts to report.
    ldmfd   sp!, {r3-r10,fp,pc}                     @ restore 10 regs and return

MterpProfileActive:
    mov     rINST, r0                               @ stash return value
    /* Report cached hotness counts */
    ldr     r0, [rFP, #OFF_FP_METHOD]
    add     r1, rFP, #OFF_FP_SHADOWFRAME
    mov     r2, rSELF
    strh    rPROFILE, [r1, #SHADOWFRAME_HOTNESS_COUNTDOWN_OFFSET]
    bl      MterpAddHotnessBatch                    @ (method, shadow_frame, self)
    mov     r0, rINST                               @ restore return value
    ldmfd   sp!, {r3-r10,fp,pc}                     @ restore 10 regs and return

    .cfi_endproc
    END MterpHelpers

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
