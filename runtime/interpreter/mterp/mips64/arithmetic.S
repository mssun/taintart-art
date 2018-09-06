%def binop(preinstr="", result="a0", chkzero="0", instr=""):
    /*
     * Generic 32-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (INT_MIN / -1) here, because the CPU handles it
     * correctly.
     *
     * For: add-int, sub-int, mul-int, div-int, rem-int, and-int, or-int,
     *      xor-int, shl-int, shr-int, ushr-int
     */
    /* binop vAA, vBB, vCC */
    srl     a4, rINST, 8                # a4 <- AA
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    GET_VREG a0, a2                     # a0 <- vBB
    GET_VREG a1, a3                     # a1 <- vCC
    .if $chkzero
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    $preinstr                           # optional op
    $instr                              # $result <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG $result, a4                # vAA <- $result
    GOTO_OPCODE v0                      # jump to next instruction

%def binop2addr(preinstr="", result="a0", chkzero="0", instr=""):
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vB (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (INT_MIN / -1) here, because the CPU handles it
     * correctly.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr
     */
    /* binop/2addr vA, vB */
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG a0, a2                     # a0 <- vA
    GET_VREG a1, a3                     # a1 <- vB
    .if $chkzero
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
    $preinstr                           # optional op
    $instr                              # $result <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG $result, a2                # vA <- $result
    GOTO_OPCODE v0                      # jump to next instruction

%def binopLit16(preinstr="", result="a0", chkzero="0", instr=""):
    /*
     * Generic 32-bit "lit16" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * CCCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit16, rsub-int, mul-int/lit16, div-int/lit16,
     *      rem-int/lit16, and-int/lit16, or-int/lit16, xor-int/lit16
     */
    /* binop/lit16 vA, vB, #+CCCC */
    lh      a1, 2(rPC)                  # a1 <- sign-extended CCCC
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG a0, a3                     # a0 <- vB
    .if $chkzero
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    $preinstr                           # optional op
    $instr                              # $result <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG $result, a2                # vA <- $result
    GOTO_OPCODE v0                      # jump to next instruction


%def binopLit8(preinstr="", result="a0", chkzero="0", instr=""):
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * CC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    /* binop/lit8 vAA, vBB, #+CC */
    lbu     a3, 2(rPC)                  # a3 <- BB
    lb      a1, 3(rPC)                  # a1 <- sign-extended CC
    srl     a2, rINST, 8                # a2 <- AA
    GET_VREG a0, a3                     # a0 <- vBB
    .if $chkzero
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    $preinstr                           # optional op
    $instr                              # $result <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG $result, a2                # vAA <- $result
    GOTO_OPCODE v0                      # jump to next instruction


%def binopWide(preinstr="", result="a0", chkzero="0", instr=""):
    /*
     * Generic 64-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (LONG_MIN / -1) here, because the CPU handles it
     * correctly.
     *
     * For: add-long, sub-long, mul-long, div-long, rem-long, and-long, or-long,
     *      xor-long, shl-long, shr-long, ushr-long
     */
    /* binop vAA, vBB, vCC */
    srl     a4, rINST, 8                # a4 <- AA
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    GET_VREG_WIDE a0, a2                # a0 <- vBB
    GET_VREG_WIDE a1, a3                # a1 <- vCC
    .if $chkzero
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    $preinstr                           # optional op
    $instr                              # $result <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_WIDE $result, a4           # vAA <- $result
    GOTO_OPCODE v0                      # jump to next instruction

%def binopWide2addr(preinstr="", result="a0", chkzero="0", instr=""):
    /*
     * Generic 64-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vB (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (LONG_MIN / -1) here, because the CPU handles it
     * correctly.
     *
     * For: add-long/2addr, sub-long/2addr, mul-long/2addr, div-long/2addr,
     *      rem-long/2addr, and-long/2addr, or-long/2addr, xor-long/2addr,
     *      shl-long/2addr, shr-long/2addr, ushr-long/2addr
     */
    /* binop/2addr vA, vB */
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG_WIDE a0, a2                # a0 <- vA
    GET_VREG_WIDE a1, a3                # a1 <- vB
    .if $chkzero
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
    $preinstr                           # optional op
    $instr                              # $result <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_WIDE $result, a2           # vA <- $result
    GOTO_OPCODE v0                      # jump to next instruction

%def unop(preinstr="", instr=""):
    /*
     * Generic 32-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "a0 = op a0".
     *
     * for: int-to-byte, int-to-char, int-to-short,
     *      not-int, neg-int
     */
    /* unop vA, vB */
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG a0, a3                     # a0 <- vB
    ext     a2, rINST, 8, 4             # a2 <- A
    $preinstr                           # optional op
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
    $instr                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a2                     # vA <- a0
    GOTO_OPCODE v0                      # jump to next instruction

%def unopWide(preinstr="", instr=""):
    /*
     * Generic 64-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "a0 = op a0".
     *
     * For: not-long, neg-long
     */
    /* unop vA, vB */
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG_WIDE a0, a3                # a0 <- vB
    ext     a2, rINST, 8, 4             # a2 <- A
    $preinstr                           # optional op
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
    $instr                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_WIDE a0, a2                # vA <- a0
    GOTO_OPCODE v0                      # jump to next instruction

%def op_add_int():
%  binop(instr="addu a0, a0, a1")

%def op_add_int_2addr():
%  binop2addr(instr="addu a0, a0, a1")

%def op_add_int_lit16():
%  binopLit16(instr="addu a0, a0, a1")

%def op_add_int_lit8():
%  binopLit8(instr="addu a0, a0, a1")

%def op_add_long():
%  binopWide(instr="daddu a0, a0, a1")

%def op_add_long_2addr():
%  binopWide2addr(instr="daddu a0, a0, a1")

%def op_and_int():
%  binop(instr="and a0, a0, a1")

%def op_and_int_2addr():
%  binop2addr(instr="and a0, a0, a1")

%def op_and_int_lit16():
%  binopLit16(instr="and a0, a0, a1")

%def op_and_int_lit8():
%  binopLit8(instr="and a0, a0, a1")

%def op_and_long():
%  binopWide(instr="and a0, a0, a1")

%def op_and_long_2addr():
%  binopWide2addr(instr="and a0, a0, a1")

%def op_cmp_long():
    /* cmp-long vAA, vBB, vCC */
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    srl     a4, rINST, 8                # a4 <- AA
    GET_VREG_WIDE a0, a2                # a0 <- vBB
    GET_VREG_WIDE a1, a3                # a1 <- vCC
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    slt     a2, a0, a1
    slt     a0, a1, a0
    subu    a0, a0, a2
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a4                     # vAA <- result
    GOTO_OPCODE v0                      # jump to next instruction

%def op_div_int():
%  binop(instr="div a0, a0, a1", chkzero="1")

%def op_div_int_2addr():
%  binop2addr(instr="div a0, a0, a1", chkzero="1")

%def op_div_int_lit16():
%  binopLit16(instr="div a0, a0, a1", chkzero="1")

%def op_div_int_lit8():
%  binopLit8(instr="div a0, a0, a1", chkzero="1")

%def op_div_long():
%  binopWide(instr="ddiv a0, a0, a1", chkzero="1")

%def op_div_long_2addr():
%  binopWide2addr(instr="ddiv a0, a0, a1", chkzero="1")

%def op_int_to_byte():
%  unop(instr="seb     a0, a0")

%def op_int_to_char():
%  unop(instr="and     a0, a0, 0xffff")

%def op_int_to_long():
    /* int-to-long vA, vB */
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG a0, a3                     # a0 <- vB (sign-extended to 64 bits)
    ext     a2, rINST, 8, 4             # a2 <- A
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_WIDE a0, a2                # vA <- vB
    GOTO_OPCODE v0                      # jump to next instruction

%def op_int_to_short():
%  unop(instr="seh     a0, a0")

%def op_long_to_int():
/* we ignore the high word, making this equivalent to a 32-bit reg move */
%  op_move()

%def op_mul_int():
%  binop(instr="mul a0, a0, a1")

%def op_mul_int_2addr():
%  binop2addr(instr="mul a0, a0, a1")

%def op_mul_int_lit16():
%  binopLit16(instr="mul a0, a0, a1")

%def op_mul_int_lit8():
%  binopLit8(instr="mul a0, a0, a1")

%def op_mul_long():
%  binopWide(instr="dmul a0, a0, a1")

%def op_mul_long_2addr():
%  binopWide2addr(instr="dmul a0, a0, a1")

%def op_neg_int():
%  unop(instr="subu    a0, zero, a0")

%def op_neg_long():
%  unopWide(instr="dsubu   a0, zero, a0")

%def op_not_int():
%  unop(instr="nor     a0, zero, a0")

%def op_not_long():
%  unopWide(instr="nor     a0, zero, a0")

%def op_or_int():
%  binop(instr="or a0, a0, a1")

%def op_or_int_2addr():
%  binop2addr(instr="or a0, a0, a1")

%def op_or_int_lit16():
%  binopLit16(instr="or a0, a0, a1")

%def op_or_int_lit8():
%  binopLit8(instr="or a0, a0, a1")

%def op_or_long():
%  binopWide(instr="or a0, a0, a1")

%def op_or_long_2addr():
%  binopWide2addr(instr="or a0, a0, a1")

%def op_rem_int():
%  binop(instr="mod a0, a0, a1", chkzero="1")

%def op_rem_int_2addr():
%  binop2addr(instr="mod a0, a0, a1", chkzero="1")

%def op_rem_int_lit16():
%  binopLit16(instr="mod a0, a0, a1", chkzero="1")

%def op_rem_int_lit8():
%  binopLit8(instr="mod a0, a0, a1", chkzero="1")

%def op_rem_long():
%  binopWide(instr="dmod a0, a0, a1", chkzero="1")

%def op_rem_long_2addr():
%  binopWide2addr(instr="dmod a0, a0, a1", chkzero="1")

%def op_rsub_int():
%  binopLit16(instr="subu a0, a1, a0")

%def op_rsub_int_lit8():
%  binopLit8(instr="subu a0, a1, a0")

%def op_shl_int():
%  binop(instr="sll a0, a0, a1")

%def op_shl_int_2addr():
%  binop2addr(instr="sll a0, a0, a1")

%def op_shl_int_lit8():
%  binopLit8(instr="sll a0, a0, a1")

%def op_shl_long():
%  binopWide(instr="dsll a0, a0, a1")

%def op_shl_long_2addr():
%  binopWide2addr(instr="dsll a0, a0, a1")

%def op_shr_int():
%  binop(instr="sra a0, a0, a1")

%def op_shr_int_2addr():
%  binop2addr(instr="sra a0, a0, a1")

%def op_shr_int_lit8():
%  binopLit8(instr="sra a0, a0, a1")

%def op_shr_long():
%  binopWide(instr="dsra a0, a0, a1")

%def op_shr_long_2addr():
%  binopWide2addr(instr="dsra a0, a0, a1")

%def op_sub_int():
%  binop(instr="subu a0, a0, a1")

%def op_sub_int_2addr():
%  binop2addr(instr="subu a0, a0, a1")

%def op_sub_long():
%  binopWide(instr="dsubu a0, a0, a1")

%def op_sub_long_2addr():
%  binopWide2addr(instr="dsubu a0, a0, a1")

%def op_ushr_int():
%  binop(instr="srl a0, a0, a1")

%def op_ushr_int_2addr():
%  binop2addr(instr="srl a0, a0, a1")

%def op_ushr_int_lit8():
%  binopLit8(instr="srl a0, a0, a1")

%def op_ushr_long():
%  binopWide(instr="dsrl a0, a0, a1")

%def op_ushr_long_2addr():
%  binopWide2addr(instr="dsrl a0, a0, a1")

%def op_xor_int():
%  binop(instr="xor a0, a0, a1")

%def op_xor_int_2addr():
%  binop2addr(instr="xor a0, a0, a1")

%def op_xor_int_lit16():
%  binopLit16(instr="xor a0, a0, a1")

%def op_xor_int_lit8():
%  binopLit8(instr="xor a0, a0, a1")

%def op_xor_long():
%  binopWide(instr="xor a0, a0, a1")

%def op_xor_long_2addr():
%  binopWide2addr(instr="xor a0, a0, a1")
