%def bindiv(result="", second="", wide="", suffix="", rem="0", ext="cdq"):
/*
 * 32-bit binary div/rem operation.  Handles special case of op1=-1.
 */
    /* div/rem vAA, vBB, vCC */
    movzbq  2(rPC), %rax                    # rax <- BB
    movzbq  3(rPC), %rcx                    # rcx <- CC
    .if $wide
    GET_WIDE_VREG %rax, %rax                # eax <- vBB
    GET_WIDE_VREG $second, %rcx             # ecx <- vCC
    .else
    GET_VREG %eax, %rax                     # eax <- vBB
    GET_VREG $second, %rcx                  # ecx <- vCC
    .endif
    test${suffix}   $second, $second
    jz      common_errDivideByZero
    cmp${suffix}  $$-1, $second
    je      2f
    $ext                                    # rdx:rax <- sign-extended of rax
    idiv${suffix}   $second
1:
    .if $wide
    SET_WIDE_VREG $result, rINSTq           # eax <- vBB
    .else
    SET_VREG $result, rINSTq                # eax <- vBB
    .endif
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2
2:
    .if $rem
    xor${suffix} $result, $result
    .else
    neg${suffix} $result
    .endif
    jmp     1b

%def bindiv2addr(result="", second="", wide="", suffix="", rem="0", ext="cdq"):
/*
 * 32-bit binary div/rem operation.  Handles special case of op1=-1.
 */
    /* div/rem/2addr vA, vB */
    movl    rINST, %ecx                     # rcx <- BA
    sarl    $$4, %ecx                       # rcx <- B
    andb    $$0xf, rINSTbl                  # rINST <- A
    .if $wide
    GET_WIDE_VREG %rax, rINSTq              # eax <- vA
    GET_WIDE_VREG $second, %rcx             # ecx <- vB
    .else
    GET_VREG %eax, rINSTq                   # eax <- vA
    GET_VREG $second, %rcx                  # ecx <- vB
    .endif
    test${suffix}   $second, $second
    jz      common_errDivideByZero
    cmp${suffix}  $$-1, $second
    je      2f
    $ext                                    # rdx:rax <- sign-extended of rax
    idiv${suffix}   $second
1:
    .if $wide
    SET_WIDE_VREG $result, rINSTq           # vA <- result
    .else
    SET_VREG $result, rINSTq                # vA <- result
    .endif
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 1
2:
    .if $rem
    xor${suffix} $result, $result
    .else
    neg${suffix} $result
    .endif
    jmp     1b

%def bindivLit16(result="", rem="0"):
/*
 * 32-bit binary div/rem operation.  Handles special case of op1=-1.
 */
    /* div/rem/lit16 vA, vB, #+CCCC */
    /* Need A in rINST, ssssCCCC in ecx, vB in eax */
    movl    rINST, %eax                     # rax <- 000000BA
    sarl    $$4, %eax                       # eax <- B
    GET_VREG %eax, %rax                     # eax <- vB
    movswl  2(rPC), %ecx                    # ecx <- ssssCCCC
    andb    $$0xf, rINSTbl                  # rINST <- A
    testl   %ecx, %ecx
    jz      common_errDivideByZero
    cmpl    $$-1, %ecx
    je      2f
    cdq                                     # rax <- sign-extended of eax
    idivl   %ecx
1:
    SET_VREG $result, rINSTq                # vA <- result
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2
2:
    .if $rem
    xorl    $result, $result
    .else
    negl    $result
    .endif
    jmp     1b

%def bindivLit8(result="", rem="0"):
/*
 * 32-bit div/rem "lit8" binary operation.  Handles special case of
 * op0=minint & op1=-1
 */
    /* div/rem/lit8 vAA, vBB, #+CC */
    movzbq  2(rPC), %rax                    # eax <- BB
    movsbl  3(rPC), %ecx                    # ecx <- ssssssCC
    GET_VREG  %eax, %rax                    # eax <- rBB
    testl   %ecx, %ecx
    je      common_errDivideByZero
    cmpl    $$-1, %ecx
    je      2f
    cdq                                     # rax <- sign-extended of eax
    idivl   %ecx
1:
    SET_VREG $result, rINSTq                # vA <- result
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2
2:
    .if $rem
    xorl    $result, $result
    .else
    negl    $result
    .endif
    jmp     1b

