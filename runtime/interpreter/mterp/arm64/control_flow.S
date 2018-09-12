%def bincmp(condition=""):
    /*
     * Generic two-operand compare-and-branch operation.  Provide a "condition"
     * fragment that specifies the comparison to perform.
     *
     * For: if-eq, if-ne, if-lt, if-ge, if-gt, if-le
     */
    /* if-cmp vA, vB, +CCCC */
    lsr     w1, wINST, #12              // w1<- B
    ubfx    w0, wINST, #8, #4           // w0<- A
    GET_VREG w3, w1                     // w3<- vB
    GET_VREG w2, w0                     // w2<- vA
    FETCH_S wINST, 1                    // wINST<- branch offset, in code units
    cmp     w2, w3                      // compare (vA, vB)
    b.${condition} MterpCommonTakenBranchNoFlags
    cmp     wPROFILE, #JIT_CHECK_OSR    // possible OSR re-entry?
    b.eq    .L_check_not_taken_osr
    FETCH_ADVANCE_INST 2
    GET_INST_OPCODE ip                  // extract opcode from wINST
    GOTO_OPCODE ip                      // jump to next instruction

%def zcmp(compare="1", branch=""):
    /*
     * Generic one-operand compare-and-branch operation.  Provide a "condition"
     * fragment that specifies the comparison to perform.
     *
     * for: if-eqz, if-nez, if-ltz, if-gez, if-gtz, if-lez
     */
    /* if-cmp vAA, +BBBB */
    lsr     w0, wINST, #8               // w0<- AA
    GET_VREG w2, w0                     // w2<- vAA
    FETCH_S wINST, 1                    // w1<- branch offset, in code units
    .if ${compare}
    cmp     w2, #0                      // compare (vA, 0)
    .endif
    ${branch} MterpCommonTakenBranchNoFlags
    cmp     wPROFILE, #JIT_CHECK_OSR    // possible OSR re-entry?
    b.eq    .L_check_not_taken_osr
    FETCH_ADVANCE_INST 2
    GET_INST_OPCODE ip                  // extract opcode from wINST
    GOTO_OPCODE ip                      // jump to next instruction

%def op_goto():
    /*
     * Unconditional branch, 8-bit offset.
     *
     * The branch distance is a signed code-unit offset, which we need to
     * double to get a byte offset.
     */
    /* goto +AA */
    sbfx    wINST, wINST, #8, #8           // wINST<- ssssssAA (sign-extended)
    b       MterpCommonTakenBranchNoFlags

%def op_goto_16():
    /*
     * Unconditional branch, 16-bit offset.
     *
     * The branch distance is a signed code-unit offset, which we need to
     * double to get a byte offset.
     */
    /* goto/16 +AAAA */
    FETCH_S wINST, 1                    // wINST<- ssssAAAA (sign-extended)
    b       MterpCommonTakenBranchNoFlags

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
    FETCH w0, 1                         // w0<- aaaa (lo)
    FETCH w1, 2                         // w1<- AAAA (hi)
    orr     wINST, w0, w1, lsl #16      // wINST<- AAAAaaaa
    b       MterpCommonTakenBranchNoFlags

%def op_if_eq():
%  bincmp(condition="eq")

%def op_if_eqz():
%  zcmp(compare="0", branch="cbz     w2,")

%def op_if_ge():
%  bincmp(condition="ge")

%def op_if_gez():
%  zcmp(compare="0", branch="tbz     w2, #31,")

%def op_if_gt():
%  bincmp(condition="gt")

%def op_if_gtz():
%  zcmp(branch="b.gt")

%def op_if_le():
%  bincmp(condition="le")

%def op_if_lez():
%  zcmp(branch="b.le")

%def op_if_lt():
%  bincmp(condition="lt")

%def op_if_ltz():
%  zcmp(compare="0", branch="tbnz    w2, #31,")

%def op_if_ne():
%  bincmp(condition="ne")

