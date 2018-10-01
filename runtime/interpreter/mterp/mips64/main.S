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

#define zero $$0  /* always zero */
#define AT   $$at /* assembler temp */
#define v0   $$2  /* return value */
#define v1   $$3
#define a0   $$4  /* argument registers */
#define a1   $$5
#define a2   $$6
#define a3   $$7
#define a4   $$8  /* expanded register arguments */
#define a5   $$9
#define a6   $$10
#define a7   $$11
#define ta0  $$8  /* alias */
#define ta1  $$9
#define ta2  $$10
#define ta3  $$11
#define t0   $$12 /* temp registers (not saved across subroutine calls) */
#define t1   $$13
#define t2   $$14
#define t3   $$15

#define s0   $$16 /* saved across subroutine calls (callee saved) */
#define s1   $$17
#define s2   $$18
#define s3   $$19
#define s4   $$20
#define s5   $$21
#define s6   $$22
#define s7   $$23
#define t8   $$24 /* two more temp registers */
#define t9   $$25
#define k0   $$26 /* kernel temporary */
#define k1   $$27
#define gp   $$28 /* global pointer */
#define sp   $$29 /* stack pointer */
#define s8   $$30 /* one more callee saved */
#define ra   $$31 /* return address */

#define f0   $$f0
#define f1   $$f1
#define f2   $$f2
#define f3   $$f3
#define f12  $$f12
#define f13  $$f13

/*
 * It looks like the GNU assembler currently does not support the blec and bgtc
 * idioms, which should translate into bgec and bltc respectively with swapped
 * left and right register operands.
 * TODO: remove these macros when the assembler is fixed.
 */
.macro blec lreg, rreg, target
    bgec    \rreg, \lreg, \target
.endm
.macro bgtc lreg, rreg, target
    bltc    \rreg, \lreg, \target
.endm

/*
Mterp and MIPS64 notes:

The following registers have fixed assignments:

  reg nick      purpose
  s0  rPC       interpreted program counter, used for fetching instructions
  s1  rFP       interpreted frame pointer, used for accessing locals and args
  s2  rSELF     self (Thread) pointer
  s3  rINST     first 16-bit code unit of current instruction
  s4  rIBASE    interpreted instruction base pointer, used for computed goto
  s5  rREFS     base of object references in shadow frame  (ideally, we'll get rid of this later).
  s6  rPROFILE  jit profile hotness countdown
*/

/* During bringup, we'll use the shadow frame model instead of rFP */
/* single-purpose registers, given names for clarity */
#define rPC      s0
#define CFI_DEX  16  // DWARF register number of the register holding dex-pc (s0).
#define CFI_TMP  4   // DWARF register number of the first argument register (a0).
#define rFP      s1
#define rSELF    s2
#define rINST    s3
#define rIBASE   s4
#define rREFS    s5
#define rPROFILE s6

/*
 * This is a #include, not a %include, because we want the C pre-processor
 * to expand the macros into assembler assignment statements.
 */
#include "asm_support.h"
#include "interpreter/cfi_asm_support.h"

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

#define MTERP_PROFILE_BRANCHES 1
#define MTERP_LOGGING 0

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
    sd      rPC, OFF_FP_DEX_PC_PTR(rFP)
.endm

/*
 * Refresh handler table.
 */
.macro REFRESH_IBASE
    ld      rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)
.endm

/*
 * Fetch the next instruction from rPC into rINST.  Does not advance rPC.
 */
.macro FETCH_INST
    lhu     rINST, 0(rPC)
.endm

/* Advance rPC by some number of code units. */
.macro ADVANCE count
    daddu   rPC, rPC, (\count) * 2
.endm

/*
 * Fetch the next instruction from an offset specified by _reg and advance xPC.
 * xPC to point to the next instruction.  "_reg" must specify the distance
 * in bytes, *not* 16-bit code units, and may be a signed value.  Must not set flags.
 *
 */
.macro FETCH_ADVANCE_INST_RB reg
    daddu   rPC, rPC, \reg
    FETCH_INST
.endm

/*
 * Fetch the next instruction from the specified offset.  Advances rPC
 * to point to the next instruction.
 *
 * This must come AFTER anything that can throw an exception, or the
 * exception catch may miss.  (This also implies that it must come after
 * EXPORT_PC.)
 */
.macro FETCH_ADVANCE_INST count
    ADVANCE \count
    FETCH_INST
.endm

