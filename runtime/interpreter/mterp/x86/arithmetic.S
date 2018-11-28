%def bindiv(result="", special="", rem=""):
/*
 * 32-bit binary div/rem operation.  Handles special case of op0=minint and
 * op1=-1.
 */
    /* div/rem vAA, vBB, vCC */
    movzbl  2(rPC), %eax                    # eax <- BB
    movzbl  3(rPC), %ecx                    # ecx <- CC
    GET_VREG %eax, %eax                     # eax <- vBB
    GET_VREG %ecx, %ecx                     # ecx <- vCC
    mov     rIBASE, LOCAL0(%esp)
    testl   %ecx, %ecx
    je      common_errDivideByZero
    movl    %eax, %edx
    orl     %ecx, %edx
    testl   $$0xFFFFFF00, %edx              # If both arguments are less
                                            #   than 8-bit and +ve
    jz      .L${opcode}_8                   # Do 8-bit divide
    testl   $$0xFFFF0000, %edx              # If both arguments are less
                                            #   than 16-bit and +ve
    jz      .L${opcode}_16                  # Do 16-bit divide
    cmpl    $$-1, %ecx
    jne     .L${opcode}_32
    cmpl    $$0x80000000, %eax
    jne     .L${opcode}_32
    movl    $special, $result
    jmp     .L${opcode}_finish
%  add_helper(lambda: bindiv_helper(result, rem))

%def bindiv_helper(result, rem):
.L${opcode}_32:
    cltd
    idivl   %ecx
    jmp     .L${opcode}_finish
.L${opcode}_8:
    div     %cl                             # 8-bit divide otherwise.
                                            # Remainder in %ah, quotient in %al
    .if $rem
    movl    %eax, %edx
    shr     $$8, %edx
    .else
    andl    $$0x000000FF, %eax
    .endif
    jmp     .L${opcode}_finish
.L${opcode}_16:
    xorl    %edx, %edx                      # Clear %edx before divide
    div     %cx
.L${opcode}_finish:
    SET_VREG $result, rINST
    mov     LOCAL0(%esp), rIBASE
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def bindiv2addr(result="", special=""):
/*
 * 32-bit binary div/rem operation.  Handles special case of op0=minint and
 * op1=-1.
 */
    /* div/rem/2addr vA, vB */
    movzx   rINSTbl, %ecx                   # eax <- BA
    mov     rIBASE, LOCAL0(%esp)
    sarl    $$4, %ecx                       # ecx <- B
    GET_VREG %ecx, %ecx                     # eax <- vBB
    andb    $$0xf, rINSTbl                  # rINST <- A
    GET_VREG %eax, rINST                    # eax <- vBB
    testl   %ecx, %ecx
    je      common_errDivideByZero
    cmpl    $$-1, %ecx
    jne     .L${opcode}_continue_div2addr
    cmpl    $$0x80000000, %eax
    jne     .L${opcode}_continue_div2addr
    movl    $special, $result
    SET_VREG $result, rINST
    mov     LOCAL0(%esp), rIBASE
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 1
%  add_helper(lambda: bindiv2addr_helper(result))

%def bindiv2addr_helper(result):
.L${opcode}_continue_div2addr:
    cltd
    idivl   %ecx
    SET_VREG $result, rINST
    mov     LOCAL0(%esp), rIBASE
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 1

%def bindivLit16(result="", special=""):
/*
 * 32-bit binary div/rem operation.  Handles special case of op0=minint and
 * op1=-1.
 */
    /* div/rem/lit16 vA, vB, #+CCCC */
    /* Need A in rINST, ssssCCCC in ecx, vB in eax */
    movzbl  rINSTbl, %eax                   # eax <- 000000BA
    sarl    $$4, %eax                       # eax <- B
    GET_VREG %eax, %eax                     # eax <- vB
    movswl  2(rPC), %ecx                    # ecx <- ssssCCCC
    andb    $$0xf, rINSTbl                  # rINST <- A
    testl   %ecx, %ecx
    je      common_errDivideByZero
    cmpl    $$-1, %ecx
    jne     .L${opcode}_continue_div
    cmpl    $$0x80000000, %eax
    jne     .L${opcode}_continue_div
    movl    $special, %eax
    SET_VREG %eax, rINST
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