%def op_if_nez():
%  zcmp(compare="0", branch="cbnz    w2,")

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
    FETCH   w0, 1                       // x0<- 000000000000bbbb (lo)
    FETCH_S x1, 2                       // x1<- ssssssssssssBBBB (hi)
    lsr     w3, wINST, #8               // w3<- AA
    orr     x0, x0, x1, lsl #16         // x0<- ssssssssBBBBbbbb
    GET_VREG w1, w3                     // w1<- vAA
    add     x0, xPC, x0, lsl #1         // x0<- PC + ssssssssBBBBbbbb*2
    bl      $func                       // w0<- code-unit branch offset
    sxtw    xINST, w0
    b       MterpCommonTakenBranchNoFlags

%def op_return():
    /*
     * Return a 32-bit value.
     *
     * for: return, return-object
     */
    /* op vAA */
    .extern MterpThreadFenceForConstructor
    bl      MterpThreadFenceForConstructor
    ldr     w7, [xSELF, #THREAD_FLAGS_OFFSET]
    mov     x0, xSELF
    ands    w7, w7, #THREAD_SUSPEND_OR_CHECKPOINT_REQUEST
    b.ne    .L${opcode}_check
.L${opcode}_return:
    lsr     w2, wINST, #8               // r2<- AA
    GET_VREG w0, w2                     // r0<- vAA
    b       MterpReturn
.L${opcode}_check:
    bl      MterpSuspendCheck           // (self)
    b       .L${opcode}_return

%def op_return_object():
%  op_return()

%def op_return_void():
    .extern MterpThreadFenceForConstructor
    bl      MterpThreadFenceForConstructor
    ldr     w7, [xSELF, #THREAD_FLAGS_OFFSET]
    mov     x0, xSELF
    ands    w7, w7, #THREAD_SUSPEND_OR_CHECKPOINT_REQUEST
    b.ne    .L${opcode}_check
.L${opcode}_return:
    mov     x0, #0
    b       MterpReturn
.L${opcode}_check:
    bl      MterpSuspendCheck           // (self)
    b       .L${opcode}_return

%def op_return_void_no_barrier():
    ldr     w7, [xSELF, #THREAD_FLAGS_OFFSET]
    mov     x0, xSELF
    ands    w7, w7, #THREAD_SUSPEND_OR_CHECKPOINT_REQUEST
    b.ne    .L${opcode}_check
.L${opcode}_return:
    mov     x0, #0
    b       MterpReturn
.L${opcode}_check:
    bl      MterpSuspendCheck           // (self)
    b       .L${opcode}_return

%def op_return_wide():
    /*
     * Return a 64-bit value.
     */
    /* return-wide vAA */
    /* op vAA */
    .extern MterpThreadFenceForConstructor
    bl      MterpThreadFenceForConstructor
    ldr     w7, [xSELF, #THREAD_FLAGS_OFFSET]
    mov     x0, xSELF
    ands    w7, w7, #THREAD_SUSPEND_OR_CHECKPOINT_REQUEST
    b.ne    .L${opcode}_check
.L${opcode}_return:
    lsr     w2, wINST, #8               // w2<- AA
    GET_VREG_WIDE x0, w2                // x0<- vAA
    b       MterpReturn
.L${opcode}_check:
    bl      MterpSuspendCheck           // (self)
    b       .L${opcode}_return

%def op_sparse_switch():
%  op_packed_switch(func="MterpDoSparseSwitch")

%def op_throw():
    /*
     * Throw an exception object in the current thread.
     */
    /* throw vAA */
    EXPORT_PC
    lsr      w2, wINST, #8               // r2<- AA
    GET_VREG w1, w2                      // r1<- vAA (exception object)
    cbz      w1, common_errNullObject
    str      x1, [xSELF, #THREAD_EXCEPTION_OFFSET]  // thread->exception<- obj
    b        MterpException
