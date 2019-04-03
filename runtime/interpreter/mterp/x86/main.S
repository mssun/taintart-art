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
x86 ABI general notes:

Caller save set:
   eax, edx, ecx, st(0)-st(7)
Callee save set:
   ebx, esi, edi, ebp
Return regs:
   32-bit in eax
   64-bit in edx:eax (low-order 32 in eax)
   fp on top of fp stack st(0)

Parameters passed on stack, pushed right-to-left.  On entry to target, first
parm is at 4(%esp).  Traditional entry code is:

functEntry:
    push    %ebp             # save old frame pointer
    mov     %ebp,%esp        # establish new frame pointer
    sub     FrameSize,%esp   # Allocate storage for spill, locals & outs

Once past the prologue, arguments are referenced at ((argno + 2)*4)(%ebp)

Stack must be 16-byte aligned to support SSE in native code.

If we're not doing variable stack allocation (alloca), the frame pointer can be
eliminated and all arg references adjusted to be esp relative.
*/

/*
Mterp and x86 notes:

Some key interpreter variables will be assigned to registers.

  nick     reg   purpose
  rPC      esi   interpreted program counter, used for fetching instructions
  rFP      edi   interpreted frame pointer, used for accessing locals and args
  rINSTw   bx    first 16-bit code of current instruction
  rINSTbl  bl    opcode portion of instruction word
  rINSTbh  bh    high byte of inst word, usually contains src/tgt reg names
  rIBASE   edx   base of instruction handler table
  rREFS    ebp   base of object references in shadow frame.

Notes:
   o High order 16 bits of ebx must be zero on entry to handler
   o rPC, rFP, rINSTw/rINSTbl valid on handler entry and exit
   o eax and ecx are scratch, rINSTw/ebx sometimes scratch

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

#define LITERAL(value) $$(value)

/*
 * Handle mac compiler specific
 */
#if defined(__APPLE__)
    #define MACRO_LITERAL(value) $$(value)
    #define FUNCTION_TYPE(name)
    #define OBJECT_TYPE(name)
    #define SIZE(start,end)
    // Mac OS' symbols have an _ prefix.
    #define SYMBOL(name) _ ## name
    #define ASM_HIDDEN .private_extern
#else
    #define MACRO_LITERAL(value) $$value
    #define FUNCTION_TYPE(name) .type name, @function
    #define OBJECT_TYPE(name) .type name, @object
    #define SIZE(start,end) .size start, .-end
    #define SYMBOL(name) name
    #define ASM_HIDDEN .hidden
#endif

.macro PUSH _reg
    pushl \_reg
    .cfi_adjust_cfa_offset 4
    .cfi_rel_offset \_reg, 0
.endm

.macro POP _reg
    popl \_reg
    .cfi_adjust_cfa_offset -4
    .cfi_restore \_reg
.endm

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
#define OFF_FP_COUNTDOWN_OFFSET OFF_FP(SHADOWFRAME_HOTNESS_COUNTDOWN_OFFSET)
#define OFF_FP_SHADOWFRAME OFF_FP(0)

/* Frame size must be 16-byte aligned.
 * Remember about 4 bytes for return address + 4 * 4 for spills
 */
#define FRAME_SIZE     28

/* Frame diagram while executing ExecuteMterpImpl, high to low addresses */
#define IN_ARG3        (FRAME_SIZE + 16 + 16)
#define IN_ARG2        (FRAME_SIZE + 16 + 12)
#define IN_ARG1        (FRAME_SIZE + 16 +  8)
#define IN_ARG0        (FRAME_SIZE + 16 +  4)
/* Spill offsets relative to %esp */
#define LOCAL0         (FRAME_SIZE -  4)
#define LOCAL1         (FRAME_SIZE -  8)
#define LOCAL2         (FRAME_SIZE - 12)
/* Out Arg offsets, relative to %esp */
#define OUT_ARG3       ( 12)
#define OUT_ARG2       (  8)
#define OUT_ARG1       (  4)
#define OUT_ARG0       (  0)  /* <- ExecuteMterpImpl esp + 0 */

