%def bincmp(revcmp=""):
/*
 * Generic two-operand compare-and-branch operation.  Provide a "revcmp"
 * fragment that specifies the *reverse* comparison to perform, e.g.
 * for "if-le" you would use "gt".
 *
 * For: if-eq, if-ne, if-lt, if-ge, if-gt, if-le
 */
    /* if-cmp vA, vB, +CCCC */
    movzx   rINSTbl, %ecx                   # ecx <- A+
    andb    $$0xf, %cl                      # ecx <- A
    GET_VREG %eax, %ecx                     # eax <- vA
    sarl    $$4, rINST                      # rINST <- B
    cmpl    VREG_ADDRESS(rINST), %eax       # compare (vA, vB)
    j${revcmp}   1f
    movswl  2(rPC), rINST                   # Get signed branch offset
    testl   rINST, rINST
    jmp     MterpCommonTakenBranch
1:
    cmpw    $$JIT_CHECK_OSR, rPROFILE
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
    cmpl    $$0, VREG_ADDRESS(rINST)        # compare (vA, 0)
    j${revcmp}   1f
    movswl  2(rPC), rINST                   # fetch signed displacement
    testl   rINST, rINST
    jmp     MterpCommonTakenBranch
1:
    cmpw    $$JIT_CHECK_OSR, rPROFILE
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
    movsbl  rINSTbl, rINST                  # rINST <- ssssssAA
    testl   rINST, rINST
    jmp     MterpCommonTakenBranch

%def op_goto_16():
/*
 * Unconditional branch, 16-bit offset.
 *
 * The branch distance is a signed code-unit offset, which we need to
 * double to get a byte offset.
 */
    /* goto/16 +AAAA */
    movswl  2(rPC), rINST                   # rINST <- ssssAAAA
    testl   rINST, rINST
    jmp     MterpCommonTakenBranch

%def op_goto_32():
/*
 * Unconditional branch, 32-bit offset.
 *
 * The branch distance is a signed code-unit offset, which we need to
 * double to get a byte offset.
 *
 * Unlike most opcodes, this one is allowed to branch to itself, so
 * our "backward branch" test must be "<=0" instead of "<0".  Because
 * we need the V bit set, we'll use an adds to convert from Dalvik
 * offset to byte offset.
 */
    /* goto/32 +AAAAAAAA */
    movl    2(rPC), rINST                   # rINST <- AAAAAAAA
    testl   rINST, rINST
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
    movl    2(rPC), %ecx                    # ecx <- BBBBbbbb
    GET_VREG %eax, rINST                    # eax <- vAA
    leal    (rPC,%ecx,2), %ecx              # ecx <- PC + BBBBbbbb*2
    movl    %eax, OUT_ARG1(%esp)            # ARG1 <- vAA
    movl    %ecx, OUT_ARG0(%esp)            # ARG0 <- switchData
    call    SYMBOL($func)
    REFRESH_IBASE
    testl   %eax, %eax
    movl    %eax, rINST
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
    movl    rSELF, %eax
    testl   $$(THREAD_SUSPEND_OR_CHECKPOINT_REQUEST), THREAD_FLAGS_OFFSET(%eax)
    jz      1f
    movl    %eax, OUT_ARG0(%esp)
    call    SYMBOL(MterpSuspendCheck)
1:
    GET_VREG %eax, rINST                    # eax <- vAA
    xorl    %ecx, %ecx
    jmp     MterpReturn

%def op_return_object():
%  op_return()

%def op_return_void():
    .extern MterpThreadFenceForConstructor
    call    SYMBOL(MterpThreadFenceForConstructor)
    movl    rSELF, %eax
    testl   $$(THREAD_SUSPEND_OR_CHECKPOINT_REQUEST), THREAD_FLAGS_OFFSET(%eax)
    jz      1f
    movl    %eax, OUT_ARG0(%esp)
    call    SYMBOL(MterpSuspendCheck)
1:
    xorl    %eax, %eax
    xorl    %ecx, %ecx
    jmp     MterpReturn

%def op_return_void_no_barrier():
    movl    rSELF, %eax
    testl   $$(THREAD_SUSPEND_OR_CHECKPOINT_REQUEST), THREAD_FLAGS_OFFSET(%eax)
    jz      1f
    movl    %eax, OUT_ARG0(%esp)
    call    SYMBOL(MterpSuspendCheck)
1:
    xorl    %eax, %eax
    xorl    %ecx, %ecx
    jmp     MterpReturn

%def op_return_wide():
/*
 * Return a 64-bit value.
 */
    /* return-wide vAA */
    .extern MterpThreadFenceForConstructor
    call    SYMBOL(MterpThreadFenceForConstructor)
    movl    rSELF, %eax
    testl   $$(THREAD_SUSPEND_OR_CHECKPOINT_REQUEST), THREAD_FLAGS_OFFSET(%eax)
    jz      1f
    movl    %eax, OUT_ARG0(%esp)
    call    SYMBOL(MterpSuspendCheck)
1:
    GET_VREG %eax, rINST                    # eax <- v[AA+0]
    GET_VREG_HIGH %ecx, rINST               # ecx <- v[AA+1]
    jmp     MterpReturn

%def op_sparse_switch():
%  op_packed_switch(func="MterpDoSparseSwitch")

%def op_throw():
/*
 * Throw an exception object in the current thread.
 */
    /* throw vAA */
    EXPORT_PC
    GET_VREG %eax, rINST                    # eax<- vAA (exception object)
    testl   %eax, %eax
    jz      common_errNullObject
    movl    rSELF,%ecx
    movl    %eax, THREAD_EXCEPTION_OFFSET(%ecx)
    jmp     MterpException
