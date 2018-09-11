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
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    srl       a3, a0, 8                    #  a3 <- CC
    and       a2, a0, 255                  #  a2 <- BB
    GET_VREG(a1, a3)                       #  a1 <- vCC
    GET_VREG(a0, a2)                       #  a0 <- vBB
    .if $chkzero
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    $preinstr                              #  optional op
    $instr                                 #  $result <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO($result, rOBJ, t0)       #  vAA <- $result

%def binop2addr(preinstr="", result="a0", chkzero="0", instr=""):
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG(a0, rOBJ)                     #  a0 <- vA
    GET_VREG(a1, a3)                       #  a1 <- vB
    .if $chkzero
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

    $preinstr                              #  optional op
    $instr                                 #  $result <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO($result, rOBJ, t0)       #  vA <- $result

%def binopLit16(preinstr="", result="a0", chkzero="0", instr=""):
    /*
     * Generic 32-bit "lit16" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit16, rsub-int, mul-int/lit16, div-int/lit16,
     *      rem-int/lit16, and-int/lit16, or-int/lit16, xor-int/lit16
     */
    /* binop/lit16 vA, vB, +CCCC */
    FETCH_S(a1, 1)                         #  a1 <- ssssCCCC (sign-extended)
    GET_OPB(a2)                            #  a2 <- B
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_VREG(a0, a2)                       #  a0 <- vB
    .if $chkzero
    # cmp a1, 0; is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

    $preinstr                              #  optional op
    $instr                                 #  $result <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO($result, rOBJ, t0)       #  vA <- $result

%def binopLit8(preinstr="", result="a0", chkzero="0", instr=""):
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    /* binop/lit8 vAA, vBB, +CC */
    FETCH_S(a3, 1)                         #  a3 <- ssssCCBB (sign-extended for CC)
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a3, 255                  #  a2 <- BB
    GET_VREG(a0, a2)                       #  a0 <- vBB
    sra       a1, a3, 8                    #  a1 <- ssssssCC (sign extended)
    .if $chkzero
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

    $preinstr                              #  optional op
    $instr                                 #  $result <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO($result, rOBJ, t0)       #  vAA <- $result

%def binopWide(preinstr="", result0="a0", result1="a1", chkzero="0", arg0="a0", arg1="a1", arg2="a2", arg3="a3", instr=""):
    /*
     * Generic 64-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0-a1 op a2-a3".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register pair other than a0-a1, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a2-a3).  Useful for integer division and modulus.
     *
     * for: add-long, sub-long, div-long, rem-long, and-long, or-long,
     *      xor-long
     *
     * IMPORTANT: you may specify "chkzero" or "preinstr" but not both.
     */
    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a0, 255                  #  a2 <- BB
    srl       a3, a0, 8                    #  a3 <- CC
    EAS2(a2, rFP, a2)                      #  a2 <- &fp[BB]
    EAS2(t1, rFP, a3)                      #  a3 <- &fp[CC]
    LOAD64($arg0, $arg1, a2)               #  a0/a1 <- vBB/vBB+1
    LOAD64($arg2, $arg3, t1)               #  a2/a3 <- vCC/vCC+1
    .if $chkzero
    or        t0, $arg2, $arg3             #  second arg (a2-a3) is zero?
    beqz      t0, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

    $preinstr                              #  optional op
    $instr                                 #  result <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_GOTO($result0, $result1, rOBJ, t0)   #  vAA/vAA+1 <- $result0/$result1