.L${opcode}_continue_div:
    mov     rIBASE, LOCAL0(%esp)
    cltd
    idivl   %ecx
    SET_VREG $result, rINST
    mov     LOCAL0(%esp), rIBASE
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def bindivLit8(result="", special=""):
/*
 * 32-bit div/rem "lit8" binary operation.  Handles special case of
 * op0=minint & op1=-1
 */
    /* div/rem/lit8 vAA, vBB, #+CC */
    movzbl  2(rPC), %eax                    # eax <- BB
    movsbl  3(rPC), %ecx                    # ecx <- ssssssCC
    GET_VREG  %eax, %eax                    # eax <- rBB
    testl   %ecx, %ecx
    je      common_errDivideByZero
    cmpl    $$0x80000000, %eax
    jne     .L${opcode}_continue_div
    cmpl    $$-1, %ecx
    jne     .L${opcode}_continue_div
    movl    $special, %eax
    SET_VREG %eax, rINST
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

.L${opcode}_continue_div:
    mov     rIBASE, LOCAL0(%esp)
    cltd
    idivl   %ecx
    SET_VREG $result, rINST
    mov     LOCAL0(%esp), rIBASE
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def binop(result="%eax", instr=""):
/*
 * Generic 32-bit binary operation.  Provide an "instr" line that
 * specifies an instruction that performs "result = eax op VREG_ADDRESS(%ecx)".
 * This could be an x86 instruction or a function call.  (If the result
 * comes back in a register other than eax, you can override "result".)
 *
 * For: add-int, sub-int, and-int, or-int,
 *      xor-int, shl-int, shr-int, ushr-int
 */
    /* binop vAA, vBB, vCC */
    movzbl  2(rPC), %eax                    # eax <- BB
    movzbl  3(rPC), %ecx                    # ecx <- CC
    GET_VREG %eax, %eax                     # eax <- vBB
    $instr VREG_ADDRESS(%ecx), %eax
    SET_VREG $result, rINST
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def binop1(result="%eax", tmp="%ecx", instr=""):
/*
 * Generic 32-bit binary operation in which both operands loaded to
 * registers (op0 in eax, op1 in ecx).
 */
    /* binop vAA, vBB, vCC */
    movzbl  2(rPC),%eax                     # eax <- BB
    movzbl  3(rPC),%ecx                     # ecx <- CC
    GET_VREG %eax, %eax                     # eax <- vBB
    GET_VREG %ecx, %ecx                     # eax <- vBB
    $instr                                  # ex: addl    %ecx,%eax
    SET_VREG $result, rINST
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
    movzx   rINSTbl, %ecx                   # ecx <- A+
    sarl    $$4, rINST                      # rINST <- B
    GET_VREG %eax, rINST                    # eax <- vB
    andb    $$0xf, %cl                      # ecx <- A
    $instr %eax, VREG_ADDRESS(%ecx)
    CLEAR_REF %ecx
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
    movzbl  rINSTbl, %eax                   # eax <- 000000BA
    sarl    $$4, %eax                       # eax <- B
    GET_VREG %eax, %eax                     # eax <- vB
    movswl  2(rPC), %ecx                    # ecx <- ssssCCCC
    andb    $$0xf, rINSTbl                  # rINST <- A
    $instr                                  # for example: addl %ecx, %eax
    SET_VREG $result, rINST
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
    movzbl  2(rPC), %eax                    # eax <- BB
    movsbl  3(rPC), %ecx                    # ecx <- ssssssCC
    GET_VREG %eax, %eax                     # eax <- rBB
    $instr                                  # ex: addl %ecx,%eax
    SET_VREG $result, rINST
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def binopWide(instr1="", instr2=""):
/*
 * Generic 64-bit binary operation.
 */
    /* binop vAA, vBB, vCC */
    movzbl  2(rPC), %eax                    # eax <- BB
    movzbl  3(rPC), %ecx                    # ecx <- CC
    movl    rIBASE, LOCAL0(%esp)            # save rIBASE
    GET_VREG rIBASE, %eax                   # rIBASE <- v[BB+0]
    GET_VREG_HIGH %eax, %eax                # eax <- v[BB+1]
    $instr1 VREG_ADDRESS(%ecx), rIBASE
    $instr2 VREG_HIGH_ADDRESS(%ecx), %eax
    SET_VREG rIBASE, rINST                  # v[AA+0] <- rIBASE
    movl    LOCAL0(%esp), rIBASE            # restore rIBASE
    SET_VREG_HIGH %eax, rINST               # v[AA+1] <- eax
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def binopWide2addr(instr1="", instr2=""):
/*
 * Generic 64-bit binary operation.
 */
    /* binop/2addr vA, vB */
    movzbl  rINSTbl, %ecx                   # ecx<- BA
    sarl    $$4, %ecx                       # ecx<- B
    GET_VREG %eax, %ecx                     # eax<- v[B+0]
    GET_VREG_HIGH %ecx, %ecx                # eax<- v[B+1]
    andb    $$0xF, rINSTbl                  # rINST<- A
    $instr1 %eax, VREG_ADDRESS(rINST)
    $instr2 %ecx, VREG_HIGH_ADDRESS(rINST)
    CLEAR_WIDE_REF rINST
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 1

