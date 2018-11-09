%def const(helper="UndefinedConstHandler"):
    /* const/class vAA, type@BBBB */
    /* const/method-handle vAA, method_handle@BBBB */
    /* const/method-type vAA, proto@BBBB */
    /* const/string vAA, string@@BBBB */
    .extern $helper
    EXPORT_PC
    movzwl  2(rPC), %eax                    # eax <- BBBB
    movl    %eax, OUT_ARG0(%esp)
    movl    rINST, OUT_ARG1(%esp)
    leal    OFF_FP_SHADOWFRAME(rFP), %eax
    movl    %eax, OUT_ARG2(%esp)
    movl    rSELF, %eax
    movl    %eax, OUT_ARG3(%esp)
    call    SYMBOL($helper)                 # (index, tgt_reg, shadow_frame, self)
    RESTORE_IBASE
    testb   %al, %al
    jnz     MterpPossibleException
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def unused():
/*
 * Bail to reference interpreter to throw.
 */
    jmp     MterpFallback

%def op_const():
    /* const vAA, #+BBBBbbbb */
    movl    2(rPC), %eax                    # grab all 32 bits at once
    SET_VREG %eax, rINST                    # vAA<- eax
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 3

%def op_const_16():
    /* const/16 vAA, #+BBBB */
    movswl  2(rPC), %ecx                    # ecx <- ssssBBBB
    SET_VREG %ecx, rINST                    # vAA <- ssssBBBB
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def op_const_4():
    /* const/4 vA, #+B */
    movsx   rINSTbl, %eax                   # eax <-ssssssBx
    movl    $$0xf, rINST
    andl    %eax, rINST                     # rINST <- A
    sarl    $$4, %eax
    SET_VREG %eax, rINST
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 1

%def op_const_class():
%  const(helper="MterpConstClass")

%def op_const_high16():
    /* const/high16 vAA, #+BBBB0000 */
    movzwl  2(rPC), %eax                    # eax <- 0000BBBB
    sall    $$16, %eax                      # eax <- BBBB0000
    SET_VREG %eax, rINST                    # vAA <- eax
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def op_const_method_handle():
%  const(helper="MterpConstMethodHandle")

%def op_const_method_type():
%  const(helper="MterpConstMethodType")

%def op_const_string():
%  const(helper="MterpConstString")

%def op_const_string_jumbo():
    /* const/string vAA, String@BBBBBBBB */
    EXPORT_PC
    movl    2(rPC), %eax                    # eax <- BBBB
    movl    %eax, OUT_ARG0(%esp)
    movl    rINST, OUT_ARG1(%esp)
    leal    OFF_FP_SHADOWFRAME(rFP), %eax
    movl    %eax, OUT_ARG2(%esp)
    movl    rSELF, %eax
    movl    %eax, OUT_ARG3(%esp)
    call    SYMBOL(MterpConstString)        # (index, tgt_reg, shadow_frame, self)
    RESTORE_IBASE
    testb   %al, %al
    jnz     MterpPossibleException
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 3

%def op_const_wide():
    /* const-wide vAA, #+HHHHhhhhBBBBbbbb */
    movl    2(rPC), %eax                    # eax <- lsw
    movzbl  rINSTbl, %ecx                   # ecx <- AA
    movl    6(rPC), rINST                   # rINST <- msw
    SET_VREG %eax, %ecx
    SET_VREG_HIGH  rINST, %ecx
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 5

%def op_const_wide_16():
    /* const-wide/16 vAA, #+BBBB */
    movswl  2(rPC), %eax                    # eax <- ssssBBBB
    movl    rIBASE, %ecx                    # preserve rIBASE (cltd trashes it)
    cltd                                    # rIBASE:eax <- ssssssssssssBBBB
    SET_VREG_HIGH rIBASE, rINST             # store msw
    SET_VREG %eax, rINST                    # store lsw
    movl    %ecx, rIBASE                    # restore rIBASE
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def op_const_wide_32():
    /* const-wide/32 vAA, #+BBBBbbbb */
    movl    2(rPC), %eax                    # eax <- BBBBbbbb
    movl    rIBASE, %ecx                    # preserve rIBASE (cltd trashes it)
    cltd                                    # rIBASE:eax <- ssssssssssssBBBB
    SET_VREG_HIGH rIBASE, rINST             # store msw
    SET_VREG %eax, rINST                    # store lsw
    movl    %ecx, rIBASE                    # restore rIBASE
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 3

%def op_const_wide_high16():
    /* const-wide/high16 vAA, #+BBBB000000000000 */
    movzwl  2(rPC), %eax                    # eax <- 0000BBBB
    sall    $$16, %eax                      # eax <- BBBB0000
    SET_VREG_HIGH %eax, rINST               # v[AA+1] <- eax
    xorl    %eax, %eax
    SET_VREG %eax, rINST                    # v[AA+0] <- eax
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def op_monitor_enter():
/*
 * Synchronize on an object.
 */
    /* monitor-enter vAA */
    EXPORT_PC
    GET_VREG %ecx, rINST
    movl    %ecx, OUT_ARG0(%esp)
    movl    rSELF, %eax
    movl    %eax, OUT_ARG1(%esp)
    call    SYMBOL(artLockObjectFromCode)   # (object, self)
    RESTORE_IBASE
    testb   %al, %al
    jnz     MterpException
    ADVANCE_PC 1
    movl    rSELF, %eax
    cmpb    LITERAL(0), THREAD_USE_MTERP_OFFSET(%eax)
    jz      MterpFallback
    FETCH_INST
    GOTO_NEXT

