%def const(helper="UndefinedConstHandler"):
    /* const/class vAA, type@BBBB */
    /* const/method-handle vAA, method_handle@BBBB */
    /* const/method-type vAA, proto@BBBB */
    /* const/string vAA, string@@BBBB */
    .extern $helper
    EXPORT_PC
    movzwq  2(rPC), OUT_ARG0                # eax <- OUT_ARG0
    movq    rINSTq, OUT_ARG1
    leaq    OFF_FP_SHADOWFRAME(rFP), OUT_ARG2
    movq    rSELF, OUT_ARG3
    call    SYMBOL($helper)                 # (index, tgt_reg, shadow_frame, self)
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
    SET_VREG %eax, rINSTq                   # vAA<- eax
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 3

%def op_const_16():
    /* const/16 vAA, #+BBBB */
    movswl  2(rPC), %ecx                    # ecx <- ssssBBBB
    SET_VREG %ecx, rINSTq                   # vAA <- ssssBBBB
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def op_const_4():
    /* const/4 vA, #+B */
    movsbl  rINSTbl, %eax                   # eax <-ssssssBx
    movl    $$0xf, rINST
    andl    %eax, rINST                     # rINST <- A
    sarl    $$4, %eax
    SET_VREG %eax, rINSTq
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 1

%def op_const_class():
%  const(helper="MterpConstClass")

%def op_const_high16():
    /* const/high16 vAA, #+BBBB0000 */
    movzwl  2(rPC), %eax                    # eax <- 0000BBBB
    sall    $$16, %eax                      # eax <- BBBB0000
    SET_VREG %eax, rINSTq                   # vAA <- eax
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
    movl    2(rPC), OUT_32_ARG0             # OUT_32_ARG0 <- BBBB
    movq    rINSTq, OUT_ARG1
    leaq    OFF_FP_SHADOWFRAME(rFP), OUT_ARG2
    movq    rSELF, OUT_ARG3
    call    SYMBOL(MterpConstString)        # (index, tgt_reg, shadow_frame, self)
    testb   %al, %al
    jnz     MterpPossibleException
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 3

%def op_const_wide():
    /* const-wide vAA, #+HHHHhhhhBBBBbbbb */
    movq    2(rPC), %rax                    # rax <- HHHHhhhhBBBBbbbb
    SET_WIDE_VREG %rax, rINSTq
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 5

%def op_const_wide_16():
    /* const-wide/16 vAA, #+BBBB */
    movswq  2(rPC), %rax                    # rax <- ssssBBBB
    SET_WIDE_VREG %rax, rINSTq              # store
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def op_const_wide_32():
    /* const-wide/32 vAA, #+BBBBbbbb */
    movslq   2(rPC), %rax                   # eax <- ssssssssBBBBbbbb
    SET_WIDE_VREG %rax, rINSTq              # store
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 3

%def op_const_wide_high16():
    /* const-wide/high16 vAA, #+BBBB000000000000 */
    movzwq  2(rPC), %rax                    # eax <- 0000BBBB
    salq    $$48, %rax                      # eax <- BBBB0000
    SET_WIDE_VREG %rax, rINSTq              # v[AA+0] <- eax
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def op_monitor_enter():
/*
 * Synchronize on an object.
 */
    /* monitor-enter vAA */
    EXPORT_PC
    GET_VREG OUT_32_ARG0, rINSTq
    movq    rSELF, OUT_ARG1
    call    SYMBOL(artLockObjectFromCode)   # (object, self)
    testq   %rax, %rax
    jnz     MterpException
    ADVANCE_PC 1
    movq    rSELF, %rax
    cmpb    LITERAL(0), THREAD_USE_MTERP_OFFSET(%rax)
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
    GET_VREG OUT_32_ARG0, rINSTq
    movq    rSELF, OUT_ARG1
    call    SYMBOL(artUnlockObjectFromCode) # (object, self)
    testq   %rax, %rax
    jnz     MterpException
    ADVANCE_PC 1
    movq    rSELF, %rax
    cmpb    LITERAL(0), THREAD_USE_MTERP_OFFSET(%rax)
    jz      MterpFallback
    FETCH_INST
    GOTO_NEXT