/*
 * Similar to FETCH_ADVANCE_INST, but does not update rPC.  Used to load
 * rINST ahead of possible exception point.  Be sure to manually advance rPC
 * later.
 */
.macro PREFETCH_INST count
    lhu     rINST, ((\count) * 2)(rPC)
.endm

/*
 * Put the instruction's opcode field into the specified register.
 */
.macro GET_INST_OPCODE reg
    and     \reg, rINST, 255
.endm

/*
 * Begin executing the opcode in _reg.
 */
.macro GOTO_OPCODE reg
    .set noat
    sll     AT, \reg, 7
    daddu   AT, rIBASE, AT
    jic     AT, 0
    .set at
.endm

/*
 * Get/set the 32-bit value from a Dalvik register.
 * Note, GET_VREG does sign extension to 64 bits while
 * GET_VREG_U does zero extension to 64 bits.
 * One is useful for arithmetic while the other is
 * useful for storing the result value as 64-bit.
 */
.macro GET_VREG reg, vreg
    .set noat
    dlsa    AT, \vreg, rFP, 2
    lw      \reg, 0(AT)
    .set at
.endm
.macro GET_VREG_U reg, vreg
    .set noat
    dlsa    AT, \vreg, rFP, 2
    lwu     \reg, 0(AT)
    .set at
.endm
.macro GET_VREG_FLOAT reg, vreg
    .set noat
    dlsa    AT, \vreg, rFP, 2
    lwc1    \reg, 0(AT)
    .set at
.endm
.macro SET_VREG reg, vreg
    .set noat
    dlsa    AT, \vreg, rFP, 2
    sw      \reg, 0(AT)
    dlsa    AT, \vreg, rREFS, 2
    sw      zero, 0(AT)
    .set at
.endm
.macro SET_VREG_OBJECT reg, vreg
    .set noat
    dlsa    AT, \vreg, rFP, 2
    sw      \reg, 0(AT)
    dlsa    AT, \vreg, rREFS, 2
    sw      \reg, 0(AT)
    .set at
.endm
.macro SET_VREG_FLOAT reg, vreg
    .set noat
    dlsa    AT, \vreg, rFP, 2
    swc1    \reg, 0(AT)
    dlsa    AT, \vreg, rREFS, 2
    sw      zero, 0(AT)
    .set at
.endm

/*
 * Get/set the 64-bit value from a Dalvik register.
 * Avoid unaligned memory accesses.
 * Note, SET_VREG_WIDE clobbers the register containing the value being stored.
 * Note, SET_VREG_DOUBLE clobbers the register containing the Dalvik register number.
 */
.macro GET_VREG_WIDE reg, vreg
    .set noat
    dlsa    AT, \vreg, rFP, 2
    lw      \reg, 0(AT)
    lw      AT, 4(AT)
    dinsu   \reg, AT, 32, 32
    .set at
.endm
.macro GET_VREG_DOUBLE reg, vreg
    .set noat
    dlsa    AT, \vreg, rFP, 2
    lwc1    \reg, 0(AT)
    lw      AT, 4(AT)
    mthc1   AT, \reg
    .set at
.endm
.macro SET_VREG_WIDE reg, vreg
    .set noat
    dlsa    AT, \vreg, rFP, 2
    sw      \reg, 0(AT)
    drotr32 \reg, \reg, 0
    sw      \reg, 4(AT)
    dlsa    AT, \vreg, rREFS, 2
    sw      zero, 0(AT)
    sw      zero, 4(AT)
    .set at
.endm
.macro SET_VREG_DOUBLE reg, vreg
    .set noat
    dlsa    AT, \vreg, rREFS, 2
    sw      zero, 0(AT)
    sw      zero, 4(AT)
    dlsa    AT, \vreg, rFP, 2
    swc1    \reg, 0(AT)
    mfhc1   \vreg, \reg
    sw      \vreg, 4(AT)
    .set at
.endm

/*
 * On-stack offsets for spilling/unspilling callee-saved registers
 * and the frame size.
 */
#define STACK_OFFSET_RA 0
#define STACK_OFFSET_GP 8
#define STACK_OFFSET_S0 16
#define STACK_OFFSET_S1 24
#define STACK_OFFSET_S2 32
#define STACK_OFFSET_S3 40
#define STACK_OFFSET_S4 48
#define STACK_OFFSET_S5 56
#define STACK_OFFSET_S6 64
#define STACK_SIZE      80    /* needs 16 byte alignment */