%def cvtfp_int(srcdouble="1", tgtlong="1"):
/* On fp to int conversions, Java requires that
 * if the result > maxint, it should be clamped to maxint.  If it is less
 * than minint, it should be clamped to minint.  If it is a nan, the result
 * should be zero.  Further, the rounding mode is to truncate.  This model
 * differs from what is delivered normally via the x86 fpu, so we have
 * to play some games.
 */
    /* float/double to int/long vA, vB */
    movzbl  rINSTbl, %ecx                   # ecx <- A+
    sarl    $$4, rINST                      # rINST <- B
    .if $srcdouble
    fldl    VREG_ADDRESS(rINST)             # %st0 <- vB
    .else
    flds    VREG_ADDRESS(rINST)             # %st0 <- vB
    .endif
    ftst
    fnstcw  LOCAL0(%esp)                    # remember original rounding mode
    movzwl  LOCAL0(%esp), %eax
    movb    $$0xc, %ah
    movw    %ax, LOCAL0+2(%esp)
    fldcw   LOCAL0+2(%esp)                  # set "to zero" rounding mode
    andb    $$0xf, %cl                      # ecx <- A
    .if $tgtlong
    fistpll VREG_ADDRESS(%ecx)              # convert and store
    .else
    fistpl  VREG_ADDRESS(%ecx)              # convert and store
    .endif
    fldcw   LOCAL0(%esp)                    # restore previous rounding mode
    .if $tgtlong
    movl    $$0x80000000, %eax
    xorl    VREG_HIGH_ADDRESS(%ecx), %eax
    orl     VREG_ADDRESS(%ecx), %eax
    .else
    cmpl    $$0x80000000, VREG_ADDRESS(%ecx)
    .endif
    je      .L${opcode}_special_case # fix up result

.L${opcode}_finish:
    xor     %eax, %eax
    mov     %eax, VREG_REF_ADDRESS(%ecx)
    .if $tgtlong
    mov     %eax, VREG_REF_HIGH_ADDRESS(%ecx)
    .endif
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 1
%  add_helper(lambda: cvtfp_int_helper(tgtlong))

%def cvtfp_int_helper(tgtlong):
.L${opcode}_special_case:
    fnstsw  %ax
    sahf
    jp      .L${opcode}_isNaN
    adcl    $$-1, VREG_ADDRESS(%ecx)
    .if $tgtlong
    adcl    $$-1, VREG_HIGH_ADDRESS(%ecx)
    .endif
   jmp      .L${opcode}_finish
.L${opcode}_isNaN:
    movl    $$0, VREG_ADDRESS(%ecx)
    .if $tgtlong
    movl    $$0, VREG_HIGH_ADDRESS(%ecx)
    .endif
    jmp     .L${opcode}_finish

%def shop2addr(result="%eax", instr=""):
/*
 * Generic 32-bit "shift/2addr" operation.
 */
    /* shift/2addr vA, vB */
    movzx   rINSTbl, %ecx                   # eax <- BA
    sarl    $$4, %ecx                       # ecx <- B
    GET_VREG %ecx, %ecx                     # eax <- vBB
    andb    $$0xf, rINSTbl                  # rINST <- A
    GET_VREG %eax, rINST                    # eax <- vAA
    $instr                                  # ex: sarl %cl, %eax
    SET_VREG $result, rINST
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 1

%def unop(instr=""):
/*
 * Generic 32-bit unary operation.  Provide an "instr" line that
 * specifies an instruction that performs "result = op eax".
 */
    /* unop vA, vB */
    movzbl  rINSTbl,%ecx                    # ecx <- A+
    sarl    $$4,rINST                       # rINST <- B
    GET_VREG %eax, rINST                    # eax <- vB
    andb    $$0xf,%cl                       # ecx <- A
    $instr
    SET_VREG %eax, %ecx
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 1

%def op_add_int():
%  binop(instr="addl")

%def op_add_int_2addr():
%  binop2addr(instr="addl")

%def op_add_int_lit16():
%  binopLit16(instr="addl    %ecx, %eax")

%def op_add_int_lit8():
%  binopLit8(instr="addl    %ecx, %eax")

%def op_add_long():
%  binopWide(instr1="addl", instr2="adcl")

%def op_add_long_2addr():
%  binopWide2addr(instr1="addl", instr2="adcl")

%def op_and_int():
%  binop(instr="andl")

%def op_and_int_2addr():
%  binop2addr(instr="andl")