/* During bringup, we'll use the shadow frame model instead of rFP */
/* single-purpose registers, given names for clarity */
#define rSELF    IN_ARG0(%esp)
#define rPC      %esi
#define CFI_DEX  6  // DWARF register number of the register holding dex-pc (esi).
#define CFI_TMP  0  // DWARF register number of the first argument register (eax).
#define rFP      %edi
#define rINST    %ebx
#define rINSTw   %bx
#define rINSTbh  %bh
#define rINSTbl  %bl
#define rIBASE   %edx
#define rREFS    %ebp
#define rPROFILE OFF_FP_COUNTDOWN_OFFSET(rFP)

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
    movl    rPC, OFF_FP_DEX_PC_PTR(rFP)
.endm

/*
 * Refresh handler table.
 */
.macro REFRESH_IBASE
    movl    rSELF, rIBASE
    movl    THREAD_CURRENT_IBASE_OFFSET(rIBASE), rIBASE
.endm

/*
 * Refresh handler table.
 * IBase handles uses the caller save register so we must restore it after each call.
 * Also it is used as a result of some 64-bit operations (like imul) and we should
 * restore it in such cases also.
 *
 * TODO: Consider spilling the IBase instead of restoring it from Thread structure.
 */
.macro RESTORE_IBASE
    movl    rSELF, rIBASE
    movl    THREAD_CURRENT_IBASE_OFFSET(rIBASE), rIBASE
.endm

/*
 * If rSELF is already loaded then we can use it from known reg.
 */
.macro RESTORE_IBASE_FROM_SELF _reg
    movl    THREAD_CURRENT_IBASE_OFFSET(\_reg), rIBASE
.endm

/*
 * Refresh rINST.
 * At enter to handler rINST does not contain the opcode number.
 * However some utilities require the full value, so this macro
 * restores the opcode number.
 */
.macro REFRESH_INST _opnum
    movb    rINSTbl, rINSTbh
    movb    MACRO_LITERAL(\_opnum), rINSTbl
.endm

/*
 * Fetch the next instruction from rPC into rINSTw.  Does not advance rPC.
 */
.macro FETCH_INST
    movzwl  (rPC), rINST
.endm

/*
 * Remove opcode from rINST, compute the address of handler and jump to it.
 */
.macro GOTO_NEXT
    movzx   rINSTbl,%eax
    movzbl  rINSTbh,rINST
    shll    MACRO_LITERAL(${handler_size_bits}), %eax
    addl    rIBASE, %eax
    jmp     *%eax
.endm

/*
 * Advance rPC by instruction count.
 */
.macro ADVANCE_PC _count
    leal    2*\_count(rPC), rPC
.endm

/*
 * Advance rPC by instruction count, fetch instruction and jump to handler.
 */
.macro ADVANCE_PC_FETCH_AND_GOTO_NEXT _count
    ADVANCE_PC \_count
    FETCH_INST
    GOTO_NEXT
.endm

/*
 * Get/set the 32-bit value from a Dalvik register.
 */
#define VREG_ADDRESS(_vreg) (rFP,_vreg,4)
#define VREG_HIGH_ADDRESS(_vreg) 4(rFP,_vreg,4)
#define VREG_REF_ADDRESS(_vreg) (rREFS,_vreg,4)
#define VREG_REF_HIGH_ADDRESS(_vreg) 4(rREFS,_vreg,4)

.macro GET_VREG _reg _vreg
    movl    VREG_ADDRESS(\_vreg), \_reg
.endm

/* Read wide value to xmm. */
.macro GET_WIDE_FP_VREG _reg _vreg
    movq    VREG_ADDRESS(\_vreg), \_reg
.endm

.macro SET_VREG _reg _vreg
    movl    \_reg, VREG_ADDRESS(\_vreg)
    movl    MACRO_LITERAL(0), VREG_REF_ADDRESS(\_vreg)
.endm

/* Write wide value from xmm. xmm is clobbered. */
.macro SET_WIDE_FP_VREG _reg _vreg
    movq    \_reg, VREG_ADDRESS(\_vreg)
    pxor    \_reg, \_reg
    movq    \_reg, VREG_REF_ADDRESS(\_vreg)
.endm

.macro SET_VREG_OBJECT _reg _vreg
    movl    \_reg, VREG_ADDRESS(\_vreg)
    movl    \_reg, VREG_REF_ADDRESS(\_vreg)
.endm

.macro GET_VREG_HIGH _reg _vreg
    movl    VREG_HIGH_ADDRESS(\_vreg), \_reg