%def binopWide2addr(preinstr="", result0="a0", result1="a1", chkzero="0", arg0="a0", arg1="a1", arg2="a2", arg3="a3", instr=""):
    /*
     * Generic 64-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0-a1 op a2-a3".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register pair other than a0-a1, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vB (a2-a3).  Useful for integer division and modulus.
     *
     * For: add-long/2addr, sub-long/2addr, div-long/2addr, rem-long/2addr,
     *      and-long/2addr, or-long/2addr, xor-long/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a1)                            #  a1 <- B
    EAS2(a1, rFP, a1)                      #  a1 <- &fp[B]
    EAS2(t0, rFP, rOBJ)                    #  t0 <- &fp[A]
    LOAD64($arg2, $arg3, a1)               #  a2/a3 <- vB/vB+1
    LOAD64($arg0, $arg1, t0)               #  a0/a1 <- vA/vA+1
    .if $chkzero
    or        t0, $arg2, $arg3             #  second arg (a2-a3) is zero?
    beqz      t0, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

    $preinstr                              #  optional op
    $instr                                 #  result <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_GOTO($result0, $result1, rOBJ, t0)   #  vA/vA+1 <- $result0/$result1

%def unop(preinstr="", result0="a0", instr=""):
    /*
     * Generic 32-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result0 = op a0".
     * This could be a MIPS instruction or a function call.
     *
     * for: int-to-byte, int-to-char, int-to-short,
     *      neg-int, not-int, neg-float
     */
    /* unop vA, vB */
    GET_OPB(a3)                            #  a3 <- B
    GET_OPA4(t0)                           #  t0 <- A+
    GET_VREG(a0, a3)                       #  a0 <- vB
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    $preinstr                              #  optional op
    $instr                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t1)                    #  extract opcode from rINST
    SET_VREG_GOTO($result0, t0, t1)        #  vA <- result0

%def unopNarrower(load="LOAD64_F(fa0, fa0f, a3)", instr=""):
    /*
     * Generic 64bit-to-32bit floating-point unary operation.  Provide an "instr"
     * line that specifies an instruction that performs "fv0 = op fa0".
     *
     * For: double-to-float
     */
    /* unop vA, vB */
    GET_OPB(a3)                            #  a3 <- B
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[B]
    $load
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    $instr
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_F_GOTO(fv0, rOBJ, t0)         #  vA <- fv0

%def unopWide(preinstr="", result0="a0", result1="a1", instr=""):
    /*
     * Generic 64-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result0/result1 = op a0/a1".
     * This could be MIPS instruction or a function call.
     *
     * For: neg-long, not-long, neg-double,
     */
    /* unop vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[B]
    LOAD64(a0, a1, a3)                     #  a0/a1 <- vA
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    $preinstr                              #  optional op
    $instr                                 #  a0/a1 <- op, a2-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_GOTO($result0, $result1, rOBJ, t0)   #  vA/vA+1 <- a0/a1

%def unopWider(preinstr="", result0="a0", result1="a1", instr=""):
    /*
     * Generic 32bit-to-64bit unary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result0/result1 = op a0".
     *
     * For: int-to-long
     */
    /* unop vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG(a0, a3)                       #  a0 <- vB
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    $preinstr                              #  optional op
    $instr                                 #  result <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_GOTO($result0, $result1, rOBJ, t0)   #  vA/vA+1 <- a0/a1

%def op_add_int():
%  binop(instr="addu a0, a0, a1")

%def op_add_int_2addr():
%  binop2addr(instr="addu a0, a0, a1")

%def op_add_int_lit16():
%  binopLit16(instr="addu a0, a0, a1")

%def op_add_int_lit8():
%  binopLit8(instr="addu a0, a0, a1")

%def op_add_long():
/*
 *  The compiler generates the following sequence for
 *  [v1 v0] =  [a1 a0] + [a3 a2];
 *    addu v0,a2,a0
 *    addu a1,a3,a1
 *    sltu v1,v0,a2
 *    addu v1,v1,a1
 */
%  binopWide(result0="v0", result1="v1", preinstr="addu v0, a2, a0", instr="addu a1, a3, a1; sltu v1, v0, a2; addu v1, v1, a1")

%def op_add_long_2addr():
/*
 * See op_add_long.S for details
 */
%  binopWide2addr(result0="v0", result1="v1", preinstr="addu v0, a2, a0", instr="addu a1, a3, a1; sltu v1, v0, a2; addu v1, v1, a1")