/* Constants for float/double_to_int/long conversions */
#define INT_MIN             0x80000000
#define INT_MIN_AS_FLOAT    0xCF000000
#define INT_MIN_AS_DOUBLE   0xC1E0000000000000
#define LONG_MIN            0x8000000000000000
#define LONG_MIN_AS_FLOAT   0xDF000000
#define LONG_MIN_AS_DOUBLE  0xC3E0000000000000

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

    .set    reorder

    .text
    .global ExecuteMterpImpl
    .type   ExecuteMterpImpl, %function
    .balign 16
/*
 * On entry:
 *  a0  Thread* self
 *  a1  dex_instructions
 *  a2  ShadowFrame
 *  a3  JValue* result_register
 *
 */
ExecuteMterpImpl:
    .cfi_startproc
    .cpsetup t9, t8, ExecuteMterpImpl

    .cfi_def_cfa sp, 0
    daddu   sp, sp, -STACK_SIZE
    .cfi_adjust_cfa_offset STACK_SIZE

    sd      t8, STACK_OFFSET_GP(sp)
    .cfi_rel_offset 28, STACK_OFFSET_GP
    sd      ra, STACK_OFFSET_RA(sp)
    .cfi_rel_offset 31, STACK_OFFSET_RA

    sd      s0, STACK_OFFSET_S0(sp)
    .cfi_rel_offset 16, STACK_OFFSET_S0
    sd      s1, STACK_OFFSET_S1(sp)
    .cfi_rel_offset 17, STACK_OFFSET_S1
    sd      s2, STACK_OFFSET_S2(sp)
    .cfi_rel_offset 18, STACK_OFFSET_S2
    sd      s3, STACK_OFFSET_S3(sp)
    .cfi_rel_offset 19, STACK_OFFSET_S3
    sd      s4, STACK_OFFSET_S4(sp)
    .cfi_rel_offset 20, STACK_OFFSET_S4
    sd      s5, STACK_OFFSET_S5(sp)
    .cfi_rel_offset 21, STACK_OFFSET_S5
    sd      s6, STACK_OFFSET_S6(sp)
    .cfi_rel_offset 22, STACK_OFFSET_S6

    /* Remember the return register */
    sd      a3, SHADOWFRAME_RESULT_REGISTER_OFFSET(a2)

    /* Remember the dex instruction pointer */
    sd      a1, SHADOWFRAME_DEX_INSTRUCTIONS_OFFSET(a2)

    /* set up "named" registers */
    move    rSELF, a0
    daddu   rFP, a2, SHADOWFRAME_VREGS_OFFSET
    lw      v0, SHADOWFRAME_NUMBER_OF_VREGS_OFFSET(a2)
    dlsa    rREFS, v0, rFP, 2
    lw      v0, SHADOWFRAME_DEX_PC_OFFSET(a2)
    dlsa    rPC, v0, a1, 1
    CFI_DEFINE_DEX_PC_WITH_OFFSET(CFI_TMP, CFI_DEX, 0)
    EXPORT_PC

    /* Starting ibase */
    REFRESH_IBASE

    /* Set up for backwards branches & osr profiling */
    ld      a0, OFF_FP_METHOD(rFP)
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rSELF
    jal     MterpSetUpHotnessCountdown
    move    rPROFILE, v0                # Starting hotness countdown to rPROFILE

    /* start executing the instruction at rPC */
    FETCH_INST
    GET_INST_OPCODE v0
    GOTO_OPCODE v0

    /* NOTE: no fallthrough */

