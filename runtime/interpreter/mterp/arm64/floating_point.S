%def fbinop(instr=""):
    /*
     * Generic 32-bit floating-point operation.
     *
     * For: add-float, sub-float, mul-float, div-float
     * form: <op> s0, s0, s1
     */
    /* floatop vAA, vBB, vCC */
    FETCH w0, 1                         // r0<- CCBB
    lsr     w1, w0, #8                  // r2<- CC
    and     w0, w0, #255                // r1<- BB
    GET_VREG  s1, w1
    GET_VREG  s0, w0
    $instr                              // s0<- op
    lsr     w1, wINST, #8               // r1<- AA
    FETCH_ADVANCE_INST 2                // advance rPC, load rINST
    GET_INST_OPCODE ip                  // extract opcode from rINST
    SET_VREG_FLOAT s0, w1
    GOTO_OPCODE ip                      // jump to next instruction

%def fbinopWide(instr="fadd d0, d1, d2", result="d0", r1="d1", r2="d2"):
    /*
     * Generic 64-bit floating-point operation.
     */
    /* binop vAA, vBB, vCC */
    FETCH w0, 1                         // w0<- CCBB
    lsr     w4, wINST, #8               // w4<- AA
    lsr     w2, w0, #8                  // w2<- CC
    and     w1, w0, #255                // w1<- BB
    GET_VREG_DOUBLE $r2, w2             // w2<- vCC
    GET_VREG_DOUBLE $r1, w1             // w1<- vBB
    FETCH_ADVANCE_INST 2                // advance rPC, load rINST
    $instr                              // $result<- op, w0-w4 changed
    GET_INST_OPCODE ip                  // extract opcode from rINST
    SET_VREG_DOUBLE $result, w4         // vAA<- $result
    GOTO_OPCODE ip                      // jump to next instruction

%def fbinop2addr(instr=""):
    /*
     * Generic 32-bit floating point "/2addr" binary operation.  Provide
     * an "instr" line that specifies an instruction that performs
     * "s2 = s0 op s1".
     *
     * For: add-float/2addr, sub-float/2addr, mul-float/2addr, div-float/2addr
     */
    /* binop/2addr vA, vB */
    lsr     w3, wINST, #12              // w3<- B
    ubfx    w9, wINST, #8, #4           // w9<- A
    GET_VREG s1, w3
    GET_VREG s0, w9
    $instr                              // s2<- op
    FETCH_ADVANCE_INST 1                // advance rPC, load rINST
    GET_INST_OPCODE ip                  // extract opcode from rINST
    SET_VREG_FLOAT s2, w9
    GOTO_OPCODE ip                      // jump to next instruction

%def fbinopWide2addr(instr="fadd d0, d0, d1", r0="d0", r1="d1"):
    /*
     * Generic 64-bit floating point "/2addr" binary operation.
     */
    /* binop/2addr vA, vB */
    lsr     w1, wINST, #12              // w1<- B
    ubfx    w2, wINST, #8, #4           // w2<- A
    GET_VREG_DOUBLE $r1, w1             // x1<- vB
    GET_VREG_DOUBLE $r0, w2             // x0<- vA
    FETCH_ADVANCE_INST 1                // advance rPC, load rINST
    $instr                              // result<- op
    GET_INST_OPCODE ip                  // extract opcode from rINST
    SET_VREG_DOUBLE $r0, w2             // vAA<- result
    GOTO_OPCODE ip                      // jump to next instruction

%def fcmp(wide="", r1="s1", r2="s2", cond="lt"):
    /*
     * Compare two floating-point values.  Puts 0, 1, or -1 into the
     * destination register based on the results of the comparison.
     */
    /* op vAA, vBB, vCC */
    FETCH w0, 1                         // w0<- CCBB
    lsr     w4, wINST, #8               // w4<- AA
    and     w2, w0, #255                // w2<- BB
    lsr     w3, w0, #8                  // w3<- CC
%  if r1.startswith("d"):
    GET_VREG_DOUBLE $r1, w2
    GET_VREG_DOUBLE $r2, w3
%  else:
    GET_VREG $r1, w2
    GET_VREG $r2, w3
%  #endif
    fcmp $r1, $r2
    cset w0, ne
    cneg w0, w0, $cond
    FETCH_ADVANCE_INST 2                // advance rPC, load rINST
    GET_INST_OPCODE ip                  // extract opcode from rINST
    SET_VREG w0, w4                     // vAA<- w0
    GOTO_OPCODE ip                      // jump to next instruction