.endm

.macro SET_VREG_HIGH _reg _vreg
    movl    \_reg, VREG_HIGH_ADDRESS(\_vreg)
    movl    MACRO_LITERAL(0), VREG_REF_HIGH_ADDRESS(\_vreg)
.endm

.macro CLEAR_REF _vreg
    movl    MACRO_LITERAL(0), VREG_REF_ADDRESS(\_vreg)
.endm

.macro CLEAR_WIDE_REF _vreg
    movl    MACRO_LITERAL(0), VREG_REF_ADDRESS(\_vreg)
    movl    MACRO_LITERAL(0), VREG_REF_HIGH_ADDRESS(\_vreg)
.endm

.macro GET_VREG_XMMs _xmmreg _vreg
    movss VREG_ADDRESS(\_vreg), \_xmmreg
.endm
.macro GET_VREG_XMMd _xmmreg _vreg
    movsd VREG_ADDRESS(\_vreg), \_xmmreg
.endm
.macro SET_VREG_XMMs _xmmreg _vreg
    movss \_xmmreg, VREG_ADDRESS(\_vreg)
.endm
.macro SET_VREG_XMMd _xmmreg _vreg
    movsd \_xmmreg, VREG_ADDRESS(\_vreg)
.endm

/*
 * function support macros.
 */
.macro ENTRY name
    .text
    ASM_HIDDEN SYMBOL(\name)
    .global SYMBOL(\name)
    FUNCTION_TYPE(\name)
SYMBOL(\name):
.endm

.macro END name
    SIZE(\name,\name)
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
 *
 * On entry:
 *  0  Thread* self
 *  1  insns_
 *  2  ShadowFrame
 *  3  JValue* result_register
 *
 */
ENTRY ExecuteMterpImpl
    .cfi_startproc
    .cfi_def_cfa esp, 4

    /* Spill callee save regs */
    PUSH    %ebp
    PUSH    %edi
    PUSH    %esi
    PUSH    %ebx

    /* Allocate frame */
    subl    $$FRAME_SIZE, %esp
    .cfi_adjust_cfa_offset FRAME_SIZE

    /* Load ShadowFrame pointer */
    movl    IN_ARG2(%esp), %edx

    /* Remember the return register */
    movl    IN_ARG3(%esp), %eax
    movl    %eax, SHADOWFRAME_RESULT_REGISTER_OFFSET(%edx)

    /* Remember the code_item */
    movl    IN_ARG1(%esp), %ecx
    movl    %ecx, SHADOWFRAME_DEX_INSTRUCTIONS_OFFSET(%edx)

    /* set up "named" registers */
    movl    SHADOWFRAME_NUMBER_OF_VREGS_OFFSET(%edx), %eax
    leal    SHADOWFRAME_VREGS_OFFSET(%edx), rFP
    leal    (rFP, %eax, 4), rREFS
    movl    SHADOWFRAME_DEX_PC_OFFSET(%edx), %eax
    lea     (%ecx, %eax, 2), rPC
    CFI_DEFINE_DEX_PC_WITH_OFFSET(CFI_TMP, CFI_DEX, 0)
    EXPORT_PC

    /* Set up for backwards branches & osr profiling */
    movl    OFF_FP_METHOD(rFP), %eax
    movl    %eax, OUT_ARG0(%esp)
    leal    OFF_FP_SHADOWFRAME(rFP), %ecx
    movl    %ecx, OUT_ARG1(%esp)
    movl    rSELF, %eax
    movl    %eax, OUT_ARG2(%esp)
    call    SYMBOL(MterpSetUpHotnessCountdown)

    /* Starting ibase */
    REFRESH_IBASE

    /* start executing the instruction at rPC */
    FETCH_INST
    GOTO_NEXT
    /* NOTE: no fallthrough */
    // cfi info continues, and covers the whole mterp implementation.
    END ExecuteMterpImpl

%def dchecks_before_helper():
    // Call C++ to do debug checks and return to the handler using tail call.
    .extern MterpCheckBefore
    popl    %eax                     # Return address (the instuction handler).
    movl    rSELF, %ecx
    movl    %ecx, OUT_ARG0(%esp)
    leal    OFF_FP_SHADOWFRAME(rFP), %ecx
    movl    %ecx, OUT_ARG1(%esp)
    movl    rPC, OUT_ARG2(%esp)
    pushl   %eax                     # Return address for the tail call.
    jmp     SYMBOL(MterpCheckBefore) # (self, shadow_frame, dex_pc_ptr)