%def op_and_int_lit16():
%  binopLit16(instr="andl    %ecx, %eax")

%def op_and_int_lit8():
%  binopLit8(instr="andl    %ecx, %eax")

%def op_and_long():
%  binopWide(instr1="andl", instr2="andl")

%def op_and_long_2addr():
%  binopWide2addr(instr1="andl", instr2="andl")

%def op_cmp_long():
/*
 * Compare two 64-bit values.  Puts 0, 1, or -1 into the destination
 * register based on the results of the comparison.
 */
    /* cmp-long vAA, vBB, vCC */
    movzbl  2(rPC), %eax                    # eax <- BB
    movzbl  3(rPC), %ecx                    # ecx <- CC
    GET_VREG_HIGH %eax, %eax                # eax <- v[BB+1], BB is clobbered
    cmpl    VREG_HIGH_ADDRESS(%ecx), %eax
    jl      .L${opcode}_smaller
    jg      .L${opcode}_bigger
    movzbl  2(rPC), %eax                    # eax <- BB, restore BB
    GET_VREG %eax, %eax                     # eax <- v[BB]
    sub     VREG_ADDRESS(%ecx), %eax
    ja      .L${opcode}_bigger
    jb      .L${opcode}_smaller
.L${opcode}_finish:
    SET_VREG %eax, rINST
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

.L${opcode}_bigger:
    movl    $$1, %eax
    jmp     .L${opcode}_finish

.L${opcode}_smaller:
    movl    $$-1, %eax
    jmp     .L${opcode}_finish

%def op_div_int():
%  bindiv(result="%eax", special="$0x80000000", rem="0")

%def op_div_int_2addr():
%  bindiv2addr(result="%eax", special="$0x80000000")

%def op_div_int_lit16():
%  bindivLit16(result="%eax", special="$0x80000000")

%def op_div_int_lit8():
%  bindivLit8(result="%eax", special="$0x80000000")

%def op_div_long(routine="art_quick_ldiv"):
/* art_quick_* methods has quick abi,
 *   so use eax, ecx, edx, ebx for args
 */
    /* div vAA, vBB, vCC */
    .extern $routine
    mov     rIBASE, LOCAL0(%esp)            # save rIBASE/%edx
    mov     rINST, LOCAL1(%esp)             # save rINST/%ebx
    movzbl  3(rPC), %eax                    # eax <- CC
    GET_VREG %ecx, %eax
    GET_VREG_HIGH %ebx, %eax
    movl    %ecx, %edx
    orl     %ebx, %ecx
    jz      common_errDivideByZero
    movzbl  2(rPC), %eax                    # eax <- BB
    GET_VREG_HIGH %ecx, %eax
    GET_VREG %eax, %eax
    call    SYMBOL($routine)
    mov     LOCAL1(%esp), rINST             # restore rINST/%ebx
    SET_VREG_HIGH rIBASE, rINST
    SET_VREG %eax, rINST
    mov     LOCAL0(%esp), rIBASE            # restore rIBASE/%edx
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def op_div_long_2addr(routine="art_quick_ldiv"):
/* art_quick_* methods has quick abi,
 *   so use eax, ecx, edx, ebx for args
 */
    /* div/2addr vA, vB */
    .extern   $routine
    mov     rIBASE, LOCAL0(%esp)            # save rIBASE/%edx
    movzbl  rINSTbl, %eax
    shrl    $$4, %eax                       # eax <- B
    andb    $$0xf, rINSTbl                  # rINST <- A
    mov     rINST, LOCAL1(%esp)             # save rINST/%ebx
    movl    %ebx, %ecx
    GET_VREG %edx, %eax
    GET_VREG_HIGH %ebx, %eax
    movl    %edx, %eax
    orl     %ebx, %eax
    jz      common_errDivideByZero
    GET_VREG %eax, %ecx
    GET_VREG_HIGH %ecx, %ecx
    call    SYMBOL($routine)
    mov     LOCAL1(%esp), rINST             # restore rINST/%ebx
    SET_VREG_HIGH rIBASE, rINST
    SET_VREG %eax, rINST
    mov     LOCAL0(%esp), rIBASE            # restore rIBASE/%edx
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 1

%def op_int_to_byte():
%  unop(instr="movsbl  %al, %eax")

%def op_int_to_char():
%  unop(instr="movzwl  %ax,%eax")

%def op_int_to_long():
    /* int to long vA, vB */
    movzbl  rINSTbl, %eax                   # eax <- +A
    sarl    $$4, %eax                       # eax <- B
    GET_VREG %eax, %eax                     # eax <- vB
    andb    $$0xf, rINSTbl                  # rINST <- A
    movl    rIBASE, %ecx                    # cltd trashes rIBASE/edx
    cltd                                    # rINST:eax<- sssssssBBBBBBBB
    SET_VREG_HIGH rIBASE, rINST             # v[A+1] <- rIBASE
    SET_VREG %eax, rINST                    # v[A+0] <- %eax
    movl    %ecx, rIBASE
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 1