%def binop(result="%eax", instr=""):
/*
 * Generic 32-bit binary operation.  Provide an "instr" line that
 * specifies an instruction that performs "result = eax op (rFP,%ecx,4)".
 * This could be an x86 instruction or a function call.  (If the result
 * comes back in a register other than eax, you can override "result".)
 *
 * For: add-int, sub-int, and-int, or-int,
 *      xor-int, shl-int, shr-int, ushr-int
 */
    /* binop vAA, vBB, vCC */
    movzbq  2(rPC), %rax                    # rax <- BB
    movzbq  3(rPC), %rcx                    # rcx <- CC
    GET_VREG %eax, %rax                     # eax <- vBB
    $instr                                  # ex: addl    (rFP,%rcx,4),%eax
    SET_VREG $result, rINSTq
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def binop1(wide="0", instr=""):
/*
 * Generic 32-bit binary operation in which both operands loaded to
 * registers (op0 in eax, op1 in ecx).
 */
    /* binop vAA, vBB, vCC */
    movzbq  2(rPC), %rax                    # eax <- BB
    movzbq  3(rPC), %rcx                    # ecx <- CC
    GET_VREG %ecx, %rcx                     # eax <- vCC
    .if $wide
    GET_WIDE_VREG %rax, %rax                # rax <- vBB
    $instr                                  # ex: addl    %ecx,%eax
    SET_WIDE_VREG %rax, rINSTq
    .else
    GET_VREG %eax, %rax                     # eax <- vBB
    $instr                                  # ex: addl    %ecx,%eax
    SET_VREG %eax, rINSTq
    .endif
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def binop2addr(result="%eax", instr=""):
/*
 * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
 * that specifies an instruction that performs "result = r0 op r1".
 * This could be an instruction or a function call.
 *
 * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
 *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
 *      shl-int/2addr, shr-int/2addr, ushr-int/2addr, add-float/2addr,
 *      sub-float/2addr, mul-float/2addr, div-float/2addr, rem-float/2addr
 */
    /* binop/2addr vA, vB */
    movl    rINST, %ecx                     # rcx <- A+
    sarl    $$4, rINST                      # rINST <- B
    andb    $$0xf, %cl                      # ecx <- A
    GET_VREG %eax, rINSTq                   # eax <- vB
    $instr                                  # for ex: addl   %eax,(rFP,%ecx,4)
    CLEAR_REF %rcx
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 1

%def binopLit16(result="%eax", instr=""):
/*
 * Generic 32-bit "lit16" binary operation.  Provide an "instr" line
 * that specifies an instruction that performs "result = eax op ecx".
 * This could be an x86 instruction or a function call.  (If the result
 * comes back in a register other than eax, you can override "result".)
 *
 * For: add-int/lit16, rsub-int,
 *      and-int/lit16, or-int/lit16, xor-int/lit16
 */
    /* binop/lit16 vA, vB, #+CCCC */
    movl    rINST, %eax                     # rax <- 000000BA
    sarl    $$4, %eax                       # eax <- B
    GET_VREG %eax, %rax                     # eax <- vB
    andb    $$0xf, rINSTbl                  # rINST <- A
    movswl  2(rPC), %ecx                    # ecx <- ssssCCCC
    $instr                                  # for example: addl %ecx, %eax
    SET_VREG $result, rINSTq
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def binopLit8(result="%eax", instr=""):
/*
 * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
 * that specifies an instruction that performs "result = eax op ecx".
 * This could be an x86 instruction or a function call.  (If the result
 * comes back in a register other than r0, you can override "result".)
 *
 * For: add-int/lit8, rsub-int/lit8
 *      and-int/lit8, or-int/lit8, xor-int/lit8,
 *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
 */
    /* binop/lit8 vAA, vBB, #+CC */
    movzbq  2(rPC), %rax                    # rax <- BB
    movsbl  3(rPC), %ecx                    # rcx <- ssssssCC
    GET_VREG %eax, %rax                     # eax <- rBB
    $instr                                  # ex: addl %ecx,%eax
    SET_VREG $result, rINSTq
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def binopWide(instr=""):
/*
 * Generic 64-bit binary operation.
 */
    /* binop vAA, vBB, vCC */
    movzbq  2(rPC), %rax                    # eax <- BB
    movzbq  3(rPC), %rcx                    # ecx <- CC
    GET_WIDE_VREG %rax, %rax                # rax <- v[BB]
    $instr                                  # ex: addq   (rFP,%rcx,4),%rax
    SET_WIDE_VREG %rax, rINSTq              # v[AA] <- rax
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def binopWide2addr(instr=""):
/*
 * Generic 64-bit binary operation.
 */
    /* binop/2addr vA, vB */
    movl    rINST, %ecx                     # rcx <- A+
    sarl    $$4, rINST                      # rINST <- B
    andb    $$0xf, %cl                      # ecx <- A
    GET_WIDE_VREG %rax, rINSTq              # rax <- vB
    $instr                                  # for ex: addq   %rax,(rFP,%rcx,4)
    CLEAR_WIDE_REF %rcx
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 1

