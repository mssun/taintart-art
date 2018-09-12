%def const(helper="UndefinedConstHandler"):
    /* const/class vAA, type@BBBB */
    /* const/method-handle vAA, method_handle@BBBB */
    /* const/method-type vAA, proto@BBBB */
    /* const/string vAA, string@@BBBB */
    .extern $helper
    EXPORT_PC()
    FETCH(a0, 1)                        # a0 <- BBBB
    GET_OPA(a1)                         # a1 <- AA
    addu   a2, rFP, OFF_FP_SHADOWFRAME  # a2 <- shadow frame
    move   a3, rSELF
    JAL($helper)                        # v0 <- Mterp(index, tgt_reg, shadow_frame, self)
    PREFETCH_INST(2)                    # load rINST
    bnez   v0, MterpPossibleException
    ADVANCE(2)                          # advance rPC
    GET_INST_OPCODE(t0)                 # extract opcode from rINST
    GOTO_OPCODE(t0)                     # jump to next instruction

%def unused():
/*
 * Bail to reference interpreter to throw.
 */
  b MterpFallback

%def op_const():
    /* const vAA, +BBBBbbbb */
    GET_OPA(a3)                            #  a3 <- AA
    FETCH(a0, 1)                           #  a0 <- bbbb (low)
    FETCH(a1, 2)                           #  a1 <- BBBB (high)
    FETCH_ADVANCE_INST(3)                  #  advance rPC, load rINST
    INSERT_HIGH_HALF(a0, a1)               #  a0 <- BBBBbbbb
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, a3, t0)              #  vAA <- a0

%def op_const_16():
    /* const/16 vAA, +BBBB */
    FETCH_S(a0, 1)                         #  a0 <- ssssBBBB (sign-extended)
    GET_OPA(a3)                            #  a3 <- AA
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, a3, t0)              #  vAA <- a0

%def op_const_4():
    /* const/4 vA, +B */
    sll       a1, rINST, 16                #  a1 <- Bxxx0000
    GET_OPA(a0)                            #  a0 <- A+
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    sra       a1, a1, 28                   #  a1 <- sssssssB (sign-extended)
    and       a0, a0, 15
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a1, a0, t0)              #  fp[A] <- a1

%def op_const_class():
%  const(helper="MterpConstClass")

%def op_const_high16():
    /* const/high16 vAA, +BBBB0000 */
    FETCH(a0, 1)                           #  a0 <- 0000BBBB (zero-extended)
    GET_OPA(a3)                            #  a3 <- AA
    sll       a0, a0, 16                   #  a0 <- BBBB0000
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, a3, t0)              #  vAA <- a0

%def op_const_method_handle():
%  const(helper="MterpConstMethodHandle")

%def op_const_method_type():
%  const(helper="MterpConstMethodType")

%def op_const_string():
%  const(helper="MterpConstString")

%def op_const_string_jumbo():
    /* const/string vAA, string@BBBBBBBB */
    EXPORT_PC()
    FETCH(a0, 1)                        # a0 <- bbbb (low)
    FETCH(a2, 2)                        # a2 <- BBBB (high)
    GET_OPA(a1)                         # a1 <- AA
    INSERT_HIGH_HALF(a0, a2)            # a0 <- BBBBbbbb
    addu   a2, rFP, OFF_FP_SHADOWFRAME  # a2 <- shadow frame
    move   a3, rSELF
    JAL(MterpConstString)               # v0 <- Mterp(index, tgt_reg, shadow_frame, self)
    PREFETCH_INST(3)                    # load rINST
    bnez   v0, MterpPossibleException
    ADVANCE(3)                          # advance rPC
    GET_INST_OPCODE(t0)                 # extract opcode from rINST
    GOTO_OPCODE(t0)                     # jump to next instruction

%def op_const_wide():
    /* const-wide vAA, +HHHHhhhhBBBBbbbb */
    FETCH(a0, 1)                           #  a0 <- bbbb (low)
    FETCH(a1, 2)                           #  a1 <- BBBB (low middle)
    FETCH(a2, 3)                           #  a2 <- hhhh (high middle)
    INSERT_HIGH_HALF(a0, a1)               #  a0 <- BBBBbbbb (low word)
    FETCH(a3, 4)                           #  a3 <- HHHH (high)
    GET_OPA(t1)                            #  t1 <- AA
    INSERT_HIGH_HALF(a2, a3)               #  a2 <- HHHHhhhh (high word)
    FETCH_ADVANCE_INST(5)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_GOTO(a0, a2, t1, t0)        #  vAA/vAA+1 <- a0/a2