%def funopNarrow(srcreg="s0", tgtreg="d0", instr=""):
    /*
     * Generic 32bit-to-32bit floating point unary operation.  Provide an
     * "instr" line that specifies an instruction that performs "$tgtreg = op $srcreg".
     *
     * For: int-to-float, float-to-int
     * TODO: refactor all of the conversions - parameterize width and use same template.
     */
    /* unop vA, vB */
    lsr     w3, wINST, #12              // w3<- B
    ubfx    w4, wINST, #8, #4           // w4<- A
    GET_VREG $srcreg, w3
    FETCH_ADVANCE_INST 1                // advance rPC, load wINST
    $instr                              // d0<- op
    GET_INST_OPCODE ip                  // extract opcode from wINST
    SET_VREG_FLOAT $tgtreg, w4          // vA<- d0
    GOTO_OPCODE ip                      // jump to next instruction

%def funopNarrower(srcreg="s0", tgtreg="d0", instr=""):
    /*
     * Generic 64bit-to-32bit floating point unary operation.  Provide an
     * "instr" line that specifies an instruction that performs "$tgtreg = op $srcreg".
     *
     * For: int-to-double, float-to-double, float-to-long
     */
    /* unop vA, vB */
    lsr     w3, wINST, #12              // w3<- B
    ubfx    w4, wINST, #8, #4           // w4<- A
%  if srcreg.startswith("d"):
    GET_VREG_DOUBLE $srcreg, w3
%  else:
    GET_VREG_WIDE $srcreg, w3
%  #endif
    FETCH_ADVANCE_INST 1                // advance rPC, load wINST
    $instr                              // d0<- op
    GET_INST_OPCODE ip                  // extract opcode from wINST
    SET_VREG_FLOAT $tgtreg, w4          // vA<- d0
    GOTO_OPCODE ip                      // jump to next instruction

%def funopWide(srcreg="s0", tgtreg="d0", instr=""):
    /*
     * Generic 64bit-to-64bit floating point unary operation.  Provide an
     * "instr" line that specifies an instruction that performs "$tgtreg = op $srcreg".
     *
     * For: long-to-double, double-to-long
     */
    /* unop vA, vB */
    lsr     w3, wINST, #12              // w3<- B
    ubfx    w4, wINST, #8, #4           // w4<- A
%  if srcreg.startswith("d"):
    GET_VREG_DOUBLE $srcreg, w3
%  else:
    GET_VREG_WIDE $srcreg, w3
%  #endif
    FETCH_ADVANCE_INST 1                // advance rPC, load wINST
    $instr                              // d0<- op
    GET_INST_OPCODE ip                  // extract opcode from wINST
%  if tgtreg.startswith("d"):
    SET_VREG_DOUBLE $tgtreg, w4         // vA<- d0
%  else:
    SET_VREG_WIDE $tgtreg, w4           // vA<- d0
%  #endif
    GOTO_OPCODE ip                      // jump to next instruction

%def funopWider(srcreg="s0", tgtreg="d0", instr=""):
    /*
     * Generic 32bit-to-64bit floating point unary operation.  Provide an
     * "instr" line that specifies an instruction that performs "$tgtreg = op $srcreg".
     *
     * For: int-to-double, float-to-double, float-to-long
     */
    /* unop vA, vB */
    lsr     w3, wINST, #12              // w3<- B
    ubfx    w4, wINST, #8, #4           // w4<- A
    GET_VREG $srcreg, w3
    FETCH_ADVANCE_INST 1                // advance rPC, load wINST
    $instr                              // d0<- op
    GET_INST_OPCODE ip                  // extract opcode from wINST
    SET_VREG_WIDE $tgtreg, w4           // vA<- d0
    GOTO_OPCODE ip                      // jump to next instruction

%def op_add_double():
%  fbinopWide(instr="fadd d0, d1, d2", result="d0", r1="d1", r2="d2")

%def op_add_double_2addr():
%  fbinopWide2addr(instr="fadd     d0, d0, d1", r0="d0", r1="d1")

%def op_add_float():
%  fbinop(instr="fadd   s0, s0, s1")

%def op_add_float_2addr():
%  fbinop2addr(instr="fadd   s2, s0, s1")

%def op_cmpg_double():
%  fcmp(wide="_WIDE", r1="d1", r2="d2", cond="cc")

%def op_cmpg_float():
%  fcmp(wide="", r1="s1", r2="s2", cond="cc")

%def op_cmpl_double():
%  fcmp(wide="_WIDE", r1="d1", r2="d2", cond="lt")

%def op_cmpl_float():
%  fcmp(wide="", r1="s1", r2="s2", cond="lt")

%def op_div_double():
%  fbinopWide(instr="fdiv d0, d1, d2", result="d0", r1="d1", r2="d2")

%def op_div_double_2addr():
%  fbinopWide2addr(instr="fdiv     d0, d0, d1", r0="d0", r1="d1")

%def op_div_float():
%  fbinop(instr="fdiv   s0, s0, s1")