%def alt_stub():
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (${opnum} * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

%def fallback():
/* Transfer stub to alternate interpreter */
    b       MterpFallback

%def helpers():
%  pass

%def footer():
/*
 * We've detected a condition that will result in an exception, but the exception
 * has not yet been thrown.  Just bail out to the reference interpreter to deal with it.
 * TUNING: for consistency, we may want to just go ahead and handle these here.
 */

    .extern MterpLogDivideByZeroException
common_errDivideByZero:
    EXPORT_PC
#if MTERP_LOGGING
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    jal     MterpLogDivideByZeroException
#endif
    b       MterpCommonFallback

    .extern MterpLogArrayIndexException
common_errArrayIndex:
    EXPORT_PC
#if MTERP_LOGGING
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    jal     MterpLogArrayIndexException
#endif
    b       MterpCommonFallback

    .extern MterpLogNullObjectException
common_errNullObject:
    EXPORT_PC
#if MTERP_LOGGING
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    jal     MterpLogNullObjectException
#endif
    b       MterpCommonFallback

/*
 * If we're here, something is out of the ordinary.  If there is a pending
 * exception, handle it.  Otherwise, roll back and retry with the reference
 * interpreter.
 */
MterpPossibleException:
    ld      a0, THREAD_EXCEPTION_OFFSET(rSELF)
    beqzc   a0, MterpFallback                       # If not, fall back to reference interpreter.
    /* intentional fallthrough - handle pending exception. */
/*
 * On return from a runtime helper routine, we've found a pending exception.
 * Can we handle it here - or need to bail out to caller?
 *
 */
    .extern MterpHandleException
    .extern MterpShouldSwitchInterpreters
MterpException:
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    jal     MterpHandleException                    # (self, shadow_frame)
    beqzc   v0, MterpExceptionReturn                # no local catch, back to caller.
    ld      a0, OFF_FP_DEX_INSTRUCTIONS(rFP)
    lwu     a1, OFF_FP_DEX_PC(rFP)
    REFRESH_IBASE
    dlsa    rPC, a1, a0, 1                          # generate new dex_pc_ptr
    /* Do we need to switch interpreters? */
    jal     MterpShouldSwitchInterpreters
    bnezc   v0, MterpFallback
    /* resume execution at catch block */
    EXPORT_PC
    FETCH_INST
    GET_INST_OPCODE v0
    GOTO_OPCODE v0
    /* NOTE: no fallthrough */

/*
 * Common handling for branches with support for Jit profiling.
 * On entry:
 *    rINST          <= signed offset
 *    rPROFILE       <= signed hotness countdown (expanded to 64 bits)
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
    bgtzc   rINST, .L_forward_branch    # don't add forward branches to hotness
/*
 * We need to subtract 1 from positive values and we should not see 0 here,
 * so we may use the result of the comparison with -1.
 */
    li      v0, JIT_CHECK_OSR
    beqc    rPROFILE, v0, .L_osr_check
    bltc    rPROFILE, v0, .L_resume_backward_branch
    dsubu   rPROFILE, 1
    beqzc   rPROFILE, .L_add_batch      # counted down to zero - report
.L_resume_backward_branch:
    lw      ra, THREAD_FLAGS_OFFSET(rSELF)
    REFRESH_IBASE
    daddu   a2, rINST, rINST            # a2<- byte offset
    FETCH_ADVANCE_INST_RB a2            # update rPC, load rINST
    and     ra, THREAD_SUSPEND_OR_CHECKPOINT_REQUEST
    bnezc   ra, .L_suspend_request_pending
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction

.L_suspend_request_pending:
    EXPORT_PC
    move    a0, rSELF
    jal     MterpSuspendCheck           # (self)
    bnezc   v0, MterpFallback
    REFRESH_IBASE                       # might have changed during suspend
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction

.L_no_count_backwards:
    li      v0, JIT_CHECK_OSR           # check for possible OSR re-entry
    bnec    rPROFILE, v0, .L_resume_backward_branch
.L_osr_check:
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rINST
    EXPORT_PC
    jal MterpMaybeDoOnStackReplacement  # (self, shadow_frame, offset)
    bnezc   v0, MterpOnStackReplacement
    b       .L_resume_backward_branch

.L_forward_branch:
    li      v0, JIT_CHECK_OSR           # check for possible OSR re-entry
    beqc    rPROFILE, v0, .L_check_osr_forward
.L_resume_forward_branch:
    daddu   a2, rINST, rINST            # a2<- byte offset
    FETCH_ADVANCE_INST_RB a2            # update rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction

.L_check_osr_forward:
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rINST
    EXPORT_PC
    jal     MterpMaybeDoOnStackReplacement # (self, shadow_frame, offset)
    bnezc   v0, MterpOnStackReplacement
    b       .L_resume_forward_branch

.L_add_batch:
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    sh      rPROFILE, SHADOWFRAME_HOTNESS_COUNTDOWN_OFFSET(a1)
    ld      a0, OFF_FP_METHOD(rFP)
    move    a2, rSELF
    jal     MterpAddHotnessBatch        # (method, shadow_frame, self)
    move    rPROFILE, v0                # restore new hotness countdown to rPROFILE
    b       .L_no_count_backwards

/*
 * Entered from the conditional branch handlers when OSR check request active on
 * not-taken path.  All Dalvik not-taken conditional branch offsets are 2.
 */
.L_check_not_taken_osr:
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    li      a2, 2
    EXPORT_PC
    jal     MterpMaybeDoOnStackReplacement # (self, shadow_frame, offset)
    bnezc   v0, MterpOnStackReplacement
    FETCH_ADVANCE_INST 2
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction

/*
 * On-stack replacement has happened, and now we've returned from the compiled method.
 */
MterpOnStackReplacement:
#if MTERP_LOGGING
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rINST                               # rINST contains offset
    jal     MterpLogOSR
#endif
    li      v0, 1                                   # Signal normal return
    b       MterpDone

/*
 * Bail out to reference interpreter.
 */
    .extern MterpLogFallback
MterpFallback:
    EXPORT_PC
#if MTERP_LOGGING
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    jal     MterpLogFallback
#endif
MterpCommonFallback:
    li      v0, 0                                   # signal retry with reference interpreter.
    b       MterpDone

/*
 * We pushed some registers on the stack in ExecuteMterpImpl, then saved
 * SP and RA.  Here we restore SP, restore the registers, and then restore
 * RA to PC.
 *
 * On entry:
 *  uint32_t* rFP  (should still be live, pointer to base of vregs)
 */
MterpExceptionReturn:
    li      v0, 1                                   # signal return to caller.
    b       MterpDone
/*
 * Returned value is expected in a0 and if it's not 64-bit, the 32 most
 * significant bits of a0 must be zero-extended or sign-extended
 * depending on the return type.
 */
MterpReturn:
    ld      a2, OFF_FP_RESULT_REGISTER(rFP)
    sd      a0, 0(a2)
    li      v0, 1                                   # signal return to caller.
MterpDone:
/*
 * At this point, we expect rPROFILE to be non-zero.  If negative, hotness is disabled or we're
 * checking for OSR.  If greater than zero, we might have unreported hotness to register
 * (the difference between the ending rPROFILE and the cached hotness counter).  rPROFILE
 * should only reach zero immediately after a hotness decrement, and is then reset to either
 * a negative special state or the new non-zero countdown value.
 */
    blez    rPROFILE, .L_pop_and_return # if > 0, we may have some counts to report.

MterpProfileActive:
    move    rINST, v0                   # stash return value
    /* Report cached hotness counts */
    ld      a0, OFF_FP_METHOD(rFP)
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rSELF
    sh      rPROFILE, SHADOWFRAME_HOTNESS_COUNTDOWN_OFFSET(a1)
    jal     MterpAddHotnessBatch        # (method, shadow_frame, self)
    move    v0, rINST                   # restore return value

.L_pop_and_return:
    ld      s6, STACK_OFFSET_S6(sp)
    .cfi_restore 22
    ld      s5, STACK_OFFSET_S5(sp)
    .cfi_restore 21
    ld      s4, STACK_OFFSET_S4(sp)
    .cfi_restore 20
    ld      s3, STACK_OFFSET_S3(sp)
    .cfi_restore 19
    ld      s2, STACK_OFFSET_S2(sp)
    .cfi_restore 18
    ld      s1, STACK_OFFSET_S1(sp)
    .cfi_restore 17
    ld      s0, STACK_OFFSET_S0(sp)
    .cfi_restore 16

    ld      ra, STACK_OFFSET_RA(sp)
    .cfi_restore 31

    ld      t8, STACK_OFFSET_GP(sp)
    .cpreturn
    .cfi_restore 28

    .set    noreorder
    jr      ra
    daddu   sp, sp, STACK_SIZE
    .cfi_adjust_cfa_offset -STACK_SIZE

    .cfi_endproc
    .set    reorder
    .size ExecuteMterpImpl, .-ExecuteMterpImpl

%def instruction_end():

    .global artMterpAsmInstructionEnd
artMterpAsmInstructionEnd:

%def instruction_end_alt():

    .global artMterpAsmAltInstructionEnd
artMterpAsmAltInstructionEnd:

%def instruction_start():

    .global artMterpAsmInstructionStart
artMterpAsmInstructionStart = .L_op_nop
    .text

%def instruction_start_alt():

    .global artMterpAsmAltInstructionStart
artMterpAsmAltInstructionStart = .L_ALT_op_nop
    .text

%def opcode_start():
%  pass
%def opcode_end():
%  pass