%def opcode_pre():
%  add_helper(dchecks_before_helper, "mterp_dchecks_before_helper")
    #if !defined(NDEBUG)
    call    SYMBOL(mterp_dchecks_before_helper)
    REFRESH_IBASE
    #endif

%def fallback():
/* Transfer stub to alternate interpreter */
    jmp     MterpFallback


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
    movl    rSELF, %eax
    movl    %eax, OUT_ARG0(%esp)
    lea     OFF_FP_SHADOWFRAME(rFP), %ecx
    movl    %ecx, OUT_ARG1(%esp)
    call    SYMBOL(MterpLogDivideByZeroException)
#endif
    jmp     MterpCommonFallback

common_errArrayIndex:
    EXPORT_PC
#if MTERP_LOGGING
    movl    rSELF, %eax
    movl    %eax, OUT_ARG0(%esp)
    lea     OFF_FP_SHADOWFRAME(rFP), %ecx
    movl    %ecx, OUT_ARG1(%esp)
    call    SYMBOL(MterpLogArrayIndexException)
#endif
    jmp     MterpCommonFallback

common_errNegativeArraySize:
    EXPORT_PC
#if MTERP_LOGGING
    movl    rSELF, %eax
    movl    %eax, OUT_ARG0(%esp)
    lea     OFF_FP_SHADOWFRAME(rFP), %ecx
    movl    %ecx, OUT_ARG1(%esp)
    call    SYMBOL(MterpLogNegativeArraySizeException)
#endif
    jmp     MterpCommonFallback

common_errNoSuchMethod:
    EXPORT_PC
#if MTERP_LOGGING
    movl    rSELF, %eax
    movl    %eax, OUT_ARG0(%esp)
    lea     OFF_FP_SHADOWFRAME(rFP), %ecx
    movl    %ecx, OUT_ARG1(%esp)
    call    SYMBOL(MterpLogNoSuchMethodException)
#endif
    jmp     MterpCommonFallback

common_errNullObject:
    EXPORT_PC
#if MTERP_LOGGING
    movl    rSELF, %eax
    movl    %eax, OUT_ARG0(%esp)
    lea     OFF_FP_SHADOWFRAME(rFP), %ecx
    movl    %ecx, OUT_ARG1(%esp)
    call    SYMBOL(MterpLogNullObjectException)
#endif
    jmp     MterpCommonFallback

common_exceptionThrown:
    EXPORT_PC
#if MTERP_LOGGING
    movl    rSELF, %eax
    movl    %eax, OUT_ARG0(%esp)
    lea     OFF_FP_SHADOWFRAME(rFP), %ecx
    movl    %ecx, OUT_ARG0(%esp)
    call    SYMBOL(MterpLogExceptionThrownException)
#endif
    jmp     MterpCommonFallback

MterpSuspendFallback:
    EXPORT_PC
#if MTERP_LOGGING
    movl    rSELF, %eax
    movl    %eax, OUT_ARG0(%esp)
    lea     OFF_FP_SHADOWFRAME(rFP), %ecx
    movl    %ecx, OUT_ARG0(%esp)
    movl    THREAD_FLAGS_OFFSET(%eax), %eax
    movl    %eax, OUT_ARG2(%esp)
    call    SYMBOL(MterpLogSuspendFallback)
#endif
    jmp     MterpCommonFallback

/*
 * If we're here, something is out of the ordinary.  If there is a pending
 * exception, handle it.  Otherwise, roll back and retry with the reference
 * interpreter.
 */
MterpPossibleException:
    movl    rSELF, %eax
    testl   $$-1, THREAD_EXCEPTION_OFFSET(%eax)
    jz      MterpFallback
    /* intentional fallthrough - handle pending exception. */

/*
 * On return from a runtime helper routine, we've found a pending exception.
 * Can we handle it here - or need to bail out to caller?
 *
 */
