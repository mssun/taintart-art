%def binop(preinstr="", result="w0", chkzero="0", instr=""):
    /*
     * Generic 32-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = w0 op w1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than w0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (w1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (INT_MIN / -1) here, because the ARM math lib
     * handles it correctly.
     *
     * For: add-int, sub-int, mul-int, div-int, rem-int, and-int, or-int,
     *      xor-int, shl-int, shr-int, ushr-int, add-float, sub-float,
     *      mul-float, div-float, rem-float
     */
    /* binop vAA, vBB, vCC */
    FETCH w0, 1                         // w0<- CCBB
    lsr     w9, wINST, #8               // w9<- AA
    lsr     w3, w0, #8                  // w3<- CC
    and     w2, w0, #255                // w2<- BB
    GET_VREG w1, w3                     // w1<- vCC
    GET_VREG w0, w2                     // w0<- vBB
    .if $chkzero
    cbz     w1, common_errDivideByZero  // is second operand zero?
    .endif
    FETCH_ADVANCE_INST 2                // advance rPC, load rINST
    $preinstr                           // optional op; may set condition codes
    $instr                              // $result<- op, w0-w3 changed
    GET_INST_OPCODE ip                  // extract opcode from rINST
    SET_VREG $result, w9                // vAA<- $result
    GOTO_OPCODE ip                      // jump to next instruction
    /* 11-14 instructions */

%def binop2addr(preinstr="", result="w0", chkzero="0", instr=""):
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = w0 op w1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than w0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (w1).  Useful for integer division and modulus.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr, add-float/2addr,
     *      sub-float/2addr, mul-float/2addr, div-float/2addr, rem-float/2addr
     */
    /* binop/2addr vA, vB */
    lsr     w3, wINST, #12              // w3<- B
    ubfx    w9, wINST, #8, #4           // w9<- A
    GET_VREG w1, w3                     // w1<- vB
    GET_VREG w0, w9                     // w0<- vA
    .if $chkzero
    cbz     w1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST 1                // advance rPC, load rINST
    $preinstr                           // optional op; may set condition codes
    $instr                              // $result<- op, w0-w3 changed
    GET_INST_OPCODE ip                  // extract opcode from rINST
    SET_VREG $result, w9                // vAA<- $result
    GOTO_OPCODE ip                      // jump to next instruction
    /* 10-13 instructions */

%def binopLit16(preinstr="", result="w0", chkzero="0", instr=""):
    /*
     * Generic 32-bit "lit16" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = w0 op w1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than w0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (w1).  Useful for integer division and modulus.
     *
     * For: add-int/lit16, rsub-int, mul-int/lit16, div-int/lit16,
     *      rem-int/lit16, and-int/lit16, or-int/lit16, xor-int/lit16
     */
    /* binop/lit16 vA, vB, #+CCCC */
    FETCH_S w1, 1                       // w1<- ssssCCCC (sign-extended)
    lsr     w2, wINST, #12              // w2<- B
    ubfx    w9, wINST, #8, #4           // w9<- A
    GET_VREG w0, w2                     // w0<- vB
    .if $chkzero
    cbz     w1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST 2                // advance rPC, load rINST
    $preinstr
    $instr                              // $result<- op, w0-w3 changed
    GET_INST_OPCODE ip                  // extract opcode from rINST
    SET_VREG $result, w9                // vAA<- $result
    GOTO_OPCODE ip                      // jump to next instruction
    /* 10-13 instructions */

