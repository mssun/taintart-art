%def bincmp(condition=""):
    /*
     * Generic two-operand compare-and-branch operation.  Provide a "condition"
     * fragment that specifies the comparison to perform.
     *
     * For: if-eq, if-ne, if-lt, if-ge, if-gt, if-le
     */
    /* if-cmp vA, vB, +CCCC */
    GET_OPA4(a0)                           #  a0 <- A+
    GET_OPB(a1)                            #  a1 <- B
    GET_VREG(a3, a1)                       #  a3 <- vB
    GET_VREG(a0, a0)                       #  a0 <- vA
    FETCH_S(rINST, 1)                      #  rINST<- branch offset, in code units
    b${condition} a0, a3, MterpCommonTakenBranchNoFlags  #  compare (vA, vB)
    li        t0, JIT_CHECK_OSR
    beq       rPROFILE, t0, .L_check_not_taken_osr
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction

%def zcmp(condition=""):
    /*
     * Generic one-operand compare-and-branch operation.  Provide a "condition"
     * fragment that specifies the comparison to perform.
     *
     * for: if-eqz, if-nez, if-ltz, if-gez, if-gtz, if-lez
     */
    /* if-cmp vAA, +BBBB */
    GET_OPA(a0)                            #  a0 <- AA
    GET_VREG(a0, a0)                       #  a0 <- vAA
    FETCH_S(rINST, 1)                      #  rINST <- branch offset, in code units
    b${condition} a0, zero, MterpCommonTakenBranchNoFlags
    li        t0, JIT_CHECK_OSR            # possible OSR re-entry?
    beq       rPROFILE, t0, .L_check_not_taken_osr
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction

%def op_goto():
    /*
     * Unconditional branch, 8-bit offset.
     *
     * The branch distance is a signed code-unit offset, which we need to
     * double to get a byte offset.
     */
    /* goto +AA */
    sll       a0, rINST, 16                #  a0 <- AAxx0000
    sra       rINST, a0, 24                #  rINST <- ssssssAA (sign-extended)
    b       MterpCommonTakenBranchNoFlags

%def op_goto_16():
    /*
     * Unconditional branch, 16-bit offset.
     *
     * The branch distance is a signed code-unit offset, which we need to
     * double to get a byte offset.
     */
    /* goto/16 +AAAA */
    FETCH_S(rINST, 1)                      #  rINST <- ssssAAAA (sign-extended)
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
    FETCH(rINST, 1)                        #  rINST <- aaaa (lo)
    FETCH(a1, 2)                           #  a1 <- AAAA (hi)
    INSERT_HIGH_HALF(rINST, a1)            #  rINST <- AAAAaaaa
    b         MterpCommonTakenBranchNoFlags

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
    /* op vAA, +BBBB */
    FETCH(a0, 1)                           #  a0 <- bbbb (lo)
    FETCH(a1, 2)                           #  a1 <- BBBB (hi)
    GET_OPA(a3)                            #  a3 <- AA
    INSERT_HIGH_HALF(a0, a1)               #  a0 <- BBBBbbbb
    GET_VREG(a1, a3)                       #  a1 <- vAA
    EAS1(a0, rPC, a0)                      #  a0 <- PC + BBBBbbbb*2
    JAL($func)                             #  a0 <- code-unit branch offset
    move      rINST, v0
    b         MterpCommonTakenBranchNoFlags

%def op_return():
    /*
     * Return a 32-bit value.
     *
     * for: return, return-object
     */
    /* op vAA */
    .extern MterpThreadFenceForConstructor
    JAL(MterpThreadFenceForConstructor)
    lw        ra, THREAD_FLAGS_OFFSET(rSELF)
    move      a0, rSELF
    and       ra, THREAD_SUSPEND_OR_CHECKPOINT_REQUEST
    beqz      ra, 1f
    JAL(MterpSuspendCheck)                 # (self)
1:
    GET_OPA(a2)                            #  a2 <- AA
    GET_VREG(v0, a2)                       #  v0 <- vAA
    move      v1, zero
    b         MterpReturn

%def op_return_object():
%  op_return()

%def op_return_void():
    .extern MterpThreadFenceForConstructor
    JAL(MterpThreadFenceForConstructor)
    lw        ra, THREAD_FLAGS_OFFSET(rSELF)
    move      a0, rSELF
    and       ra, THREAD_SUSPEND_OR_CHECKPOINT_REQUEST
    beqz      ra, 1f
    JAL(MterpSuspendCheck)                 # (self)
1:
    move      v0, zero
    move      v1, zero
    b         MterpReturn

%def op_return_void_no_barrier():
    lw     ra, THREAD_FLAGS_OFFSET(rSELF)
    move   a0, rSELF
    and    ra, THREAD_SUSPEND_OR_CHECKPOINT_REQUEST
    beqz   ra, 1f
    JAL(MterpSuspendCheck)                 # (self)
1:
    move   v0, zero
    move   v1, zero
    b      MterpReturn

%def op_return_wide():
    /*
     * Return a 64-bit value.
     */
    /* return-wide vAA */
    .extern MterpThreadFenceForConstructor
    JAL(MterpThreadFenceForConstructor)
    lw        ra, THREAD_FLAGS_OFFSET(rSELF)
    move      a0, rSELF
    and       ra, THREAD_SUSPEND_OR_CHECKPOINT_REQUEST
    beqz      ra, 1f
    JAL(MterpSuspendCheck)                 # (self)
1:
    GET_OPA(a2)                            #  a2 <- AA
    EAS2(a2, rFP, a2)                      #  a2 <- &fp[AA]
    LOAD64(v0, v1, a2)                     #  v0/v1 <- vAA/vAA+1
    b         MterpReturn

%def op_sparse_switch():
%  op_packed_switch(func="MterpDoSparseSwitch")

%def op_throw():
    /*
     * Throw an exception object in the current thread.
     */
    /* throw vAA */
    EXPORT_PC()                              #  exception handler can throw
    GET_OPA(a2)                              #  a2 <- AA
    GET_VREG(a1, a2)                         #  a1 <- vAA (exception object)
    # null object?
    beqz  a1, common_errNullObject           #  yes, throw an NPE instead
    sw    a1, THREAD_EXCEPTION_OFFSET(rSELF) #  thread->exception <- obj
    b         MterpException