%def op_const_wide_16():
    /* const-wide/16 vAA, +BBBB */
    FETCH_S(a0, 1)                         #  a0 <- ssssBBBB (sign-extended)
    GET_OPA(a3)                            #  a3 <- AA
    sra       a1, a0, 31                   #  a1 <- ssssssss
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_GOTO(a0, a1, a3, t0)        #  vAA/vAA+1 <- a0/a1

%def op_const_wide_32():
    /* const-wide/32 vAA, +BBBBbbbb */
    FETCH(a0, 1)                           #  a0 <- 0000bbbb (low)
    GET_OPA(a3)                            #  a3 <- AA
    FETCH_S(a2, 2)                         #  a2 <- ssssBBBB (high)
    FETCH_ADVANCE_INST(3)                  #  advance rPC, load rINST
    INSERT_HIGH_HALF(a0, a2)               #  a0 <- BBBBbbbb
    sra       a1, a0, 31                   #  a1 <- ssssssss
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_GOTO(a0, a1, a3, t0)        #  vAA/vAA+1 <- a0/a1

%def op_const_wide_high16():
    /* const-wide/high16 vAA, +BBBB000000000000 */
    FETCH(a1, 1)                           #  a1 <- 0000BBBB (zero-extended)
    GET_OPA(a3)                            #  a3 <- AA
    li        a0, 0                        #  a0 <- 00000000
    sll       a1, 16                       #  a1 <- BBBB0000
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_GOTO(a0, a1, a3, t0)        #  vAA/vAA+1 <- a0/a1

%def op_monitor_enter():
    /*
     * Synchronize on an object.
     */
    /* monitor-enter vAA */
    EXPORT_PC()
    GET_OPA(a2)                            # a2 <- AA
    GET_VREG(a0, a2)                       # a0 <- vAA (object)
    move   a1, rSELF                       # a1 <- self
    JAL(artLockObjectFromCode)             # v0 <- artLockObject(obj, self)
    bnez v0, MterpException
    FETCH_ADVANCE_INST(1)                  # advance rPC, load rINST
    GET_INST_OPCODE(t0)                    # extract opcode from rINST
    GOTO_OPCODE(t0)                        # jump to next instruction

%def op_monitor_exit():
    /*
     * Unlock an object.
     *
     * Exceptions that occur when unlocking a monitor need to appear as
     * if they happened at the following instruction.  See the Dalvik
     * instruction spec.
     */
    /* monitor-exit vAA */
    EXPORT_PC()
    GET_OPA(a2)                            # a2 <- AA
    GET_VREG(a0, a2)                       # a0 <- vAA (object)
    move   a1, rSELF                       # a1 <- self
    JAL(artUnlockObjectFromCode)           # v0 <- artUnlockObject(obj, self)
    bnez v0, MterpException
    FETCH_ADVANCE_INST(1)                  # advance rPC, load rINST
    GET_INST_OPCODE(t0)                    # extract opcode from rINST
    GOTO_OPCODE(t0)                        # jump to next instruction

%def op_move(is_object="0"):
    /* for move, move-object, long-to-int */
    /* op vA, vB */
    GET_OPB(a1)                            #  a1 <- B from 15:12
    GET_OPA4(a0)                           #  a0 <- A from 11:8
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    GET_VREG(a2, a1)                       #  a2 <- fp[B]
    GET_INST_OPCODE(t0)                    #  t0 <- opcode from rINST
    .if $is_object
    SET_VREG_OBJECT_GOTO(a2, a0, t0)       #  fp[A] <- a2
    .else
    SET_VREG_GOTO(a2, a0, t0)              #  fp[A] <- a2
    .endif

%def op_move_16(is_object="0"):
    /* for: move/16, move-object/16 */
    /* op vAAAA, vBBBB */
    FETCH(a1, 2)                           #  a1 <- BBBB
    FETCH(a0, 1)                           #  a0 <- AAAA
    FETCH_ADVANCE_INST(3)                  #  advance rPC, load rINST
    GET_VREG(a2, a1)                       #  a2 <- fp[BBBB]
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    .if $is_object
    SET_VREG_OBJECT_GOTO(a2, a0, t0)       #  fp[AAAA] <- a2
    .else
    SET_VREG_GOTO(a2, a0, t0)              #  fp[AAAA] <- a2
    .endif