%def op_monitor_exit():
/*
 * Unlock an object.
 *
 * Exceptions that occur when unlocking a monitor need to appear as
 * if they happened at the following instruction.  See the Dalvik
 * instruction spec.
 */
    /* monitor-exit vAA */
    EXPORT_PC
    GET_VREG %ecx, rINST
    movl    %ecx, OUT_ARG0(%esp)
    movl    rSELF, %eax
    movl    %eax, OUT_ARG1(%esp)
    call    SYMBOL(artUnlockObjectFromCode) # (object, self)
    RESTORE_IBASE
    testb   %al, %al
    jnz     MterpException
    ADVANCE_PC 1
    movl    rSELF, %eax
    cmpb    LITERAL(0), THREAD_USE_MTERP_OFFSET(%eax)
    jz      MterpFallback
    FETCH_INST
    GOTO_NEXT

%def op_move(is_object="0"):
    /* for move, move-object, long-to-int */
    /* op vA, vB */
    movzbl  rINSTbl, %eax                   # eax <- BA
    andb    $$0xf, %al                      # eax <- A
    shrl    $$4, rINST                      # rINST <- B
    GET_VREG rINST, rINST
    .if $is_object
    SET_VREG_OBJECT rINST, %eax             # fp[A] <- fp[B]
    .else
    SET_VREG rINST, %eax                    # fp[A] <- fp[B]
    .endif
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 1

%def op_move_16(is_object="0"):
    /* for: move/16, move-object/16 */
    /* op vAAAA, vBBBB */
    movzwl  4(rPC), %ecx                    # ecx <- BBBB
    movzwl  2(rPC), %eax                    # eax <- AAAA
    GET_VREG rINST, %ecx
    .if $is_object
    SET_VREG_OBJECT rINST, %eax             # fp[A] <- fp[B]
    .else
    SET_VREG rINST, %eax                    # fp[A] <- fp[B]
    .endif
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 3

%def op_move_exception():
    /* move-exception vAA */
    movl    rSELF, %ecx
    movl    THREAD_EXCEPTION_OFFSET(%ecx), %eax
    SET_VREG_OBJECT %eax, rINST             # fp[AA] <- exception object
    movl    $$0, THREAD_EXCEPTION_OFFSET(%ecx)
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 1

%def op_move_from16(is_object="0"):
    /* for: move/from16, move-object/from16 */
    /* op vAA, vBBBB */
    movzx   rINSTbl, %eax                   # eax <- AA
    movw    2(rPC), rINSTw                  # rINSTw <- BBBB
    GET_VREG rINST, rINST                   # rINST <- fp[BBBB]
    .if $is_object
    SET_VREG_OBJECT rINST, %eax             # fp[A] <- fp[B]
    .else
    SET_VREG rINST, %eax                    # fp[A] <- fp[B]
    .endif
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def op_move_object():
%  op_move(is_object="1")

%def op_move_object_16():
%  op_move_16(is_object="1")

%def op_move_object_from16():
%  op_move_from16(is_object="1")

%def op_move_result(is_object="0"):
    /* for: move-result, move-result-object */
    /* op vAA */
    movl    OFF_FP_RESULT_REGISTER(rFP), %eax    # get pointer to result JType.
    movl    (%eax), %eax                    # r0 <- result.i.
    .if $is_object
    SET_VREG_OBJECT %eax, rINST             # fp[A] <- fp[B]
    .else
    SET_VREG %eax, rINST                    # fp[A] <- fp[B]
    .endif
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 1

%def op_move_result_object():
%  op_move_result(is_object="1")

%def op_move_result_wide():
    /* move-result-wide vAA */
    movl    OFF_FP_RESULT_REGISTER(rFP), %eax    # get pointer to result JType.
    movl    4(%eax), %ecx                   # Get high
    movl    (%eax), %eax                    # Get low
    SET_VREG %eax, rINST                    # v[AA+0] <- eax
    SET_VREG_HIGH %ecx, rINST               # v[AA+1] <- ecx
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 1

%def op_move_wide():
    /* move-wide vA, vB */
    /* NOTE: regs can overlap, e.g. "move v6,v7" or "move v7,v6" */
    movzbl  rINSTbl, %ecx                   # ecx <- BA
    sarl    $$4, rINST                      # rINST <- B
    andb    $$0xf, %cl                      # ecx <- A
    GET_WIDE_FP_VREG %xmm0, rINST           # xmm0 <- v[B]
    SET_WIDE_FP_VREG %xmm0, %ecx            # v[A] <- xmm0
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 1

%def op_move_wide_16():
    /* move-wide/16 vAAAA, vBBBB */
    /* NOTE: regs can overlap, e.g. "move v6,v7" or "move v7,v6" */
    movzwl  4(rPC), %ecx                    # ecx<- BBBB
    movzwl  2(rPC), %eax                    # eax<- AAAA
    GET_WIDE_FP_VREG %xmm0, %ecx            # xmm0 <- v[B]
    SET_WIDE_FP_VREG %xmm0, %eax            # v[A] <- xmm0
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 3

%def op_move_wide_from16():
    /* move-wide/from16 vAA, vBBBB */
    /* NOTE: regs can overlap, e.g. "move v6,v7" or "move v7,v6" */
    movzwl  2(rPC), %ecx                    # ecx <- BBBB
    movzbl  rINSTbl, %eax                   # eax <- AAAA
    GET_WIDE_FP_VREG %xmm0, %ecx            # xmm0 <- v[B]
    SET_WIDE_FP_VREG %xmm0, %eax            # v[A] <- xmm0
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def op_nop():
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 1

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
