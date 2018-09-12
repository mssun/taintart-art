%def bincmp(condition=""):
    /*
     * Generic two-operand compare-and-branch operation.  Provide a "condition"
     * fragment that specifies the comparison to perform, e.g. for
     * "if-le" you would use "le".
     *
     * For: if-eq, if-ne, if-lt, if-ge, if-gt, if-le
     */
    /* if-cmp vA, vB, +CCCC */
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    lh      rINST, 2(rPC)               # rINST <- offset (sign-extended CCCC)
    GET_VREG a0, a2                     # a0 <- vA
    GET_VREG a1, a3                     # a1 <- vB
    b${condition}c a0, a1, MterpCommonTakenBranchNoFlags
    li      v0, JIT_CHECK_OSR           # possible OSR re-entry?
    beqc    rPROFILE, v0, .L_check_not_taken_osr
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction

%def zcmp(condition=""):
    /*
     * Generic one-operand compare-and-branch operation.  Provide a "condition"
     * fragment that specifies the comparison to perform, e.g. for
     * "if-lez" you would use "le".
     *
     * For: if-eqz, if-nez, if-ltz, if-gez, if-gtz, if-lez
     */
    /* if-cmp vAA, +BBBB */
    srl     a2, rINST, 8                # a2 <- AA
    lh      rINST, 2(rPC)               # rINST <- offset (sign-extended BBBB)
    GET_VREG a0, a2                     # a0 <- vAA
    b${condition}zc a0, MterpCommonTakenBranchNoFlags
    li      v0, JIT_CHECK_OSR           # possible OSR re-entry?
    beqc    rPROFILE, v0, .L_check_not_taken_osr
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction

%def op_goto():
    /*
     * Unconditional branch, 8-bit offset.
     *
     * The branch distance is a signed code-unit offset, which we need to
     * double to get a byte offset.
     */
    /* goto +AA */
    srl     rINST, rINST, 8
    seb     rINST, rINST                # rINST <- offset (sign-extended AA)
    b       MterpCommonTakenBranchNoFlags

%def op_goto_16():
    /*
     * Unconditional branch, 16-bit offset.
     *
     * The branch distance is a signed code-unit offset, which we need to
     * double to get a byte offset.
     */
    /* goto/16 +AAAA */
    lh      rINST, 2(rPC)               # rINST <- offset (sign-extended AAAA)
    b       MterpCommonTakenBranchNoFlags

%def op_goto_32():
    /*
     * Unconditional branch, 32-bit offset.
     *
     * The branch distance is a signed code-unit offset, which we need to
     * double to get a byte offset.
     *
     * Unlike most opcodes, this one is allowed to branch to itself, so
     * our "backward branch" test must be "<=0" instead of "<0".
     */
    /* goto/32 +AAAAAAAA */
    lh      rINST, 2(rPC)               # rINST <- aaaa (low)
    lh      a1, 4(rPC)                  # a1 <- AAAA (high)
    ins     rINST, a1, 16, 16           # rINST <- offset (sign-extended AAAAaaaa)
    b       MterpCommonTakenBranchNoFlags

%def op_if_eq():
%  bincmp(condition="eq")

%def op_if_eqz():
%  zcmp(condition="eq")

%def op_if_ge():
%  bincmp(condition="ge")

%def op_if_gez():
%  zcmp(condition="ge")

%def op_if_gt():
%  bincmp(condition="gt")

%def op_if_gtz():
%  zcmp(condition="gt")

%def op_if_le():
%  bincmp(condition="le")

%def op_if_lez():
%  zcmp(condition="le")

%def op_if_lt():
%  bincmp(condition="lt")

%def op_if_ltz():
%  zcmp(condition="lt")

%def op_if_ne():
%  bincmp(condition="ne")

%def op_if_nez():
%  zcmp(condition="ne")

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
    /* op vAA, +BBBBBBBB */
    .extern $func
    lh      a0, 2(rPC)                  # a0 <- bbbb (lo)
    lh      a1, 4(rPC)                  # a1 <- BBBB (hi)
    srl     a3, rINST, 8                # a3 <- AA
    ins     a0, a1, 16, 16              # a0 <- BBBBbbbb
    GET_VREG a1, a3                     # a1 <- vAA
    dlsa    a0, a0, rPC, 1              # a0 <- PC + BBBBbbbb*2
    jal     $func                       # v0 <- code-unit branch offset
    move    rINST, v0
    b       MterpCommonTakenBranchNoFlags

%def op_return(instr="GET_VREG"):
    /*
     * Return a 32-bit value.
     *
     * for: return (sign-extend), return-object (zero-extend)
     */
    /* op vAA */
    .extern MterpThreadFenceForConstructor
    .extern MterpSuspendCheck
    jal     MterpThreadFenceForConstructor
    lw      ra, THREAD_FLAGS_OFFSET(rSELF)
    move    a0, rSELF
    and     ra, ra, THREAD_SUSPEND_OR_CHECKPOINT_REQUEST
    beqzc   ra, 1f
    jal     MterpSuspendCheck           # (self)
1:
    srl     a2, rINST, 8                # a2 <- AA
    $instr  a0, a2                      # a0 <- vAA
    b       MterpReturn

%def op_return_object():
%  op_return(instr="GET_VREG_U")

%def op_return_void():
    .extern MterpThreadFenceForConstructor
    .extern MterpSuspendCheck
    jal     MterpThreadFenceForConstructor
    lw      ra, THREAD_FLAGS_OFFSET(rSELF)
    move    a0, rSELF
    and     ra, ra, THREAD_SUSPEND_OR_CHECKPOINT_REQUEST
    beqzc   ra, 1f
    jal     MterpSuspendCheck           # (self)
1:
    li      a0, 0
    b       MterpReturn

%def op_return_void_no_barrier():
    .extern MterpSuspendCheck
    lw      ra, THREAD_FLAGS_OFFSET(rSELF)
    move    a0, rSELF
    and     ra, ra, THREAD_SUSPEND_OR_CHECKPOINT_REQUEST
    beqzc   ra, 1f
    jal     MterpSuspendCheck           # (self)
1:
    li      a0, 0
    b       MterpReturn

%def op_return_wide():
    /*
     * Return a 64-bit value.
     */
    /* return-wide vAA */
    /* op vAA */
    .extern MterpThreadFenceForConstructor
    .extern MterpSuspendCheck
    jal     MterpThreadFenceForConstructor
    lw      ra, THREAD_FLAGS_OFFSET(rSELF)
    move    a0, rSELF
    and     ra, ra, THREAD_SUSPEND_OR_CHECKPOINT_REQUEST
    beqzc   ra, 1f
    jal     MterpSuspendCheck           # (self)
1:
    srl     a2, rINST, 8                # a2 <- AA
    GET_VREG_WIDE a0, a2                # a0 <- vAA
    b       MterpReturn

%def op_sparse_switch():
%  op_packed_switch(func="MterpDoSparseSwitch")

%def op_throw():
    /*
     * Throw an exception object in the current thread.
     */
    /* throw vAA */
    EXPORT_PC
    srl     a2, rINST, 8                # a2 <- AA
    GET_VREG_U a0, a2                   # a0 <- vAA (exception object)
    beqzc   a0, common_errNullObject
    sd      a0, THREAD_EXCEPTION_OFFSET(rSELF)  # thread->exception <- obj
    b       MterpException