%def cvtfp_int(fp_suffix="", i_suffix="", max_const="", result_reg="", wide=""):
/* On fp to int conversions, Java requires that
 * if the result > maxint, it should be clamped to maxint.  If it is less
 * than minint, it should be clamped to minint.  If it is a nan, the result
 * should be zero.  Further, the rounding mode is to truncate.
 */
    /* float/double to int/long vA, vB */
    movl    rINST, %ecx                     # rcx <- A+
    sarl    $$4, rINST                      # rINST <- B
    andb    $$0xf, %cl                      # ecx <- A
    movs${fp_suffix}   VREG_ADDRESS(rINSTq), %xmm0
    mov${i_suffix}  ${max_const}, ${result_reg}
    cvtsi2s${fp_suffix}${i_suffix} ${result_reg}, %xmm1
    comis${fp_suffix}    %xmm1, %xmm0
    jae     1f
    jp      2f
    cvtts${fp_suffix}2si${i_suffix}  %xmm0, ${result_reg}
    jmp     1f
2:
    xor${i_suffix}    ${result_reg}, ${result_reg}
1:
    .if $wide
    SET_WIDE_VREG ${result_reg}, %rcx
    .else
    SET_VREG ${result_reg}, %rcx
    .endif
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 1

%def shop2addr(wide="0", instr=""):
/*
 * Generic 32-bit "shift/2addr" operation.
 */
    /* shift/2addr vA, vB */
    movl    rINST, %ecx                     # ecx <- BA
    sarl    $$4, %ecx                       # ecx <- B
    GET_VREG %ecx, %rcx                     # ecx <- vBB
    andb    $$0xf, rINSTbl                  # rINST <- A
    .if $wide
    GET_WIDE_VREG %rax, rINSTq              # rax <- vAA
    $instr                                  # ex: sarl %cl, %eax
    SET_WIDE_VREG %rax, rINSTq
    .else
    GET_VREG %eax, rINSTq                   # eax <- vAA
    $instr                                  # ex: sarl %cl, %eax
    SET_VREG %eax, rINSTq
    .endif
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 1

%def unop(preinstr="", instr="", wide="0"):
/*
 * Generic 32/64-bit unary operation.  Provide an "instr" line that
 * specifies an instruction that performs "result = op eax".
 */
    /* unop vA, vB */
    movl    rINST, %ecx                     # rcx <- A+
    sarl    $$4,rINST                       # rINST <- B
    .if ${wide}
    GET_WIDE_VREG %rax, rINSTq              # rax <- vB
    .else
    GET_VREG %eax, rINSTq                   # eax <- vB
    .endif
    andb    $$0xf,%cl                       # ecx <- A
$preinstr
$instr
    .if ${wide}
    SET_WIDE_VREG %rax, %rcx
    .else
    SET_VREG %eax, %rcx
    .endif
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 1

%def op_add_int():
%  binop(instr="addl    (rFP,%rcx,4), %eax")

%def op_add_int_2addr():
%  binop2addr(instr="addl    %eax, (rFP,%rcx,4)")

%def op_add_int_lit16():
%  binopLit16(instr="addl    %ecx, %eax")

%def op_add_int_lit8():
%  binopLit8(instr="addl    %ecx, %eax")

%def op_add_long():
%  binopWide(instr="addq    (rFP,%rcx,4), %rax")