%def op_int_to_short():
%  unop(instr="movswl %ax, %eax")

%def op_long_to_int():
/* we ignore the high word, making this equivalent to a 32-bit reg move */
%  op_move()

%def op_mul_int():
    /*
     * 32-bit binary multiplication.
     */
    /* mul vAA, vBB, vCC */
    movzbl  2(rPC), %eax                    # eax <- BB
    movzbl  3(rPC), %ecx                    # ecx <- CC
    GET_VREG %eax, %eax                     # eax <- vBB
    mov     rIBASE, LOCAL0(%esp)
    imull   VREG_ADDRESS(%ecx), %eax        # trashes rIBASE/edx
    mov     LOCAL0(%esp), rIBASE
    SET_VREG %eax, rINST
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def op_mul_int_2addr():
    /* mul vA, vB */
    movzx   rINSTbl, %ecx                   # ecx <- A+
    sarl    $$4, rINST                      # rINST <- B
    GET_VREG %eax, rINST                    # eax <- vB
    andb    $$0xf, %cl                      # ecx <- A
    movl    rIBASE, rINST
    imull   VREG_ADDRESS(%ecx), %eax        # trashes rIBASE/edx
    movl    rINST, rIBASE
    SET_VREG %eax, %ecx
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 1

%def op_mul_int_lit16():
    /* mul/lit16 vA, vB, #+CCCC */
    /* Need A in rINST, ssssCCCC in ecx, vB in eax */
    movzbl  rINSTbl, %eax                   # eax <- 000000BA
    sarl    $$4, %eax                       # eax <- B
    GET_VREG %eax, %eax                     # eax <- vB
    movl    rIBASE, %ecx
    movswl  2(rPC), rIBASE                  # rIBASE <- ssssCCCC
    andb    $$0xf, rINSTbl                  # rINST <- A
    imull   rIBASE, %eax                    # trashes rIBASE/edx
    movl    %ecx, rIBASE
    SET_VREG %eax, rINST
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def op_mul_int_lit8():
    /* mul/lit8 vAA, vBB, #+CC */
    movzbl  2(rPC), %eax                    # eax <- BB
    movl    rIBASE, %ecx
    GET_VREG  %eax, %eax                    # eax <- rBB
    movsbl  3(rPC), rIBASE                  # rIBASE <- ssssssCC
    imull   rIBASE, %eax                    # trashes rIBASE/edx
    movl    %ecx, rIBASE
    SET_VREG %eax, rINST
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def op_mul_long():
/*
 * Signed 64-bit integer multiply.
 *
 * We could definately use more free registers for
 * this code.   We spill rINSTw (ebx),
 * giving us eax, ebc, ecx and edx as computational
 * temps.  On top of that, we'll spill edi (rFP)
 * for use as the vB pointer and esi (rPC) for use
 * as the vC pointer.  Yuck.
 *
 */
    /* mul-long vAA, vBB, vCC */
    movzbl  2(rPC), %eax                    # eax <- B
    movzbl  3(rPC), %ecx                    # ecx <- C
    mov     rPC, LOCAL0(%esp)               # save Interpreter PC
    mov     rFP, LOCAL1(%esp)               # save FP
    mov     rIBASE, LOCAL2(%esp)            # save rIBASE
    leal    (rFP,%eax,4), %esi              # esi <- &v[B]
    leal    VREG_ADDRESS(%ecx), rFP         # rFP <- &v[C]
    movl    4(%esi), %ecx                   # ecx <- Bmsw
    imull   (rFP), %ecx                     # ecx <- (Bmsw*Clsw)
    movl    4(rFP), %eax                    # eax <- Cmsw
    imull   (%esi), %eax                    # eax <- (Cmsw*Blsw)
    addl    %eax, %ecx                      # ecx <- (Bmsw*Clsw)+(Cmsw*Blsw)
    movl    (rFP), %eax                     # eax <- Clsw
    mull    (%esi)                          # eax <- (Clsw*Alsw)
    mov     LOCAL0(%esp), rPC               # restore Interpreter PC
    mov     LOCAL1(%esp), rFP               # restore FP
    leal    (%ecx,rIBASE), rIBASE           # full result now in rIBASE:%eax
    SET_VREG_HIGH rIBASE, rINST             # v[B+1] <- rIBASE
    mov     LOCAL2(%esp), rIBASE            # restore IBASE
    SET_VREG %eax, rINST                    # v[B] <- eax
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def op_mul_long_2addr():
/*
 * Signed 64-bit integer multiply, 2-addr version
 *
 * We could definately use more free registers for
 * this code.  We must spill %edx (rIBASE) because it
 * is used by imul.  We'll also spill rINST (ebx),
 * giving us eax, ebc, ecx and rIBASE as computational
 * temps.  On top of that, we'll spill %esi (edi)
 * for use as the vA pointer and rFP (esi) for use
 * as the vB pointer.  Yuck.
 */
    /* mul-long/2addr vA, vB */
    movzbl  rINSTbl, %eax                   # eax <- BA
    andb    $$0xf, %al                      # eax <- A
    CLEAR_WIDE_REF %eax                     # clear refs in advance
    sarl    $$4, rINST                      # rINST <- B
    mov     rPC, LOCAL0(%esp)               # save Interpreter PC
    mov     rFP, LOCAL1(%esp)               # save FP
    mov     rIBASE, LOCAL2(%esp)            # save rIBASE
    leal    (rFP,%eax,4), %esi              # esi <- &v[A]
    leal    (rFP,rINST,4), rFP              # rFP <- &v[B]
    movl    4(%esi), %ecx                   # ecx <- Amsw
    imull   (rFP), %ecx                     # ecx <- (Amsw*Blsw)
    movl    4(rFP), %eax                    # eax <- Bmsw
    imull   (%esi), %eax                    # eax <- (Bmsw*Alsw)
    addl    %eax, %ecx                      # ecx <- (Amsw*Blsw)+(Bmsw*Alsw)
    movl    (rFP), %eax                     # eax <- Blsw
    mull    (%esi)                          # eax <- (Blsw*Alsw)
    leal    (%ecx,rIBASE), rIBASE           # full result now in %edx:%eax
    movl    rIBASE, 4(%esi)                 # v[A+1] <- rIBASE
    movl    %eax, (%esi)                    # v[A] <- %eax
    mov     LOCAL0(%esp), rPC               # restore Interpreter PC
    mov     LOCAL2(%esp), rIBASE            # restore IBASE
    mov     LOCAL1(%esp), rFP               # restore FP
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 1