%def op_move(is_object="0"):
    /* for move, move-object, long-to-int */
    /* op vA, vB */
    movl    rINST, %eax                     # eax <- BA
    andb    $$0xf, %al                      # eax <- A
    shrl    $$4, rINST                      # rINST <- B
    GET_VREG %edx, rINSTq
    .if $is_object
    SET_VREG_OBJECT %edx, %rax              # fp[A] <- fp[B]
    .else
    SET_VREG %edx, %rax                     # fp[A] <- fp[B]
    .endif
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 1

%def op_move_16(is_object="0"):
    /* for: move/16, move-object/16 */
    /* op vAAAA, vBBBB */
    movzwq  4(rPC), %rcx                    # ecx <- BBBB
    movzwq  2(rPC), %rax                    # eax <- AAAA
    GET_VREG %edx, %rcx
    .if $is_object
    SET_VREG_OBJECT %edx, %rax              # fp[A] <- fp[B]
    .else
    SET_VREG %edx, %rax                     # fp[A] <- fp[B]
    .endif
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 3

%def op_move_exception():
    /* move-exception vAA */
    movq    rSELF, %rcx
    movl    THREAD_EXCEPTION_OFFSET(%rcx), %eax
    SET_VREG_OBJECT %eax, rINSTq            # fp[AA] <- exception object
    movl    $$0, THREAD_EXCEPTION_OFFSET(%rcx)
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 1

%def op_move_from16(is_object="0"):
    /* for: move/from16, move-object/from16 */
    /* op vAA, vBBBB */
    movzwq  2(rPC), %rax                    # eax <- BBBB
    GET_VREG %edx, %rax                     # edx <- fp[BBBB]
    .if $is_object
    SET_VREG_OBJECT %edx, rINSTq            # fp[A] <- fp[B]
    .else
    SET_VREG %edx, rINSTq                   # fp[A] <- fp[B]
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
    movq    OFF_FP_RESULT_REGISTER(rFP), %rax    # get pointer to result JType.
    movl    (%rax), %eax                    # r0 <- result.i.
    .if $is_object
    SET_VREG_OBJECT %eax, rINSTq            # fp[A] <- fp[B]
    .else
    SET_VREG %eax, rINSTq                   # fp[A] <- fp[B]
    .endif
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 1

%def op_move_result_object():
%  op_move_result(is_object="1")

%def op_move_result_wide():
    /* move-result-wide vAA */
    movq    OFF_FP_RESULT_REGISTER(rFP), %rax    # get pointer to result JType.
    movq    (%rax), %rdx                         # Get wide
    SET_WIDE_VREG %rdx, rINSTq                   # v[AA] <- rdx
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 1

%def op_move_wide():
    /* move-wide vA, vB */
    /* NOTE: regs can overlap, e.g. "move v6,v7" or "move v7,v6" */
    movl    rINST, %ecx                     # ecx <- BA
    sarl    $$4, rINST                      # rINST <- B
    andb    $$0xf, %cl                      # ecx <- A
    GET_WIDE_VREG %rdx, rINSTq              # rdx <- v[B]
    SET_WIDE_VREG %rdx, %rcx                # v[A] <- rdx
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 1

%def op_move_wide_16():
    /* move-wide/16 vAAAA, vBBBB */
    /* NOTE: regs can overlap, e.g. "move v6,v7" or "move v7,v6" */
    movzwq  4(rPC), %rcx                    # ecx<- BBBB
    movzwq  2(rPC), %rax                    # eax<- AAAA
    GET_WIDE_VREG %rdx, %rcx                # rdx <- v[B]
    SET_WIDE_VREG %rdx, %rax                # v[A] <- rdx
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 3

%def op_move_wide_from16():
    /* move-wide/from16 vAA, vBBBB */
    /* NOTE: regs can overlap, e.g. "move v6,v7" or "move v7,v6" */
    movzwl  2(rPC), %ecx                    # ecx <- BBBB
    GET_WIDE_VREG %rdx, %rcx                # rdx <- v[B]
    SET_WIDE_VREG %rdx, rINSTq              # v[A] <- rdx
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