%def op_and_int():
%  binop(instr="and a0, a0, a1")

%def op_and_int_2addr():
%  binop2addr(instr="and a0, a0, a1")

%def op_and_int_lit16():
%  binopLit16(instr="and a0, a0, a1")

%def op_and_int_lit8():
%  binopLit8(instr="and a0, a0, a1")

%def op_and_long():
%  binopWide(preinstr="and a0, a0, a2", instr="and a1, a1, a3")

%def op_and_long_2addr():
%  binopWide2addr(preinstr="and a0, a0, a2", instr="and a1, a1, a3")

%def op_cmp_long():
    /*
     * Compare two 64-bit values
     *    x = y     return  0
     *    x < y     return -1
     *    x > y     return  1
     *
     * I think I can improve on the ARM code by the following observation
     *    slt   t0,  x.hi, y.hi;        # (x.hi < y.hi) ? 1:0
     *    sgt   t1,  x.hi, y.hi;        # (y.hi > x.hi) ? 1:0
     *    subu  v0, t0, t1              # v0= -1:1:0 for [ < > = ]
     */
    /* cmp-long vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a0, 255                  #  a2 <- BB
    srl       a3, a0, 8                    #  a3 <- CC
    EAS2(a2, rFP, a2)                      #  a2 <- &fp[BB]
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[CC]
    LOAD64(a0, a1, a2)                     #  a0/a1 <- vBB/vBB+1
    LOAD64(a2, a3, a3)                     #  a2/a3 <- vCC/vCC+1

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    slt       t0, a1, a3                   #  compare hi
    sgt       t1, a1, a3
    subu      v0, t1, t0                   #  v0 <- (-1, 1, 0)
    bnez      v0, .L${opcode}_finish
    # at this point x.hi==y.hi
    sltu      t0, a0, a2                   #  compare lo
    sgtu      t1, a0, a2
    subu      v0, t1, t0                   #  v0 <- (-1, 1, 0) for [< > =]

.L${opcode}_finish:
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(v0, rOBJ, t0)            #  vAA <- v0

%def op_div_int():
#ifdef MIPS32REVGE6
%  binop(instr="div a0, a0, a1", chkzero="1")
#else
%  binop(preinstr="div zero, a0, a1", instr="mflo a0", chkzero="1")
#endif

%def op_div_int_2addr():
#ifdef MIPS32REVGE6
%  binop2addr(instr="div a0, a0, a1", chkzero="1")
#else
%  binop2addr(preinstr="div zero, a0, a1", instr="mflo a0", chkzero="1")
#endif

%def op_div_int_lit16():
#ifdef MIPS32REVGE6
%  binopLit16(instr="div a0, a0, a1", chkzero="1")
#else
%  binopLit16(preinstr="div zero, a0, a1", instr="mflo a0", chkzero="1")
#endif

%def op_div_int_lit8():
#ifdef MIPS32REVGE6
%  binopLit8(instr="div a0, a0, a1", chkzero="1")
#else
%  binopLit8(preinstr="div zero, a0, a1", instr="mflo a0", chkzero="1")
#endif

%def op_div_long():
%  binopWide(result0="v0", result1="v1", instr="JAL(__divdi3)", chkzero="1")

%def op_div_long_2addr():
%  binopWide2addr(result0="v0", result1="v1", instr="JAL(__divdi3)", chkzero="1")

%def op_int_to_byte():
%  unop(instr="SEB(a0, a0)")

%def op_int_to_char():
%  unop(preinstr="", instr="and a0, 0xffff")

%def op_int_to_long():
%  unopWider(instr="sra a1, a0, 31")

%def op_int_to_short():
%  unop(instr="SEH(a0, a0)")

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
    /*
     * Signed 64-bit integer multiply.
     *         a1   a0
     *   x     a3   a2
     *   -------------
     *       a2a1 a2a0
     *       a3a0
     *  a3a1 (<= unused)
     *  ---------------
     *         v1   v0
     */
    /* mul-long vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    and       t0, a0, 255                  #  a2 <- BB
    srl       t1, a0, 8                    #  a3 <- CC
    EAS2(t0, rFP, t0)                      #  t0 <- &fp[BB]
    LOAD64(a0, a1, t0)                     #  a0/a1 <- vBB/vBB+1

    EAS2(t1, rFP, t1)                      #  t0 <- &fp[CC]
    LOAD64(a2, a3, t1)                     #  a2/a3 <- vCC/vCC+1

    mul       v1, a3, a0                   #  v1= a3a0
#ifdef MIPS32REVGE6
    mulu      v0, a2, a0                   #  v0= a2a0
    muhu      t1, a2, a0
#else
    multu     a2, a0
    mfhi      t1
    mflo      v0                           #  v0= a2a0
#endif
    mul       t0, a2, a1                   #  t0= a2a1
    addu      v1, v1, t1                   #  v1+= hi(a2a0)
    addu      v1, v1, t0                   #  v1= a3a0 + a2a1;

    GET_OPA(a0)                            #  a0 <- AA
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    b         .L${opcode}_finish
%def op_mul_long_helper_code():

.Lop_mul_long_finish:
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_GOTO(v0, v1, a0, t0)        #  vAA/vAA+1 <- v0(low)/v1(high)

%def op_mul_long_2addr():
    /*
     * See op_mul_long.S for more details
     */
    /* mul-long/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+

    EAS2(t0, rFP, rOBJ)                    #  t0 <- &fp[A]
    LOAD64(a0, a1, t0)                     #  vAA.low / high

    GET_OPB(t1)                            #  t1 <- B
    EAS2(t1, rFP, t1)                      #  t1 <- &fp[B]
    LOAD64(a2, a3, t1)                     #  vBB.low / high

    mul       v1, a3, a0                   #  v1= a3a0
#ifdef MIPS32REVGE6
    mulu      v0, a2, a0                   #  v0= a2a0
    muhu      t1, a2, a0
#else
    multu     a2, a0
    mfhi      t1
    mflo      v0                           #  v0= a2a0
 #endif
    mul       t2, a2, a1                   #  t2= a2a1
    addu      v1, v1, t1                   #  v1= a3a0 + hi(a2a0)
    addu      v1, v1, t2                   #  v1= v1 + a2a1;

    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t1)                    #  extract opcode from rINST
    SET_VREG64_GOTO(v0, v1, rOBJ, t1)      #  vA/vA+1 <- v0(low)/v1(high)

%def op_neg_int():
%  unop(instr="negu a0, a0")

%def op_neg_long():
%  unopWide(result0="v0", result1="v1", preinstr="negu v0, a0", instr="negu v1, a1; sltu a0, zero, v0; subu v1, v1, a0")

%def op_not_int():
%  unop(instr="not a0, a0")

%def op_not_long():
%  unopWide(preinstr="not a0, a0", instr="not a1, a1")

%def op_or_int():
%  binop(instr="or a0, a0, a1")

%def op_or_int_2addr():
%  binop2addr(instr="or a0, a0, a1")

%def op_or_int_lit16():
%  binopLit16(instr="or a0, a0, a1")

%def op_or_int_lit8():
%  binopLit8(instr="or a0, a0, a1")

%def op_or_long():
%  binopWide(preinstr="or a0, a0, a2", instr="or a1, a1, a3")

%def op_or_long_2addr():
%  binopWide2addr(preinstr="or a0, a0, a2", instr="or a1, a1, a3")

%def op_rem_int():
#ifdef MIPS32REVGE6
%  binop(instr="mod a0, a0, a1", chkzero="1")
#else
%  binop(preinstr="div zero, a0, a1", instr="mfhi a0", chkzero="1")
#endif

%def op_rem_int_2addr():
#ifdef MIPS32REVGE6
%  binop2addr(instr="mod a0, a0, a1", chkzero="1")
#else
%  binop2addr(preinstr="div zero, a0, a1", instr="mfhi a0", chkzero="1")
#endif

%def op_rem_int_lit16():
#ifdef MIPS32REVGE6
%  binopLit16(instr="mod a0, a0, a1", chkzero="1")
#else
%  binopLit16(preinstr="div zero, a0, a1", instr="mfhi a0", chkzero="1")
#endif

%def op_rem_int_lit8():
#ifdef MIPS32REVGE6
%  binopLit8(instr="mod a0, a0, a1", chkzero="1")
#else
%  binopLit8(preinstr="div zero, a0, a1", instr="mfhi a0", chkzero="1")
#endif

%def op_rem_long():
%  binopWide(result0="v0", result1="v1", instr="JAL(__moddi3)", chkzero="1")

%def op_rem_long_2addr():
%  binopWide2addr(result0="v0", result1="v1", instr="JAL(__moddi3)", chkzero="1")

%def op_rsub_int():
/* this op is "rsub-int", but can be thought of as "rsub-int/lit16" */
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
    /*
     * Long integer shift.  This is different from the generic 32/64-bit
     * binary operations because vAA/vBB are 64-bit but vCC (the shift
     * distance) is 32-bit.  Also, Dalvik requires us to mask off the low
     * 6 bits of the shift distance.
     */
    /* shl-long vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(t2)                            #  t2 <- AA
    and       a3, a0, 255                  #  a3 <- BB
    srl       a0, a0, 8                    #  a0 <- CC
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[BB]
    GET_VREG(a2, a0)                       #  a2 <- vCC
    LOAD64(a0, a1, a3)                     #  a0/a1 <- vBB/vBB+1

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST

    andi    v1, a2, 0x20                   #  shift< shift & 0x20
    sll     v0, a0, a2                     #  rlo<- alo << (shift&31)
    bnez    v1, .L${opcode}_finish
    not     v1, a2                         #  rhi<- 31-shift  (shift is 5b)
    srl     a0, 1
    srl     a0, v1                         #  alo<- alo >> (32-(shift&31))
    sll     v1, a1, a2                     #  rhi<- ahi << (shift&31)
    or      v1, a0                         #  rhi<- rhi | alo
    SET_VREG64_GOTO(v0, v1, t2, t0)        #  vAA/vAA+1 <- v0/v1
%def op_shl_long_helper_code():

.Lop_shl_long_finish:
    SET_VREG64_GOTO(zero, v0, t2, t0)      #  vAA/vAA+1 <- rlo/rhi

%def op_shl_long_2addr():
    /*
     * Long integer shift, 2addr version.  vA is 64-bit value/result, vB is
     * 32-bit shift distance.
     */
    /* shl-long/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG(a2, a3)                       #  a2 <- vB
    EAS2(t2, rFP, rOBJ)                    #  t2 <- &fp[A]
    LOAD64(a0, a1, t2)                     #  a0/a1 <- vA/vA+1

    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST

    andi    v1, a2, 0x20                   #  shift< shift & 0x20
    sll     v0, a0, a2                     #  rlo<- alo << (shift&31)
    bnez    v1, .L${opcode}_finish
    not     v1, a2                         #  rhi<- 31-shift  (shift is 5b)
    srl     a0, 1
    srl     a0, v1                         #  alo<- alo >> (32-(shift&31))
    sll     v1, a1, a2                     #  rhi<- ahi << (shift&31)
    or      v1, a0                         #  rhi<- rhi | alo
    SET_VREG64_GOTO(v0, v1, rOBJ, t0)      #  vA/vA+1 <- v0/v1
%def op_shl_long_2addr_helper_code():

.Lop_shl_long_2addr_finish:
    SET_VREG64_GOTO(zero, v0, rOBJ, t0)    #  vA/vA+1 <- rlo/rhi

%def op_shr_int():
%  binop(instr="sra a0, a0, a1")

%def op_shr_int_2addr():
%  binop2addr(instr="sra a0, a0, a1")

%def op_shr_int_lit8():
%  binopLit8(instr="sra a0, a0, a1")

%def op_shr_long():
    /*
     * Long integer shift.  This is different from the generic 32/64-bit
     * binary operations because vAA/vBB are 64-bit but vCC (the shift
     * distance) is 32-bit.  Also, Dalvik requires us to mask off the low
     * 6 bits of the shift distance.
     */
    /* shr-long vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(t3)                            #  t3 <- AA
    and       a3, a0, 255                  #  a3 <- BB
    srl       a0, a0, 8                    #  a0 <- CC
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[BB]
    GET_VREG(a2, a0)                       #  a2 <- vCC
    LOAD64(a0, a1, a3)                     #  a0/a1 <- vBB/vBB+1
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST

    andi    v0, a2, 0x20                   #  shift & 0x20
    sra     v1, a1, a2                     #  rhi<- ahi >> (shift&31)
    bnez    v0, .L${opcode}_finish
    srl     v0, a0, a2                     #  rlo<- alo >> (shift&31)
    not     a0, a2                         #  alo<- 31-shift (shift is 5b)
    sll     a1, 1
    sll     a1, a0                         #  ahi<- ahi << (32-(shift&31))
    or      v0, a1                         #  rlo<- rlo | ahi
    SET_VREG64_GOTO(v0, v1, t3, t0)        #  vAA/VAA+1 <- v0/v1
%def op_shr_long_helper_code():

.Lop_shr_long_finish:
    sra     a3, a1, 31                     #  a3<- sign(ah)
    SET_VREG64_GOTO(v1, a3, t3, t0)        #  vAA/VAA+1 <- rlo/rhi

%def op_shr_long_2addr():
    /*
     * Long integer shift, 2addr version.  vA is 64-bit value/result, vB is
     * 32-bit shift distance.
     */
    /* shr-long/2addr vA, vB */
    GET_OPA4(t2)                           #  t2 <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG(a2, a3)                       #  a2 <- vB
    EAS2(t0, rFP, t2)                      #  t0 <- &fp[A]
    LOAD64(a0, a1, t0)                     #  a0/a1 <- vA/vA+1
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST

    andi    v0, a2, 0x20                   #  shift & 0x20
    sra     v1, a1, a2                     #  rhi<- ahi >> (shift&31)
    bnez    v0, .L${opcode}_finish
    srl     v0, a0, a2                     #  rlo<- alo >> (shift&31)
    not     a0, a2                         #  alo<- 31-shift (shift is 5b)
    sll     a1, 1
    sll     a1, a0                         #  ahi<- ahi << (32-(shift&31))
    or      v0, a1                         #  rlo<- rlo | ahi
    SET_VREG64_GOTO(v0, v1, t2, t0)        #  vA/vA+1 <- v0/v1