%def op_neg_int():
%  unop(instr="negl    %eax")

%def op_neg_long():
    /* unop vA, vB */
    movzbl  rINSTbl, %ecx                   # ecx <- BA
    sarl    $$4, %ecx                       # ecx <- B
    andb    $$0xf, rINSTbl                  # rINST <- A
    GET_VREG %eax, %ecx                     # eax <- v[B+0]
    GET_VREG_HIGH %ecx, %ecx                # ecx <- v[B+1]
    negl    %eax
    adcl    $$0, %ecx
    negl    %ecx
    SET_VREG %eax, rINST                    # v[A+0] <- eax
    SET_VREG_HIGH %ecx, rINST               # v[A+1] <- ecx
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 1


%def op_not_int():
%  unop(instr="notl %eax")

%def op_not_long():
    /* unop vA, vB */
    movzbl  rINSTbl, %ecx                   # ecx <- BA
    sarl    $$4, %ecx                       # ecx <- B
    andb    $$0xf, rINSTbl                  # rINST <- A
    GET_VREG %eax, %ecx                     # eax <- v[B+0]
    GET_VREG_HIGH %ecx, %ecx                # ecx <- v[B+1]
    notl    %eax
    notl    %ecx
    SET_VREG %eax, rINST                    # v[A+0] <- eax
    SET_VREG_HIGH %ecx, rINST               # v[A+1] <- ecx
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 1

%def op_or_int():
%  binop(instr="orl")

%def op_or_int_2addr():
%  binop2addr(instr="orl")

%def op_or_int_lit16():
%  binopLit16(instr="orl     %ecx, %eax")

%def op_or_int_lit8():
%  binopLit8(instr="orl     %ecx, %eax")

%def op_or_long():
%  binopWide(instr1="orl", instr2="orl")

%def op_or_long_2addr():
%  binopWide2addr(instr1="orl", instr2="orl")

%def op_rem_int():
%  bindiv(result="rIBASE", special="$0", rem="1")

%def op_rem_int_2addr():
%  bindiv2addr(result="rIBASE", special="$0")

%def op_rem_int_lit16():
%  bindivLit16(result="rIBASE", special="$0")

%def op_rem_int_lit8():
%  bindivLit8(result="rIBASE", special="$0")

%def op_rem_long():
%  op_div_long(routine="art_quick_lmod")