MterpException:
    movl    rSELF, %eax
    movl    %eax, OUT_ARG0(%esp)
    lea     OFF_FP_SHADOWFRAME(rFP), %ecx
    movl    %ecx, OUT_ARG1(%esp)
    call    SYMBOL(MterpHandleException)
    testb   %al, %al
    jz      MterpExceptionReturn
    movl    OFF_FP_DEX_INSTRUCTIONS(rFP), %eax
    movl    OFF_FP_DEX_PC(rFP), %ecx
    lea     (%eax, %ecx, 2), rPC
    movl    rPC, OFF_FP_DEX_PC_PTR(rFP)
    /* Do we need to switch interpreters? */
    movl    rSELF, %eax
    cmpb    LITERAL(0), THREAD_USE_MTERP_OFFSET(%eax)
    jz      MterpFallback
    /* resume execution at catch block */
    REFRESH_IBASE
    FETCH_INST
    GOTO_NEXT
    /* NOTE: no fallthrough */

/*
 * Common handling for branches with support for Jit profiling.
 * On entry:
 *    rINST          <= signed offset
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
MterpCommonTakenBranch:
    jg      .L_forward_branch               # don't add forward branches to hotness
/*
 * We need to subtract 1 from positive values and we should not see 0 here,
 * so we may use the result of the comparison with -1.
 */
#if JIT_CHECK_OSR != -1
#  error "JIT_CHECK_OSR must be -1."
#endif
    cmpw    $$JIT_CHECK_OSR, rPROFILE
    je      .L_osr_check
    decw    rPROFILE
    je      .L_add_batch                    # counted down to zero - report
.L_resume_backward_branch:
    movl    rSELF, %eax
    testl   $$(THREAD_SUSPEND_OR_CHECKPOINT_REQUEST), THREAD_FLAGS_OFFSET(%eax)
    leal    (rPC, rINST, 2), rPC
    FETCH_INST
    jnz     .L_suspend_request_pending
    REFRESH_IBASE
    GOTO_NEXT

.L_suspend_request_pending:
    EXPORT_PC
    movl    %eax, OUT_ARG0(%esp)            # rSELF in eax
    call    SYMBOL(MterpSuspendCheck)       # (self)
    testb   %al, %al
    jnz     MterpFallback
    REFRESH_IBASE                           # might have changed during suspend
    GOTO_NEXT

.L_no_count_backwards:
    cmpw    $$JIT_CHECK_OSR, rPROFILE         # possible OSR re-entry?
    jne     .L_resume_backward_branch
.L_osr_check:
    EXPORT_PC
    movl    rSELF, %eax
    movl    %eax, OUT_ARG0(%esp)
    leal    OFF_FP_SHADOWFRAME(rFP), %ecx
    movl    %ecx, OUT_ARG1(%esp)
    movl    rINST, OUT_ARG2(%esp)
    call    SYMBOL(MterpMaybeDoOnStackReplacement) # (self, shadow_frame, offset)
    testb   %al, %al
    jz      .L_resume_backward_branch
    jmp     MterpOnStackReplacement

.L_forward_branch:
    cmpw    $$JIT_CHECK_OSR, rPROFILE         # possible OSR re-entry?
    je      .L_check_osr_forward
.L_resume_forward_branch:
    leal    (rPC, rINST, 2), rPC
    FETCH_INST
    GOTO_NEXT

.L_check_osr_forward:
    EXPORT_PC
    movl    rSELF, %eax
    movl    %eax, OUT_ARG0(%esp)
    leal    OFF_FP_SHADOWFRAME(rFP), %ecx
    movl    %ecx, OUT_ARG1(%esp)
    movl    rINST, OUT_ARG2(%esp)
    call    SYMBOL(MterpMaybeDoOnStackReplacement) # (self, shadow_frame, offset)
    testb   %al, %al
    REFRESH_IBASE
    jz      .L_resume_forward_branch
    jmp     MterpOnStackReplacement

.L_add_batch:
    movl    OFF_FP_METHOD(rFP), %eax
    movl    %eax, OUT_ARG0(%esp)
    leal    OFF_FP_SHADOWFRAME(rFP), %ecx
    movl    %ecx, OUT_ARG1(%esp)
    movl    rSELF, %eax
    movl    %eax, OUT_ARG2(%esp)
    call    SYMBOL(MterpAddHotnessBatch)    # (method, shadow_frame, self)
    jmp     .L_no_count_backwards

/*
 * Entered from the conditional branch handlers when OSR check request active on
 * not-taken path.  All Dalvik not-taken conditional branch offsets are 2.
 */