%def op_shr_long_2addr_helper_code():

.Lop_shr_long_2addr_finish:
    sra     a3, a1, 31                     #  a3<- sign(ah)
    SET_VREG64_GOTO(v1, a3, t2, t0)        #  vA/vA+1 <- rlo/rhi

%def op_sub_int():
%  binop(instr="subu a0, a0, a1")

%def op_sub_int_2addr():
%  binop2addr(instr="subu a0, a0, a1")

%def op_sub_long():
/*
 * For little endian the code sequence looks as follows:
 *    subu    v0,a0,a2
 *    subu    v1,a1,a3
 *    sltu    a0,a0,v0
 *    subu    v1,v1,a0
 */
%  binopWide(result0="v0", result1="v1", preinstr="subu v0, a0, a2", instr="subu v1, a1, a3; sltu a0, a0, v0; subu v1, v1, a0")

%def op_sub_long_2addr():
/*
 * See op_sub_long.S for more details
 */
%  binopWide2addr(result0="v0", result1="v1", preinstr="subu v0, a0, a2", instr="subu v1, a1, a3; sltu a0, a0, v0; subu v1, v1, a0")

%def op_ushr_int():
%  binop(instr="srl a0, a0, a1")

%def op_ushr_int_2addr():
%  binop2addr(instr="srl a0, a0, a1 ")