%def op_rem_long_2addr():
%  op_div_long_2addr(routine="art_quick_lmod")

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
/*
 * Long integer shift.  This is different from the generic 32/64-bit
 * binary operations because vAA/vBB are 64-bit but vCC (the shift
 * distance) is 32-bit.  Also, Dalvik requires us to mask off the low
 * 6 bits of the shift distance.  x86 shifts automatically mask off
 * the low 5 bits of %cl, so have to handle the 64 > shiftcount > 31
 * case specially.
 */
    /* shl-long vAA, vBB, vCC */
    /* ecx gets shift count */
    /* Need to spill rINST */
    /* rINSTw gets AA */
    movzbl  2(rPC), %eax                    # eax <- BB
    movzbl  3(rPC), %ecx                    # ecx <- CC
    movl    rIBASE, LOCAL0(%esp)
    GET_VREG_HIGH rIBASE, %eax              # ecx <- v[BB+1]
    GET_VREG %ecx, %ecx                     # ecx <- vCC
    GET_VREG %eax, %eax                     # eax <- v[BB+0]
    shldl   %eax,rIBASE
    sall    %cl, %eax
    testb   $$32, %cl
    je      2f
    movl    %eax, rIBASE
    xorl    %eax, %eax
2:
    SET_VREG_HIGH rIBASE, rINST             # v[AA+1] <- rIBASE
    movl    LOCAL0(%esp), rIBASE
    SET_VREG %eax, rINST                    # v[AA+0] <- %eax
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def op_shl_long_2addr():
/*
 * Long integer shift, 2addr version.  vA is 64-bit value/result, vB is
 * 32-bit shift distance.
 */
    /* shl-long/2addr vA, vB */
    /* ecx gets shift count */
    /* Need to spill rIBASE */
    /* rINSTw gets AA */
    movzbl  rINSTbl, %ecx                   # ecx <- BA
    andb    $$0xf, rINSTbl                  # rINST <- A
    GET_VREG %eax, rINST                    # eax <- v[AA+0]
    sarl    $$4, %ecx                       # ecx <- B
    movl    rIBASE, LOCAL0(%esp)
    GET_VREG_HIGH rIBASE, rINST             # rIBASE <- v[AA+1]
    GET_VREG %ecx, %ecx                     # ecx <- vBB
    shldl   %eax, rIBASE
    sall    %cl, %eax
    testb   $$32, %cl
    je      2f
    movl    %eax, rIBASE
    xorl    %eax, %eax
2:
    SET_VREG_HIGH rIBASE, rINST             # v[AA+1] <- rIBASE
    movl    LOCAL0(%esp), rIBASE
    SET_VREG %eax, rINST                    # v[AA+0] <- eax
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 1

%def op_shr_int():
%  binop1(instr="sarl    %cl, %eax")

%def op_shr_int_2addr():
%  shop2addr(instr="sarl    %cl, %eax")

%def op_shr_int_lit8():
%  binopLit8(instr="sarl    %cl, %eax")

%def op_shr_long():
/*
 * Long integer shift.  This is different from the generic 32/64-bit
 * binary operations because vAA/vBB are 64-bit but vCC (the shift
 * distance) is 32-bit.  Also, Dalvik requires us to mask off the low
 * 6 bits of the shift distance.  x86 shifts automatically mask off
 * the low 5 bits of %cl, so have to handle the 64 > shiftcount > 31
 * case specially.
 */
    /* shr-long vAA, vBB, vCC */
    /* ecx gets shift count */
    /* Need to spill rIBASE */
    /* rINSTw gets AA */
    movzbl  2(rPC), %eax                    # eax <- BB
    movzbl  3(rPC), %ecx                    # ecx <- CC
    movl    rIBASE, LOCAL0(%esp)
    GET_VREG_HIGH rIBASE, %eax              # rIBASE<- v[BB+1]
    GET_VREG %ecx, %ecx                     # ecx <- vCC
    GET_VREG %eax, %eax                     # eax <- v[BB+0]
    shrdl   rIBASE, %eax
    sarl    %cl, rIBASE
    testb   $$32, %cl
    je      2f
    movl    rIBASE, %eax
    sarl    $$31, rIBASE