%def binopLit8(extract="asr     w1, w3, #8", preinstr="", result="w0", chkzero="0", instr=""):
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = w0 op w1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than w0, you can override "result".)
     *
     * You can override "extract" if the extraction of the literal value
     * from w3 to w1 is not the default "asr w1, w3, #8". The extraction
     * can be omitted completely if the shift is embedded in "instr".
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (w1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    /* binop/lit8 vAA, vBB, #+CC */
    FETCH_S w3, 1                       // w3<- ssssCCBB (sign-extended for CC)
    lsr     w9, wINST, #8               // w9<- AA
    and     w2, w3, #255                // w2<- BB
    GET_VREG w0, w2                     // w0<- vBB
    $extract                            // optional; typically w1<- ssssssCC (sign extended)
    .if $chkzero
    cbz     w1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST 2                // advance rPC, load rINST
    $preinstr                           // optional op; may set condition codes
    $instr                              // $result<- op, w0-w3 changed
    GET_INST_OPCODE ip                  // extract opcode from rINST
    SET_VREG $result, w9                // vAA<- $result
    GOTO_OPCODE ip                      // jump to next instruction
    /* 10-12 instructions */

%def binopWide(preinstr="", instr="add x0, x1, x2", result="x0", r1="x1", r2="x2", chkzero="0"):
    /*
     * Generic 64-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = x1 op x2".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than x0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (w1).  Useful for integer division and modulus.
     *
     * For: add-long, sub-long, mul-long, div-long, rem-long, and-long, or-long,
     *      xor-long, add-double, sub-double, mul-double, div-double, rem-double
     */
    /* binop vAA, vBB, vCC */
    FETCH w0, 1                         // w0<- CCBB
    lsr     w4, wINST, #8               // w4<- AA
    lsr     w2, w0, #8                  // w2<- CC
    and     w1, w0, #255                // w1<- BB
    GET_VREG_WIDE $r2, w2               // w2<- vCC
    GET_VREG_WIDE $r1, w1               // w1<- vBB
    .if $chkzero
    cbz     $r2, common_errDivideByZero  // is second operand zero?
    .endif
    FETCH_ADVANCE_INST 2                // advance rPC, load rINST
    $preinstr
    $instr                              // $result<- op, w0-w4 changed
    GET_INST_OPCODE ip                  // extract opcode from rINST
    SET_VREG_WIDE $result, w4           // vAA<- $result
    GOTO_OPCODE ip                      // jump to next instruction
    /* 11-14 instructions */

%def binopWide2addr(preinstr="", instr="add x0, x0, x1", r0="x0", r1="x1", chkzero="0"):
    /*
     * Generic 64-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "x0 = x0 op x1".
     * This must not be a function call, as we keep w2 live across it.
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (w1).  Useful for integer division and modulus.
     *
     * For: add-long/2addr, sub-long/2addr, mul-long/2addr, div-long/2addr,
     *      and-long/2addr, or-long/2addr, xor-long/2addr,
     *      shl-long/2addr, shr-long/2addr, ushr-long/2addr, add-double/2addr,
     *      sub-double/2addr, mul-double/2addr, div-double/2addr, rem-double/2addr
     */
    /* binop/2addr vA, vB */
    lsr     w1, wINST, #12              // w1<- B
    ubfx    w2, wINST, #8, #4           // w2<- A
    GET_VREG_WIDE $r1, w1               // x1<- vB
    GET_VREG_WIDE $r0, w2               // x0<- vA
    .if $chkzero
    cbz     $r1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST 1                // advance rPC, load rINST
    $preinstr
    $instr                              // result<- op
    GET_INST_OPCODE ip                  // extract opcode from rINST
    SET_VREG_WIDE $r0, w2               // vAA<- result
    GOTO_OPCODE ip                      // jump to next instruction
    /* 10-13 instructions */

%def shiftWide(opcode="shl"):
    /*
     * 64-bit shift operation.
     *
     * For: shl-long, shr-long, ushr-long
     */
    /* binop vAA, vBB, vCC */
    FETCH w0, 1                         // w0<- CCBB
    lsr      w3, wINST, #8               // w3<- AA
    lsr      w2, w0, #8                  // w2<- CC
    GET_VREG w2, w2                     // w2<- vCC (shift count)
    and      w1, w0, #255                // w1<- BB
    GET_VREG_WIDE x1, w1                // x1<- vBB
    FETCH_ADVANCE_INST 2                // advance rPC, load rINST
    $opcode  x0, x1, x2                 // Do the shift. Only low 6 bits of x2 are used.
    GET_INST_OPCODE ip                  // extract opcode from rINST
    SET_VREG_WIDE x0, w3                // vAA<- x0
    GOTO_OPCODE ip                      // jump to next instruction
    /* 11-14 instructions */

%def shiftWide2addr(opcode="lsl"):
    /*
     * Generic 64-bit shift operation.
     */
    /* binop/2addr vA, vB */
    lsr     w1, wINST, #12              // w1<- B
    ubfx    w2, wINST, #8, #4           // w2<- A
    GET_VREG w1, w1                     // x1<- vB
    GET_VREG_WIDE x0, w2                // x0<- vA
    FETCH_ADVANCE_INST 1                // advance rPC, load rINST
    $opcode x0, x0, x1                  // Do the shift. Only low 6 bits of x1 are used.
    GET_INST_OPCODE ip                  // extract opcode from rINST
    SET_VREG_WIDE x0, w2               // vAA<- result
    GOTO_OPCODE ip                      // jump to next instruction
    /* 10-13 instructions */

%def unop(instr=""):
    /*
     * Generic 32-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = op w0".
     * This could be an ARM instruction or a function call.
     *
     * for: neg-int, not-int, neg-float, int-to-float, float-to-int,
     *      int-to-byte, int-to-char, int-to-short
     */
    /* unop vA, vB */
    lsr     w3, wINST, #12              // w3<- B
    GET_VREG w0, w3                     // w0<- vB
    ubfx    w9, wINST, #8, #4           // w9<- A
    FETCH_ADVANCE_INST 1                // advance rPC, load rINST
    $instr                              // w0<- op, w0-w3 changed
    GET_INST_OPCODE ip                  // extract opcode from rINST
    SET_VREG w0, w9                     // vAA<- w0
    GOTO_OPCODE ip                      // jump to next instruction
    /* 8-9 instructions */

%def unopWide(instr="sub x0, xzr, x0"):
    /*
     * Generic 64-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = op x0".
     *
     * For: neg-long, not-long
     */
    /* unop vA, vB */
    lsr     w3, wINST, #12              // w3<- B
    ubfx    w4, wINST, #8, #4           // w4<- A
    GET_VREG_WIDE x0, w3
    FETCH_ADVANCE_INST 1                // advance rPC, load wINST
    $instr
    GET_INST_OPCODE ip                  // extract opcode from wINST
    SET_VREG_WIDE x0, w4
    GOTO_OPCODE ip                      // jump to next instruction
    /* 10-11 instructions */

%def op_add_int():
%  binop(instr="add     w0, w0, w1")

%def op_add_int_2addr():
%  binop2addr(instr="add     w0, w0, w1")

%def op_add_int_lit16():
%  binopLit16(instr="add     w0, w0, w1")

%def op_add_int_lit8():
%  binopLit8(extract="", instr="add     w0, w0, w3, asr #8")

%def op_add_long():
%  binopWide(instr="add x0, x1, x2")

%def op_add_long_2addr():
%  binopWide2addr(instr="add     x0, x0, x1")

%def op_and_int():
%  binop(instr="and     w0, w0, w1")

%def op_and_int_2addr():
%  binop2addr(instr="and     w0, w0, w1")

%def op_and_int_lit16():
%  binopLit16(instr="and     w0, w0, w1")

%def op_and_int_lit8():
%  binopLit8(extract="", instr="and     w0, w0, w3, asr #8")

%def op_and_long():
%  binopWide(instr="and x0, x1, x2")

%def op_and_long_2addr():
%  binopWide2addr(instr="and     x0, x0, x1")

%def op_cmp_long():
    FETCH w0, 1                         // w0<- CCBB
    lsr     w4, wINST, #8               // w4<- AA
    and     w2, w0, #255                // w2<- BB
    lsr     w3, w0, #8                  // w3<- CC
    GET_VREG_WIDE x1, w2
    GET_VREG_WIDE x2, w3
    cmp     x1, x2
    cset    w0, ne
    cneg    w0, w0, lt
    FETCH_ADVANCE_INST 2                // advance rPC, load wINST
    SET_VREG w0, w4
    GET_INST_OPCODE ip                  // extract opcode from wINST
    GOTO_OPCODE ip                      // jump to next instruction

%def op_div_int():
%  binop(instr="sdiv     w0, w0, w1", chkzero="1")

%def op_div_int_2addr():
%  binop2addr(instr="sdiv     w0, w0, w1", chkzero="1")

%def op_div_int_lit16():
%  binopLit16(instr="sdiv w0, w0, w1", chkzero="1")

%def op_div_int_lit8():
%  binopLit8(instr="sdiv     w0, w0, w1", chkzero="1")

%def op_div_long():
%  binopWide(instr="sdiv x0, x1, x2", chkzero="1")

%def op_div_long_2addr():
%  binopWide2addr(instr="sdiv     x0, x0, x1", chkzero="1")

%def op_int_to_byte():
%  unop(instr="sxtb    w0, w0")

%def op_int_to_char():
%  unop(instr="uxth    w0, w0")

%def op_int_to_long():
    /* int-to-long vA, vB */
    lsr     w3, wINST, #12              // w3<- B
    ubfx    w4, wINST, #8, #4           // w4<- A
    GET_VREG_S x0, w3                   // x0<- sign_extend(fp[B])
    FETCH_ADVANCE_INST 1                // advance rPC, load wINST
    GET_INST_OPCODE ip                  // extract opcode from wINST
    SET_VREG_WIDE x0, w4                // fp[A]<- x0
    GOTO_OPCODE ip                      // jump to next instruction

%def op_int_to_short():
%  unop(instr="sxth    w0, w0")

%def op_long_to_int():
/* we ignore the high word, making this equivalent to a 32-bit reg move */
%  op_move()

%def op_mul_int():
/* must be "mul w0, w1, w0" -- "w0, w0, w1" is illegal */
%  binop(instr="mul     w0, w1, w0")

%def op_mul_int_2addr():
/* must be "mul w0, w1, w0" -- "w0, w0, w1" is illegal */
%  binop2addr(instr="mul     w0, w1, w0")

%def op_mul_int_lit16():
/* must be "mul w0, w1, w0" -- "w0, w0, w1" is illegal */
%  binopLit16(instr="mul     w0, w1, w0")

%def op_mul_int_lit8():
/* must be "mul w0, w1, w0" -- "w0, w0, w1" is illegal */
%  binopLit8(instr="mul     w0, w1, w0")

%def op_mul_long():
%  binopWide(instr="mul x0, x1, x2")

%def op_mul_long_2addr():
%  binopWide2addr(instr="mul     x0, x0, x1")

%def op_neg_int():
%  unop(instr="sub     w0, wzr, w0")

%def op_neg_long():
%  unopWide(instr="sub x0, xzr, x0")

%def op_not_int():
%  unop(instr="mvn     w0, w0")

%def op_not_long():
%  unopWide(instr="mvn     x0, x0")

%def op_or_int():
%  binop(instr="orr     w0, w0, w1")

%def op_or_int_2addr():
%  binop2addr(instr="orr     w0, w0, w1")

%def op_or_int_lit16():
%  binopLit16(instr="orr     w0, w0, w1")

%def op_or_int_lit8():
%  binopLit8(extract="", instr="orr     w0, w0, w3, asr #8")

%def op_or_long():
%  binopWide(instr="orr x0, x1, x2")

%def op_or_long_2addr():
%  binopWide2addr(instr="orr     x0, x0, x1")

%def op_rem_int():
%  binop(preinstr="sdiv     w2, w0, w1", instr="msub w0, w2, w1, w0", chkzero="1")

%def op_rem_int_2addr():
%  binop2addr(preinstr="sdiv     w2, w0, w1", instr="msub w0, w2, w1, w0", chkzero="1")

%def op_rem_int_lit16():
%  binopLit16(preinstr="sdiv w3, w0, w1", instr="msub w0, w3, w1, w0", chkzero="1")

%def op_rem_int_lit8():
%  binopLit8(preinstr="sdiv w3, w0, w1", instr="msub w0, w3, w1, w0", chkzero="1")

%def op_rem_long():
%  binopWide(preinstr="sdiv x3, x1, x2", instr="msub x0, x3, x2, x1", chkzero="1")

%def op_rem_long_2addr():
%  binopWide2addr(preinstr="sdiv x3, x0, x1", instr="msub x0, x3, x1, x0", chkzero="1")

%def op_rsub_int():
/* this op is "rsub-int", but can be thought of as "rsub-int/lit16" */
%  binopLit16(instr="sub     w0, w1, w0")

%def op_rsub_int_lit8():
%  binopLit8(instr="sub     w0, w1, w0")

%def op_shl_int():
%  binop(instr="lsl     w0, w0, w1")

%def op_shl_int_2addr():
%  binop2addr(instr="lsl     w0, w0, w1")

%def op_shl_int_lit8():
%  binopLit8(extract="ubfx    w1, w3, #8, #5", instr="lsl     w0, w0, w1")

%def op_shl_long():
%  shiftWide(opcode="lsl")

%def op_shl_long_2addr():
%  shiftWide2addr(opcode="lsl")

%def op_shr_int():
%  binop(instr="asr     w0, w0, w1")

%def op_shr_int_2addr():
%  binop2addr(instr="asr     w0, w0, w1")

%def op_shr_int_lit8():
%  binopLit8(extract="ubfx    w1, w3, #8, #5", instr="asr     w0, w0, w1")

%def op_shr_long():
%  shiftWide(opcode="asr")

%def op_shr_long_2addr():
%  shiftWide2addr(opcode="asr")

%def op_sub_int():
%  binop(instr="sub     w0, w0, w1")

%def op_sub_int_2addr():
%  binop2addr(instr="sub     w0, w0, w1")

%def op_sub_long():
%  binopWide(instr="sub x0, x1, x2")

%def op_sub_long_2addr():
%  binopWide2addr(instr="sub     x0, x0, x1")

%def op_ushr_int():
%  binop(instr="lsr     w0, w0, w1")

%def op_ushr_int_2addr():
%  binop2addr(instr="lsr     w0, w0, w1")

%def op_ushr_int_lit8():
%  binopLit8(extract="ubfx    w1, w3, #8, #5", instr="lsr     w0, w0, w1")

%def op_ushr_long():
%  shiftWide(opcode="lsr")

%def op_ushr_long_2addr():
%  shiftWide2addr(opcode="lsr")

%def op_xor_int():
%  binop(instr="eor     w0, w0, w1")

%def op_xor_int_2addr():
%  binop2addr(instr="eor     w0, w0, w1")

%def op_xor_int_lit16():
%  binopLit16(instr="eor     w0, w0, w1")

%def op_xor_int_lit8():
%  binopLit8(extract="", instr="eor     w0, w0, w3, asr #8")

%def op_xor_long():
%  binopWide(instr="eor x0, x1, x2")

%def op_xor_long_2addr():
%  binopWide2addr(instr="eor     x0, x0, x1")