%def op_move_exception():
    /* move-exception vAA */
    GET_OPA(a2)                                 #  a2 <- AA
    lw    a3, THREAD_EXCEPTION_OFFSET(rSELF)    #  get exception obj
    FETCH_ADVANCE_INST(1)                       #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                         #  extract opcode from rINST
    GET_OPCODE_TARGET(t0)
    SET_VREG_OBJECT(a3, a2)                     #  fp[AA] <- exception obj
    sw    zero, THREAD_EXCEPTION_OFFSET(rSELF)  #  clear exception
    JR(t0)                                      #  jump to next instruction

%def op_move_from16(is_object="0"):
    /* for: move/from16, move-object/from16 */
    /* op vAA, vBBBB */
    FETCH(a1, 1)                           #  a1 <- BBBB
    GET_OPA(a0)                            #  a0 <- AA
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_VREG(a2, a1)                       #  a2 <- fp[BBBB]
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    .if $is_object
    SET_VREG_OBJECT_GOTO(a2, a0, t0)       #  fp[AA] <- a2
    .else
    SET_VREG_GOTO(a2, a0, t0)              #  fp[AA] <- a2
    .endif

%def op_move_object():
%  op_move(is_object="1")

%def op_move_object_16():
%  op_move_16(is_object="1")

%def op_move_object_from16():
%  op_move_from16(is_object="1")

%def op_move_result(is_object="0"):
    /* for: move-result, move-result-object */
    /* op vAA */
    GET_OPA(a2)                            #  a2 <- AA
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    lw    a0, OFF_FP_RESULT_REGISTER(rFP)  #  get pointer to result JType
    lw    a0, 0(a0)                        #  a0 <- result.i
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    .if $is_object
    SET_VREG_OBJECT_GOTO(a0, a2, t0)       #  fp[AA] <- a0
    .else
    SET_VREG_GOTO(a0, a2, t0)              #  fp[AA] <- a0
    .endif

%def op_move_result_object():
%  op_move_result(is_object="1")

%def op_move_result_wide():
    /* move-result-wide vAA */
    GET_OPA(a2)                            #  a2 <- AA
    lw    a3, OFF_FP_RESULT_REGISTER(rFP)  #  get pointer to result JType
    LOAD64(a0, a1, a3)                     #  a0/a1 <- retval.j
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_GOTO(a0, a1, a2, t0)        #  fp[AA] <- a0/a1

%def op_move_wide():
    /* move-wide vA, vB */
    /* NOTE: regs can overlap, e.g. "move v6, v7" or "move v7, v6" */
    GET_OPA4(a2)                           #  a2 <- A(+)
    GET_OPB(a3)                            #  a3 <- B
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[B]
    LOAD64(a0, a1, a3)                     #  a0/a1 <- fp[B]
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_GOTO(a0, a1, a2, t0)        #  fp[A] <- a0/a1

%def op_move_wide_16():
    /* move-wide/16 vAAAA, vBBBB */
    /* NOTE: regs can overlap, e.g. "move v6, v7" or "move v7, v6" */
    FETCH(a3, 2)                           #  a3 <- BBBB
    FETCH(a2, 1)                           #  a2 <- AAAA
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[BBBB]
    LOAD64(a0, a1, a3)                     #  a0/a1 <- fp[BBBB]
    FETCH_ADVANCE_INST(3)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_GOTO(a0, a1, a2, t0)        #  fp[AAAA] <- a0/a1

%def op_move_wide_from16():
    /* move-wide/from16 vAA, vBBBB */
    /* NOTE: regs can overlap, e.g. "move v6, v7" or "move v7, v6" */
    FETCH(a3, 1)                           #  a3 <- BBBB
    GET_OPA(a2)                            #  a2 <- AA
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[BBBB]
    LOAD64(a0, a1, a3)                     #  a0/a1 <- fp[BBBB]
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_GOTO(a0, a1, a2, t0)        #  fp[AA] <- a0/a1

%def op_nop():
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction

%def op_unused_3e():
%  unused()

%def op_unused_3f():
%  unused()

%def op_unused_40():
%  unused()

%def op_unused_41():
%  unused()

%def op_unused_42():
%  unused()

%def op_unused_43():
%  unused()

%def op_unused_73():
%  unused()

%def op_unused_79():
%  unused()

%def op_unused_7a():
%  unused()

%def op_unused_f3():
%  unused()

%def op_unused_f4():
%  unused()

%def op_unused_f5():
%  unused()

%def op_unused_f6():
%  unused()

%def op_unused_f7():
%  unused()

%def op_unused_f8():
%  unused()

%def op_unused_f9():
%  unused()

%def op_unused_fc():
%  unused()

%def op_unused_fd():
%  unused()