%def op_ushr_int_lit8():
%  binopLit8(instr="srl a0, a0, a1")

%def op_ushr_long():
    /*
     * Long integer shift.  This is different from the generic 32/64-bit
     * binary operations because vAA/vBB are 64-bit but vCC (the shift
     * distance) is 32-bit.  Also, Dalvik requires us to mask off the low
     * 6 bits of the shift distance.
     */
    /* ushr-long vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a3, a0, 255                  #  a3 <- BB
    srl       a0, a0, 8                    #  a0 <- CC
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[BB]
    GET_VREG(a2, a0)                       #  a2 <- vCC
    LOAD64(a0, a1, a3)                     #  a0/a1 <- vBB/vBB+1

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST

    andi      v0, a2, 0x20                 #  shift & 0x20
    srl       v1, a1, a2                   #  rhi<- ahi >> (shift&31)
    bnez      v0, .L${opcode}_finish
    srl       v0, a0, a2                   #  rlo<- alo >> (shift&31)
    not       a0, a2                       #  alo<- 31-n  (shift is 5b)
    sll       a1, 1
    sll       a1, a0                       #  ahi<- ahi << (32-(shift&31))
    or        v0, a1                       #  rlo<- rlo | ahi
    SET_VREG64_GOTO(v0, v1, rOBJ, t0)      #  vAA/vAA+1 <- v0/v1
%def op_ushr_long_helper_code():

.Lop_ushr_long_finish:
    SET_VREG64_GOTO(v1, zero, rOBJ, t0)    #  vAA/vAA+1 <- rlo/rhi

%def op_ushr_long_2addr():
    /*
     * Long integer shift, 2addr version.  vA is 64-bit value/result, vB is
     * 32-bit shift distance.
     */
    /* ushr-long/2addr vA, vB */
    GET_OPA4(t3)                           #  t3 <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG(a2, a3)                       #  a2 <- vB
    EAS2(t0, rFP, t3)                      #  t0 <- &fp[A]
    LOAD64(a0, a1, t0)                     #  a0/a1 <- vA/vA+1

    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST

    andi      v0, a2, 0x20                 #  shift & 0x20
    srl       v1, a1, a2                   #  rhi<- ahi >> (shift&31)
    bnez      v0, .L${opcode}_finish
    srl       v0, a0, a2                   #  rlo<- alo >> (shift&31)
    not       a0, a2                       #  alo<- 31-n  (shift is 5b)
    sll       a1, 1
    sll       a1, a0                       #  ahi<- ahi << (32-(shift&31))
    or        v0, a1                       #  rlo<- rlo | ahi
    SET_VREG64_GOTO(v0, v1, t3, t0)        #  vA/vA+1 <- v0/v1
%def op_ushr_long_2addr_helper_code():

.Lop_ushr_long_2addr_finish:
    SET_VREG64_GOTO(v1, zero, t3, t0)      #  vA/vA+1 <- rlo/rhi

%def op_xor_int():
%  binop(instr="xor a0, a0, a1")

%def op_xor_int_2addr():
%  binop2addr(instr="xor a0, a0, a1")

%def op_xor_int_lit16():
%  binopLit16(instr="xor a0, a0, a1")

%def op_xor_int_lit8():
%  binopLit8(instr="xor a0, a0, a1")

%def op_xor_long():
%  binopWide(preinstr="xor a0, a0, a2", instr="xor a1, a1, a3")

%def op_xor_long_2addr():
%  binopWide2addr(preinstr="xor a0, a0, a2", instr="xor a1, a1, a3")