%def op_add_long_2addr():
%  binopWide2addr(instr="addq    %rax, (rFP,%rcx,4)")

%def op_and_int():
%  binop(instr="andl    (rFP,%rcx,4), %eax")

%def op_and_int_2addr():
%  binop2addr(instr="andl    %eax, (rFP,%rcx,4)")

%def op_and_int_lit16():
%  binopLit16(instr="andl    %ecx, %eax")

%def op_and_int_lit8():
%  binopLit8(instr="andl    %ecx, %eax")

%def op_and_long():
%  binopWide(instr="andq    (rFP,%rcx,4), %rax")

%def op_and_long_2addr():
%  binopWide2addr(instr="andq    %rax, (rFP,%rcx,4)")

%def op_cmp_long():
/*
 * Compare two 64-bit values.  Puts 0, 1, or -1 into the destination
 * register based on the results of the comparison.
 */
    /* cmp-long vAA, vBB, vCC */
    movzbq  2(rPC), %rdx                    # edx <- BB
    movzbq  3(rPC), %rcx                    # ecx <- CC
    GET_WIDE_VREG %rdx, %rdx                # rdx <- v[BB]
    xorl    %eax, %eax
    xorl    %edi, %edi
    addb    $$1, %al
    movl    $$-1, %esi
    cmpq    VREG_ADDRESS(%rcx), %rdx
    cmovl   %esi, %edi
    cmovg   %eax, %edi
    SET_VREG %edi, rINSTq
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def op_div_int():
%  bindiv(result="%eax", second="%ecx", wide="0", suffix="l")

%def op_div_int_2addr():
%  bindiv2addr(result="%eax", second="%ecx", wide="0", suffix="l")

%def op_div_int_lit16():
%  bindivLit16(result="%eax")

%def op_div_int_lit8():
%  bindivLit8(result="%eax")

%def op_div_long():
%  bindiv(result="%rax", second="%rcx", wide="1", suffix="q", ext="cqo")

%def op_div_long_2addr():
%  bindiv2addr(result="%rax", second="%rcx", wide="1", suffix="q", ext="cqo")

%def op_int_to_byte():
%  unop(instr="movsbl  %al, %eax")

%def op_int_to_char():
%  unop(instr="movzwl  %ax,%eax")

%def op_int_to_long():
    /* int to long vA, vB */
    movzbq  rINSTbl, %rax                   # rax <- +A
    sarl    $$4, %eax                       # eax <- B
    andb    $$0xf, rINSTbl                  # rINST <- A
    movslq  VREG_ADDRESS(%rax), %rax
    SET_WIDE_VREG %rax, rINSTq              # v[A] <- %rax
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 1


%def op_int_to_short():
%  unop(instr="movswl %ax, %eax")

%def op_long_to_int():
/* we ignore the high word, making this equivalent to a 32-bit reg move */
%  op_move()

%def op_mul_int():
%  binop(instr="imull   (rFP,%rcx,4), %eax")

%def op_mul_int_2addr():
    /* mul vA, vB */
    movl    rINST, %ecx                     # rcx <- A+
    sarl    $$4, rINST                      # rINST <- B
    andb    $$0xf, %cl                      # ecx <- A
    GET_VREG %eax, %rcx                     # eax <- vA
    imull   (rFP,rINSTq,4), %eax
    SET_VREG %eax, %rcx
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 1

%def op_mul_int_lit16():
%  binopLit16(instr="imull   %ecx, %eax")

%def op_mul_int_lit8():
%  binopLit8(instr="imull   %ecx, %eax")

%def op_mul_long():
%  binopWide(instr="imulq   (rFP,%rcx,4), %rax")

%def op_mul_long_2addr():
    /* mul vA, vB */
    movl    rINST, %ecx                     # rcx <- A+
    sarl    $$4, rINST                      # rINST <- B
    andb    $$0xf, %cl                      # ecx <- A
    GET_WIDE_VREG %rax, %rcx                # rax <- vA
    imulq   (rFP,rINSTq,4), %rax
    SET_WIDE_VREG %rax, %rcx
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 1

%def op_neg_int():
%  unop(instr="    negl    %eax")

%def op_neg_long():
%  unop(instr="    negq    %rax", wide="1")

