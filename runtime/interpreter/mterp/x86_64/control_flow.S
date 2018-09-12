%def bincmp(revcmp=""):
/*
 * Generic two-operand compare-and-branch operation.  Provide a "revcmp"
 * fragment that specifies the *reverse* comparison to perform, e.g.
 * for "if-le" you would use "gt".
 *
 * For: if-eq, if-ne, if-lt, if-ge, if-gt, if-le
 */
    /* if-cmp vA, vB, +CCCC */
    movl    rINST, %ecx                     # rcx <- A+
    sarl    $$4, rINST                      # rINST <- B
    andb    $$0xf, %cl                      # rcx <- A
    GET_VREG %eax, %rcx                     # eax <- vA
    cmpl    VREG_ADDRESS(rINSTq), %eax      # compare (vA, vB)
    j${revcmp}   1f
    movswq  2(rPC), rINSTq                  # Get signed branch offset
    testq   rINSTq, rINSTq
    jmp     MterpCommonTakenBranch
1:
    cmpl    $$JIT_CHECK_OSR, rPROFILE
    je      .L_check_not_taken_osr
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def zcmp(revcmp=""):
/*
 * Generic one-operand compare-and-branch operation.  Provide a "revcmp"
 * fragment that specifies the *reverse* comparison to perform, e.g.
 * for "if-le" you would use "gt".
 *
 * for: if-eqz, if-nez, if-ltz, if-gez, if-gtz, if-lez
 */
    /* if-cmp vAA, +BBBB */
    cmpl    $$0, VREG_ADDRESS(rINSTq)       # compare (vA, 0)
    j${revcmp}   1f
    movswq  2(rPC), rINSTq                  # fetch signed displacement
    testq   rINSTq, rINSTq
    jmp     MterpCommonTakenBranch
1:
    cmpl    $$JIT_CHECK_OSR, rPROFILE
    je      .L_check_not_taken_osr
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def op_goto():
/*
 * Unconditional branch, 8-bit offset.
 *
 * The branch distance is a signed code-unit offset, which we need to
 * double to get a byte offset.
 */
    /* goto +AA */
    movsbq  rINSTbl, rINSTq                 # rINSTq <- ssssssAA
    testq   rINSTq, rINSTq
    jmp     MterpCommonTakenBranch

%def op_goto_16():
/*
 * Unconditional branch, 16-bit offset.
 *
 * The branch distance is a signed code-unit offset, which we need to
 * double to get a byte offset.
 */
    /* goto/16 +AAAA */
    movswq  2(rPC), rINSTq                  # rINSTq <- ssssAAAA
    testq   rINSTq, rINSTq
    jmp     MterpCommonTakenBranch

%def op_goto_32():
/*
 * Unconditional branch, 32-bit offset.
 *
 * The branch distance is a signed code-unit offset, which we need to
 * double to get a byte offset.
 *
 *  Because we need the SF bit set, we'll use an adds
 * to convert from Dalvik offset to byte offset.
 */
    /* goto/32 +AAAAAAAA */
    movslq  2(rPC), rINSTq                  # rINSTq <- AAAAAAAA
    testq   rINSTq, rINSTq
    jmp     MterpCommonTakenBranch

%def op_if_eq():
%  bincmp(revcmp="ne")

%def op_if_eqz():
%  zcmp(revcmp="ne")

%def op_if_ge():
%  bincmp(revcmp="l")

%def op_if_gez():
%  zcmp(revcmp="l")

%def op_if_gt():
%  bincmp(revcmp="le")

%def op_if_gtz():
%  zcmp(revcmp="le")

%def op_if_le():
%  bincmp(revcmp="g")

%def op_if_lez():
%  zcmp(revcmp="g")

%def op_if_lt():
%  bincmp(revcmp="ge")

%def op_if_ltz():
%  zcmp(revcmp="ge")

%def op_if_ne():
%  bincmp(revcmp="e")

%def op_if_nez():
%  zcmp(revcmp="e")

%def op_packed_switch(func="MterpDoPackedSwitch"):
/*
 * Handle a packed-switch or sparse-switch instruction.  In both cases
 * we decode it and hand it off to a helper function.
 *
 * We don't really expect backward branches in a switch statement, but
 * they're perfectly legal, so we check for them here.
 *
 * for: packed-switch, sparse-switch
 */
    /* op vAA, +BBBB */
    movslq  2(rPC), OUT_ARG0                # rcx <- ssssssssBBBBbbbb
    leaq    (rPC,OUT_ARG0,2), OUT_ARG0      # rcx <- PC + ssssssssBBBBbbbb*2
    GET_VREG OUT_32_ARG1, rINSTq            # eax <- vAA
    call    SYMBOL($func)
    testl   %eax, %eax
    movslq  %eax, rINSTq
    jmp     MterpCommonTakenBranch

%def op_return():
/*
 * Return a 32-bit value.
 *
 * for: return, return-object
 */
    /* op vAA */
    .extern MterpThreadFenceForConstructor
    call    SYMBOL(MterpThreadFenceForConstructor)
    movq    rSELF, OUT_ARG0
    testl   $$(THREAD_SUSPEND_OR_CHECKPOINT_REQUEST), THREAD_FLAGS_OFFSET(OUT_ARG0)
    jz      1f
    call    SYMBOL(MterpSuspendCheck)
1:
    GET_VREG %eax, rINSTq                   # eax <- vAA
    jmp     MterpReturn

%def op_return_object():
%  op_return()

%def op_return_void():
    .extern MterpThreadFenceForConstructor
    call    SYMBOL(MterpThreadFenceForConstructor)
    movq    rSELF, OUT_ARG0
    testl   $$(THREAD_SUSPEND_OR_CHECKPOINT_REQUEST), THREAD_FLAGS_OFFSET(OUT_ARG0)
    jz      1f
    call    SYMBOL(MterpSuspendCheck)
1:
    xorq    %rax, %rax
    jmp     MterpReturn

%def op_return_void_no_barrier():
    movq    rSELF, OUT_ARG0
    testl   $$(THREAD_SUSPEND_OR_CHECKPOINT_REQUEST), THREAD_FLAGS_OFFSET(OUT_ARG0)
    jz      1f
    call    SYMBOL(MterpSuspendCheck)
1:
    xorq    %rax, %rax
    jmp     MterpReturn

%def op_return_wide():
/*
 * Return a 64-bit value.
 */
    /* return-wide vAA */
    .extern MterpThreadFenceForConstructor
    call    SYMBOL(MterpThreadFenceForConstructor)
    movq    rSELF, OUT_ARG0
    testl   $$(THREAD_SUSPEND_OR_CHECKPOINT_REQUEST), THREAD_FLAGS_OFFSET(OUT_ARG0)
    jz      1f
    call    SYMBOL(MterpSuspendCheck)
1:
    GET_WIDE_VREG %rax, rINSTq              # eax <- v[AA]
    jmp     MterpReturn

%def op_sparse_switch():
%  op_packed_switch(func="MterpDoSparseSwitch")

%def op_throw():
/*
 * Throw an exception object in the current thread.
 */
    /* throw vAA */
    EXPORT_PC
    GET_VREG %eax, rINSTq                   # eax<- vAA (exception object)
    testb   %al, %al
    jz      common_errNullObject
    movq    rSELF, %rcx
    movq    %rax, THREAD_EXCEPTION_OFFSET(%rcx)
    jmp     MterpException