2:
    SET_VREG_HIGH rIBASE, rINST             # v[AA+1] <- rIBASE
    movl    LOCAL0(%esp), rIBASE
    SET_VREG %eax, rINST                    # v[AA+0] <- eax
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def op_shr_long_2addr():
/*
 * Long integer shift, 2addr version.  vA is 64-bit value/result, vB is
 * 32-bit shift distance.
 */
    /* shl-long/2addr vA, vB */
    /* ecx gets shift count */
    /* Need to spill rIBASE */
    /* rINSTw gets AA */
    movzbl  rINSTbl, %ecx                   # ecx <- BA
    andb    $$0xf, rINSTbl                  # rINST <- A
    GET_VREG %eax, rINST                    # eax <- v[AA+0]
    sarl    $$4, %ecx                       # ecx <- B
    movl    rIBASE, LOCAL0(%esp)
    GET_VREG_HIGH rIBASE, rINST             # rIBASE <- v[AA+1]
    GET_VREG %ecx, %ecx                     # ecx <- vBB
    shrdl   rIBASE, %eax
    sarl    %cl, rIBASE
    testb   $$32, %cl
    je      2f
    movl    rIBASE, %eax
    sarl    $$31, rIBASE
2:
    SET_VREG_HIGH rIBASE, rINST             # v[AA+1] <- rIBASE
    movl    LOCAL0(%esp), rIBASE
    SET_VREG %eax, rINST                    # v[AA+0] <- eax
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 1

%def op_sub_int():
%  binop(instr="subl")

%def op_sub_int_2addr():
%  binop2addr(instr="subl")

%def op_sub_long():
%  binopWide(instr1="subl", instr2="sbbl")

%def op_sub_long_2addr():
%  binopWide2addr(instr1="subl", instr2="sbbl")

%def op_ushr_int():
%  binop1(instr="shrl    %cl, %eax")

%def op_ushr_int_2addr():
%  shop2addr(instr="shrl    %cl, %eax")

%def op_ushr_int_lit8():
%  binopLit8(instr="shrl    %cl, %eax")

%def op_ushr_long():
/*
 * Long integer shift.  This is different from the generic 32/64-bit
 * binary operations because vAA/vBB are 64-bit but vCC (the shift
 * distance) is 32-bit.  Also, Dalvik requires us to mask off the low
 * 6 bits of the shift distance.  x86 shifts automatically mask off
 * the low 5 bits of %cl, so have to handle the 64 > shiftcount > 31
 * case specially.
 */
    /* shr-long vAA, vBB, vCC */
    /* ecx gets shift count */
    /* Need to spill rIBASE */
    /* rINSTw gets AA */
    movzbl  2(rPC), %eax                    # eax <- BB
    movzbl  3(rPC), %ecx                    # ecx <- CC
    movl    rIBASE, LOCAL0(%esp)
    GET_VREG_HIGH rIBASE, %eax              # rIBASE <- v[BB+1]
    GET_VREG %ecx, %ecx                     # ecx <- vCC
    GET_VREG %eax, %eax                     # eax <- v[BB+0]
    shrdl   rIBASE, %eax
    shrl    %cl, rIBASE
    testb   $$32, %cl
    je      2f
    movl    rIBASE, %eax
    xorl    rIBASE, rIBASE
2:
    SET_VREG_HIGH rIBASE, rINST             # v[AA+1] <- rIBASE
    movl    LOCAL0(%esp), rIBASE
    SET_VREG %eax, rINST                    # v[BB+0] <- eax
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def op_ushr_long_2addr():
/*
 * Long integer shift, 2addr version.  vA is 64-bit value/result, vB is
 * 32-bit shift distance.
 */
    /* shl-long/2addr vA, vB */
    /* ecx gets shift count */
    /* Need to spill rIBASE */
    /* rINSTw gets AA */
    movzbl  rINSTbl, %ecx                   # ecx <- BA
    andb    $$0xf, rINSTbl                  # rINST <- A
    GET_VREG %eax, rINST                    # eax <- v[AA+0]
    sarl    $$4, %ecx                       # ecx <- B
    movl    rIBASE, LOCAL0(%esp)
    GET_VREG_HIGH rIBASE, rINST             # rIBASE <- v[AA+1]
    GET_VREG %ecx, %ecx                     # ecx <- vBB
    shrdl   rIBASE, %eax
    shrl    %cl, rIBASE
    testb   $$32, %cl
    je      2f
    movl    rIBASE, %eax
    xorl    rIBASE, rIBASE
2:
    SET_VREG_HIGH rIBASE, rINST             # v[AA+1] <- rIBASE
    movl    LOCAL0(%esp), rIBASE
    SET_VREG %eax, rINST                    # v[AA+0] <- eax
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 1

%def op_xor_int():
%  binop(instr="xorl")

%def op_xor_int_2addr():
%  binop2addr(instr="xorl")

%def op_xor_int_lit16():
%  binopLit16(instr="xorl    %ecx, %eax")

%def op_xor_int_lit8():
%  binopLit8(instr="xorl    %ecx, %eax")

%def op_xor_long():
%  binopWide(instr1="xorl", instr2="xorl")

%def op_xor_long_2addr():
%  binopWide2addr(instr1="xorl", instr2="xorl")