%def op_not_int():
%  unop(instr="    notl    %eax")

%def op_not_long():
%  unop(instr="    notq    %rax", wide="1")

%def op_or_int():
%  binop(instr="orl     (rFP,%rcx,4), %eax")

%def op_or_int_2addr():
%  binop2addr(instr="orl     %eax, (rFP,%rcx,4)")

%def op_or_int_lit16():
%  binopLit16(instr="orl     %ecx, %eax")

%def op_or_int_lit8():
%  binopLit8(instr="orl     %ecx, %eax")

%def op_or_long():
%  binopWide(instr="orq     (rFP,%rcx,4), %rax")

%def op_or_long_2addr():
%  binopWide2addr(instr="orq     %rax, (rFP,%rcx,4)")

%def op_rem_int():
%  bindiv(result="%edx", second="%ecx", wide="0", suffix="l", rem="1")

%def op_rem_int_2addr():
%  bindiv2addr(result="%edx", second="%ecx", wide="0", suffix="l", rem="1")

%def op_rem_int_lit16():
%  bindivLit16(result="%edx", rem="1")

%def op_rem_int_lit8():
%  bindivLit8(result="%edx", rem="1")

%def op_rem_long():
%  bindiv(result="%rdx", second="%rcx", wide="1", suffix="q", ext="cqo", rem="1")

%def op_rem_long_2addr():
%  bindiv2addr(result="%rdx", second="%rcx", wide="1", suffix="q", rem="1", ext="cqo")

%def op_rsub_int():
/* this op is "rsub-int", but can be thought of as "rsub-int/lit16" */
%  binopLit16(instr="subl    %eax, %ecx", result="%ecx")

%def op_rsub_int_lit8():
%  binopLit8(instr="subl    %eax, %ecx", result="%ecx")

%def op_shl_int():
%  binop1(instr="sall    %cl, %eax")

%def op_shl_int_2addr():
%  shop2addr(instr="sall    %cl, %eax")

%def op_shl_int_lit8():
%  binopLit8(instr="sall    %cl, %eax")

%def op_shl_long():
%  binop1(instr="salq    %cl, %rax", wide="1")

%def op_shl_long_2addr():
%  shop2addr(instr="salq    %cl, %rax", wide="1")

%def op_shr_int():
%  binop1(instr="sarl    %cl, %eax")

%def op_shr_int_2addr():
%  shop2addr(instr="sarl    %cl, %eax")

%def op_shr_int_lit8():
%  binopLit8(instr="sarl    %cl, %eax")

%def op_shr_long():
%  binop1(instr="sarq    %cl, %rax", wide="1")

%def op_shr_long_2addr():
%  shop2addr(instr="sarq    %cl, %rax", wide="1")

%def op_sub_int():
%  binop(instr="subl    (rFP,%rcx,4), %eax")

%def op_sub_int_2addr():
%  binop2addr(instr="subl    %eax, (rFP,%rcx,4)")

%def op_sub_long():
%  binopWide(instr="subq    (rFP,%rcx,4), %rax")

%def op_sub_long_2addr():
%  binopWide2addr(instr="subq    %rax, (rFP,%rcx,4)")

%def op_ushr_int():
%  binop1(instr="shrl    %cl, %eax")

%def op_ushr_int_2addr():
%  shop2addr(instr="shrl    %cl, %eax")

%def op_ushr_int_lit8():
%  binopLit8(instr="shrl    %cl, %eax")

%def op_ushr_long():
%  binop1(instr="shrq    %cl, %rax", wide="1")

%def op_ushr_long_2addr():
%  shop2addr(instr="shrq    %cl, %rax", wide="1")

%def op_xor_int():
%  binop(instr="xorl    (rFP,%rcx,4), %eax")

%def op_xor_int_2addr():
%  binop2addr(instr="xorl    %eax, (rFP,%rcx,4)")

%def op_xor_int_lit16():
%  binopLit16(instr="xorl    %ecx, %eax")

%def op_xor_int_lit8():
%  binopLit8(instr="xorl    %ecx, %eax")

%def op_xor_long():
%  binopWide(instr="xorq    (rFP,%rcx,4), %rax")

%def op_xor_long_2addr():
%  binopWide2addr(instr="xorq    %rax, (rFP,%rcx,4)")