%def op_div_float_2addr():
%  fbinop2addr(instr="fdiv   s2, s0, s1")

%def op_double_to_float():
%  funopNarrower(instr="fcvt s0, d0", srcreg="d0", tgtreg="s0")

%def op_double_to_int():
%  funopNarrower(instr="fcvtzs w0, d0", srcreg="d0", tgtreg="w0")

%def op_double_to_long():
%  funopWide(instr="fcvtzs x0, d0", srcreg="d0", tgtreg="x0")

%def op_float_to_double():
%  funopWider(instr="fcvt  d0, s0", srcreg="s0", tgtreg="d0")

%def op_float_to_int():
%  funopNarrow(instr="fcvtzs w0, s0", srcreg="s0", tgtreg="w0")

%def op_float_to_long():
%  funopWider(instr="fcvtzs x0, s0", srcreg="s0", tgtreg="x0")

%def op_int_to_double():
%  funopWider(instr="scvtf d0, w0", srcreg="w0", tgtreg="d0")

%def op_int_to_float():
%  funopNarrow(instr="scvtf s0, w0", srcreg="w0", tgtreg="s0")

%def op_long_to_double():
%  funopWide(instr="scvtf d0, x0", srcreg="x0", tgtreg="d0")

%def op_long_to_float():
%  funopNarrower(instr="scvtf s0, x0", srcreg="x0", tgtreg="s0")

%def op_mul_double():
%  fbinopWide(instr="fmul d0, d1, d2", result="d0", r1="d1", r2="d2")

%def op_mul_double_2addr():
%  fbinopWide2addr(instr="fmul     d0, d0, d1", r0="d0", r1="d1")

%def op_mul_float():
%  fbinop(instr="fmul   s0, s0, s1")

%def op_mul_float_2addr():
%  fbinop2addr(instr="fmul   s2, s0, s1")

%def op_neg_double():
%  unopWide(instr="eor     x0, x0, #0x8000000000000000")

%def op_neg_float():
%  unop(instr="eor     w0, w0, #0x80000000")

%def op_rem_double():
    /* rem vAA, vBB, vCC */
    FETCH w0, 1                         // w0<- CCBB
    lsr     w2, w0, #8                  // w2<- CC
    and     w1, w0, #255                // w1<- BB
    GET_VREG_DOUBLE d1, w2              // d1<- vCC
    GET_VREG_DOUBLE d0, w1              // d0<- vBB
    bl  fmod
    lsr     w4, wINST, #8               // w4<- AA
    FETCH_ADVANCE_INST 2                // advance rPC, load rINST
    GET_INST_OPCODE ip                  // extract opcode from rINST
    SET_VREG_WIDE d0, w4                // vAA<- result
    GOTO_OPCODE ip                      // jump to next instruction
    /* 11-14 instructions */

%def op_rem_double_2addr():
    /* rem vA, vB */
    lsr     w1, wINST, #12              // w1<- B
    ubfx    w2, wINST, #8, #4           // w2<- A
    GET_VREG_DOUBLE d1, w1              // d1<- vB
    GET_VREG_DOUBLE d0, w2              // d0<- vA
    bl fmod
    ubfx    w2, wINST, #8, #4           // w2<- A (need to reload - killed across call)
    FETCH_ADVANCE_INST 1                // advance rPC, load rINST
    GET_INST_OPCODE ip                  // extract opcode from rINST
    SET_VREG_WIDE d0, w2                // vAA<- result
    GOTO_OPCODE ip                      // jump to next instruction
    /* 10-13 instructions */

%def op_rem_float():
/* EABI doesn't define a float remainder function, but libm does */
%  fbinop(instr="bl      fmodf")

%def op_rem_float_2addr():
    /* rem vA, vB */
    lsr     w3, wINST, #12              // w3<- B
    ubfx    w9, wINST, #8, #4           // w9<- A
    GET_VREG s1, w3
    GET_VREG s0, w9
    bl  fmodf
    ubfx    w9, wINST, #8, #4           // w9<- A
    FETCH_ADVANCE_INST 1                // advance rPC, load rINST
    GET_INST_OPCODE ip                  // extract opcode from rINST
    SET_VREG_FLOAT s0, w9
    GOTO_OPCODE ip                      // jump to next instruction

%def op_sub_double():
%  fbinopWide(instr="fsub d0, d1, d2", result="d0", r1="d1", r2="d2")

%def op_sub_double_2addr():
%  fbinopWide2addr(instr="fsub     d0, d0, d1", r0="d0", r1="d1")

%def op_sub_float():
%  fbinop(instr="fsub   s0, s0, s1")

%def op_sub_float_2addr():
%  fbinop2addr(instr="fsub   s2, s0, s1")