.L_check_not_taken_osr:
    EXPORT_PC
    movl    rSELF, %eax
    movl    %eax, OUT_ARG0(%esp)
    leal    OFF_FP_SHADOWFRAME(rFP), %ecx
    movl    %ecx, OUT_ARG1(%esp)
    movl    $$2, OUT_ARG2(%esp)
    call    SYMBOL(MterpMaybeDoOnStackReplacement) # (self, shadow_frame, offset)
    testb   %al, %al
    REFRESH_IBASE
    jnz     MterpOnStackReplacement
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

/*
 * On-stack replacement has happened, and now we've returned from the compiled method.
 */
MterpOnStackReplacement:
#if MTERP_LOGGING
    movl    rSELF, %eax
    movl    %eax, OUT_ARG0(%esp)
    lea     OFF_FP_SHADOWFRAME(rFP), %ecx
    movl    %ecx, OUT_ARG1(%esp)
    movl    rINST, OUT_ARG2(%esp)
    call    SYMBOL(MterpLogOSR)
#endif
    movl    $$1, %eax
    jmp     MterpDone

/*
 * Bail out to reference interpreter.
 */
MterpFallback:
    EXPORT_PC
#if MTERP_LOGGING
    movl    rSELF, %eax
    movl    %eax, OUT_ARG0(%esp)
    lea     OFF_FP_SHADOWFRAME(rFP), %ecx
    movl    %ecx, OUT_ARG1(%esp)
    call    SYMBOL(MterpLogFallback)
#endif
MterpCommonFallback:
    xor     %eax, %eax
    jmp     MterpDone

/*
 * On entry:
 *  uint32_t* rFP  (should still be live, pointer to base of vregs)
 */
MterpExceptionReturn:
    movl    $$1, %eax
    jmp     MterpDone
MterpReturn:
    movl    OFF_FP_RESULT_REGISTER(rFP), %edx
    movl    %eax, (%edx)
    movl    %ecx, 4(%edx)
    mov     $$1, %eax
MterpDone:
/*
 * At this point, we expect rPROFILE to be non-zero.  If negative, hotness is disabled or we're
 * checking for OSR.  If greater than zero, we might have unreported hotness to register
 * (the difference between the ending rPROFILE and the cached hotness counter).  rPROFILE
 * should only reach zero immediately after a hotness decrement, and is then reset to either
 * a negative special state or the new non-zero countdown value.
 */
    cmpw    $$0, rPROFILE
    jle     MRestoreFrame                   # if > 0, we may have some counts to report.

    movl    %eax, rINST                     # stash return value
    /* Report cached hotness counts */
    movl    OFF_FP_METHOD(rFP), %eax
    movl    %eax, OUT_ARG0(%esp)
    leal    OFF_FP_SHADOWFRAME(rFP), %ecx
    movl    %ecx, OUT_ARG1(%esp)
    movl    rSELF, %eax
    movl    %eax, OUT_ARG2(%esp)
    call    SYMBOL(MterpAddHotnessBatch)    # (method, shadow_frame, self)
    movl    rINST, %eax                     # restore return value

    /* pop up frame */
MRestoreFrame:
    addl    $$FRAME_SIZE, %esp
    .cfi_adjust_cfa_offset -FRAME_SIZE

    /* Restore callee save register */
    POP     %ebx
    POP     %esi
    POP     %edi
    POP     %ebp
    ret
    .cfi_endproc
    END MterpHelpers

%def instruction_end():

    OBJECT_TYPE(artMterpAsmInstructionEnd)
    ASM_HIDDEN SYMBOL(artMterpAsmInstructionEnd)
    .global SYMBOL(artMterpAsmInstructionEnd)
SYMBOL(artMterpAsmInstructionEnd):

%def instruction_start():

    OBJECT_TYPE(artMterpAsmInstructionStart)
    ASM_HIDDEN SYMBOL(artMterpAsmInstructionStart)
    .global SYMBOL(artMterpAsmInstructionStart)
SYMBOL(artMterpAsmInstructionStart) = .L_op_nop
    .text

%def opcode_start():
    ENTRY mterp_${opcode}
%def opcode_end():
    END mterp_${opcode}
%def helper_start(name):
    ENTRY ${name}
%def helper_end(name):
    END ${name}
