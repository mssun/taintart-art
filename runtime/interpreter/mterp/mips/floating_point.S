%def fbinop(instr=""):
    /*
     * Generic 32-bit binary float operation.
     *
     * For: add-fp, sub-fp, mul-fp, div-fp, rem-fp
     */

    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    srl       a3, a0, 8                    #  a3 <- CC
    and       a2, a0, 255                  #  a2 <- BB
    GET_VREG_F(fa1, a3)                    #  a1 <- vCC
    GET_VREG_F(fa0, a2)                    #  a0 <- vBB

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    $instr                                 #  f0 = result
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_F_GOTO(fv0, rOBJ, t0)         #  vAA <- fv0

%def fbinop2addr(instr=""):
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr"
     * that specifies an instruction that performs "fv0 = fa0 op fa1".
     * This could be an MIPS instruction or a function call.
     *
     * For: add-float/2addr, sub-float/2addr, mul-float/2addr,
     *      div-float/2addr, rem-float/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG_F(fa0, rOBJ)
    GET_VREG_F(fa1, a3)
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

    $instr
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_F_GOTO(fv0, rOBJ, t0)         #  vA <- result

%def fbinopWide(instr=""):
    /*
     * Generic 64-bit floating-point binary operation.  Provide an "instr"
     * line that specifies an instruction that performs "fv0 = fa0 op fa1".
     * This could be an MIPS instruction or a function call.
     *
     * for: add-double, sub-double, mul-double, div-double,
     *      rem-double
     *
     */
    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a0, 255                  #  a2 <- BB
    srl       a3, a0, 8                    #  a3 <- CC
    EAS2(a2, rFP, a2)                      #  a2 <- &fp[BB]
    EAS2(t1, rFP, a3)                      #  a3 <- &fp[CC]
    LOAD64_F(fa0, fa0f, a2)
    LOAD64_F(fa1, fa1f, t1)

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    $instr
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_F_GOTO(fv0, fv0f, rOBJ, t0)  #  vAA/vAA+1 <- fv0

%def fbinopWide2addr(instr=""):
    /*
     * Generic 64-bit floating-point "/2addr" binary operation.
     * Provide an "instr" line that specifies an instruction that
     * performs "fv0 = fa0 op fa1".
     * This could be an MIPS instruction or a function call.
     *
     * For: add-double/2addr, sub-double/2addr, mul-double/2addr,
     *      div-double/2addr, rem-double/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a1)                            #  a1 <- B
    EAS2(a1, rFP, a1)                      #  a1 <- &fp[B]
    EAS2(t0, rFP, rOBJ)                    #  t0 <- &fp[A]
    LOAD64_F(fa0, fa0f, t0)
    LOAD64_F(fa1, fa1f, a1)

    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    $instr
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_F_GOTO(fv0, fv0f, rOBJ, t0)  #  vA/vA+1 <- fv0

%def funop(instr=""):
    /*
     * Generic 32-bit floating-point unary operation.  Provide an "instr"
     * line that specifies an instruction that performs "fv0 = op fa0".
     * This could be a MIPS instruction or a function call.
     *
     * for: int-to-float
     */
    /* unop vA, vB */
    GET_OPB(a3)                            #  a3 <- B
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_VREG_F(fa0, a3)
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    $instr
    GET_INST_OPCODE(t1)                    #  extract opcode from rINST
    SET_VREG_F_GOTO(fv0, rOBJ, t1)         #  vA <- fv0

%def funopWider(instr=""):
    /*
     * Generic 32bit-to-64bit floating-point unary operation.  Provide an "instr"
     * line that specifies an instruction that performs "fv0 = op fa0".
     *
     * For: int-to-double, float-to-double
     */
    /* unop vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG_F(fa0, a3)
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    $instr
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_F_GOTO(fv0, fv0f, rOBJ, t0) #  vA/vA+1 <- fv0

%def op_add_double():
%  fbinopWide(instr="add.d fv0, fa0, fa1")

%def op_add_double_2addr():
%  fbinopWide2addr(instr="add.d fv0, fa0, fa1")

%def op_add_float():
%  fbinop(instr="add.s fv0, fa0, fa1")

%def op_add_float_2addr():
%  fbinop2addr(instr="add.s fv0, fa0, fa1")

%def op_cmpg_double():
%  op_cmpl_double(gt_bias="1")

%def op_cmpg_float():
%  op_cmpl_float(gt_bias="1")

%def op_cmpl_double(gt_bias="0"):
    /*
     * Compare two floating-point values. Puts 0(==), 1(>), or -1(<)
     * into the destination register based on the comparison results.
     *
     * For: cmpl-double, cmpg-double
     */
    /* op vAA, vBB, vCC */

    FETCH(a0, 1)                           #  a0 <- CCBB
    and       rOBJ, a0, 255                #  rOBJ <- BB
    srl       t0, a0, 8                    #  t0 <- CC
    EAS2(rOBJ, rFP, rOBJ)                  #  rOBJ <- &fp[BB]
    EAS2(t0, rFP, t0)                      #  t0 <- &fp[CC]
    LOAD64_F(ft0, ft0f, rOBJ)
    LOAD64_F(ft1, ft1f, t0)
#ifdef MIPS32REVGE6
    cmp.eq.d  ft2, ft0, ft1
    li        rTEMP, 0
    bc1nez    ft2, 1f                      # done if vBB == vCC (ordered)
    .if $gt_bias
    cmp.lt.d  ft2, ft0, ft1
    li        rTEMP, -1
    bc1nez    ft2, 1f                      # done if vBB < vCC (ordered)
    li        rTEMP, 1                     # vBB > vCC or unordered
    .else
    cmp.lt.d  ft2, ft1, ft0
    li        rTEMP, 1
    bc1nez    ft2, 1f                      # done if vBB > vCC (ordered)
    li        rTEMP, -1                    # vBB < vCC or unordered
    .endif
#else
    c.eq.d    fcc0, ft0, ft1
    li        rTEMP, 0
    bc1t      fcc0, 1f                     # done if vBB == vCC (ordered)
    .if $gt_bias
    c.olt.d   fcc0, ft0, ft1
    li        rTEMP, -1
    bc1t      fcc0, 1f                     # done if vBB < vCC (ordered)
    li        rTEMP, 1                     # vBB > vCC or unordered
    .else
    c.olt.d   fcc0, ft1, ft0
    li        rTEMP, 1
    bc1t      fcc0, 1f                     # done if vBB > vCC (ordered)
    li        rTEMP, -1                    # vBB < vCC or unordered
    .endif
#endif
1:
    GET_OPA(rOBJ)
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(rTEMP, rOBJ, t0)         #  vAA <- rTEMP

%def op_cmpl_float(gt_bias="0"):
    /*
     * Compare two floating-point values. Puts 0(==), 1(>), or -1(<)
     * into the destination register based on the comparison results.
     *
     * for: cmpl-float, cmpg-float
     */
    /* op vAA, vBB, vCC */

    FETCH(a0, 1)                           #  a0 <- CCBB
    and       a2, a0, 255                  #  a2 <- BB
    srl       a3, a0, 8
    GET_VREG_F(ft0, a2)
    GET_VREG_F(ft1, a3)
#ifdef MIPS32REVGE6
    cmp.eq.s  ft2, ft0, ft1
    li        rTEMP, 0
    bc1nez    ft2, 1f                      # done if vBB == vCC (ordered)
    .if $gt_bias
    cmp.lt.s  ft2, ft0, ft1
    li        rTEMP, -1
    bc1nez    ft2, 1f                      # done if vBB < vCC (ordered)
    li        rTEMP, 1                     # vBB > vCC or unordered
    .else
    cmp.lt.s  ft2, ft1, ft0
    li        rTEMP, 1
    bc1nez    ft2, 1f                      # done if vBB > vCC (ordered)
    li        rTEMP, -1                    # vBB < vCC or unordered
    .endif
#else
    c.eq.s    fcc0, ft0, ft1
    li        rTEMP, 0
    bc1t      fcc0, 1f                     # done if vBB == vCC (ordered)
    .if $gt_bias
    c.olt.s   fcc0, ft0, ft1
    li        rTEMP, -1
    bc1t      fcc0, 1f                     # done if vBB < vCC (ordered)
    li        rTEMP, 1                     # vBB > vCC or unordered
    .else
    c.olt.s   fcc0, ft1, ft0
    li        rTEMP, 1
    bc1t      fcc0, 1f                     # done if vBB > vCC (ordered)
    li        rTEMP, -1                    # vBB < vCC or unordered
    .endif
#endif
1:
    GET_OPA(rOBJ)
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(rTEMP, rOBJ, t0)         #  vAA <- rTEMP

%def op_div_double():
%  fbinopWide(instr="div.d fv0, fa0, fa1")

%def op_div_double_2addr():
%  fbinopWide2addr(instr="div.d fv0, fa0, fa1")

%def op_div_float():
%  fbinop(instr="div.s fv0, fa0, fa1")

%def op_div_float_2addr():
%  fbinop2addr(instr="div.s fv0, fa0, fa1")

%def op_double_to_float():
%  unopNarrower(instr="cvt.s.d fv0, fa0")

%def op_double_to_int():
    /*
     * double-to-int
     *
     * We have to clip values to int min/max per the specification.  The
     * expected common case is a "reasonable" value that converts directly
     * to modest integer.  The EABI convert function isn't doing this for us
     * for pre-R6.
     */
    /* unop vA, vB */
    GET_OPB(a3)                            #  a3 <- B
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[B]
    LOAD64_F(fa0, fa0f, a3)
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
#ifndef MIPS32REVGE6
    li        t0, INT_MIN_AS_DOUBLE_HIGH
    mtc1      zero, fa1
    MOVE_TO_FPU_HIGH(t0, fa1, fa1f)
    c.ole.d   fcc0, fa1, fa0
#endif
    GET_INST_OPCODE(t1)                    #  extract opcode from rINST
#ifndef MIPS32REVGE6
    bc1t      fcc0, 1f                     #  if INT_MIN <= vB, proceed to truncation
    c.eq.d    fcc0, fa0, fa0
    mtc1      zero, fa0
    MOVE_TO_FPU_HIGH(zero, fa0, fa0f)
    movt.d    fa0, fa1, fcc0               #  fa0 = ordered(vB) ? INT_MIN_AS_DOUBLE : 0
1:
#endif
    trunc.w.d fa0, fa0
    SET_VREG_F_GOTO(fa0, rOBJ, t1)         #  vA <- result

%def op_double_to_long():
    /*
     * double-to-long
     *
     * We have to clip values to long min/max per the specification.  The
     * expected common case is a "reasonable" value that converts directly
     * to modest integer.  The EABI convert function isn't doing this for us
     * for pre-R6.
     */
    /* unop vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[B]
    LOAD64_F(fa0, fa0f, a3)
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

#ifdef MIPS32REVGE6
    GET_INST_OPCODE(t1)                    #  extract opcode from rINST
    trunc.l.d fa0, fa0
    SET_VREG64_F_GOTO(fa0, fa0f, rOBJ, t1) #  vA <- result
#else
    c.eq.d    fcc0, fa0, fa0
    li        rRESULT0, 0
    li        rRESULT1, 0
    bc1f      fcc0, .L${opcode}_get_opcode

    li        t0, LONG_MIN_AS_DOUBLE_HIGH
    mtc1      zero, fa1
    MOVE_TO_FPU_HIGH(t0, fa1, fa1f)
    c.ole.d   fcc0, fa0, fa1
    li        rRESULT1, LONG_MIN_HIGH
    bc1t      fcc0, .L${opcode}_get_opcode

    neg.d     fa1, fa1
    c.ole.d   fcc0, fa1, fa0
    nor       rRESULT0, rRESULT0, zero
    nor       rRESULT1, rRESULT1, zero
    bc1t      fcc0, .L${opcode}_get_opcode

    JAL(__fixdfdi)
    GET_INST_OPCODE(t1)                    #  extract opcode from rINST
    b         .L${opcode}_set_vreg
#endif
%def op_double_to_long_helper_code():

#ifndef MIPS32REVGE6
.Lop_double_to_long_get_opcode:
    GET_INST_OPCODE(t1)                    #  extract opcode from rINST
.Lop_double_to_long_set_vreg:
    SET_VREG64_GOTO(rRESULT0, rRESULT1, rOBJ, t1)   #  vA/vA+1 <- v0/v1
#endif

%def op_float_to_double():
%  funopWider(instr="cvt.d.s fv0, fa0")

%def op_float_to_int():
    /*
     * float-to-int
     *
     * We have to clip values to int min/max per the specification.  The
     * expected common case is a "reasonable" value that converts directly
     * to modest integer.  The EABI convert function isn't doing this for us
     * for pre-R6.
     */
    /* unop vA, vB */
    GET_OPB(a3)                            #  a3 <- B
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_VREG_F(fa0, a3)
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

#ifndef MIPS32REVGE6
    li        t0, INT_MIN_AS_FLOAT
    mtc1      t0, fa1
    c.ole.s   fcc0, fa1, fa0
#endif
    GET_INST_OPCODE(t1)                    #  extract opcode from rINST
#ifndef MIPS32REVGE6
    bc1t      fcc0, 1f                     #  if INT_MIN <= vB, proceed to truncation
    c.eq.s    fcc0, fa0, fa0
    mtc1      zero, fa0
    movt.s    fa0, fa1, fcc0               #  fa0 = ordered(vB) ? INT_MIN_AS_FLOAT : 0
1:
#endif
    trunc.w.s fa0, fa0
    SET_VREG_F_GOTO(fa0, rOBJ, t1)         #  vA <- result

%def op_float_to_long():
    /*
     * float-to-long
     *
     * We have to clip values to long min/max per the specification.  The
     * expected common case is a "reasonable" value that converts directly
     * to modest integer.  The EABI convert function isn't doing this for us
     * for pre-R6.
     */
    /* unop vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG_F(fa0, a3)
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

#ifdef MIPS32REVGE6
    GET_INST_OPCODE(t1)                    #  extract opcode from rINST
    trunc.l.s fa0, fa0
    SET_VREG64_F_GOTO(fa0, fa0f, rOBJ, t1) #  vA <- result
#else
    c.eq.s    fcc0, fa0, fa0
    li        rRESULT0, 0
    li        rRESULT1, 0
    bc1f      fcc0, .L${opcode}_get_opcode

    li        t0, LONG_MIN_AS_FLOAT
    mtc1      t0, fa1
    c.ole.s   fcc0, fa0, fa1
    li        rRESULT1, LONG_MIN_HIGH
    bc1t      fcc0, .L${opcode}_get_opcode

    neg.s     fa1, fa1
    c.ole.s   fcc0, fa1, fa0
    nor       rRESULT0, rRESULT0, zero
    nor       rRESULT1, rRESULT1, zero
    bc1t      fcc0, .L${opcode}_get_opcode

    JAL(__fixsfdi)
    GET_INST_OPCODE(t1)                    #  extract opcode from rINST
    b         .L${opcode}_set_vreg
#endif
%def op_float_to_long_helper_code():

#ifndef MIPS32REVGE6
.Lop_float_to_long_get_opcode:
    GET_INST_OPCODE(t1)                    #  extract opcode from rINST
.Lop_float_to_long_set_vreg:
    SET_VREG64_GOTO(rRESULT0, rRESULT1, rOBJ, t1)   #  vA/vA+1 <- v0/v1
#endif

%def op_int_to_double():
%  funopWider(instr="cvt.d.w fv0, fa0")

%def op_int_to_float():
%  funop(instr="cvt.s.w fv0, fa0")

%def op_long_to_double():
    /*
     * long-to-double
     */
    /* unop vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[B]

#ifdef MIPS32REVGE6
    LOAD64_F(fv0, fv0f, a3)
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    cvt.d.l   fv0, fv0
#else
    LOAD64(rARG0, rARG1, a3)
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    JAL(__floatdidf)                       #  a0/a1 <- op, a2-a3 changed
#endif

    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_F_GOTO(fv0, fv0f, rOBJ, t0) #  vA/vA+1 <- result

%def op_long_to_float():
    /*
     * long-to-float
     */
    /* unop vA, vB */
    GET_OPB(a3)                            #  a3 <- B
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[B]

#ifdef MIPS32REVGE6
    LOAD64_F(fv0, fv0f, a3)
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    cvt.s.l   fv0, fv0
#else
    LOAD64(rARG0, rARG1, a3)
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    JAL(__floatdisf)
#endif

    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_F_GOTO(fv0, rOBJ, t0)         #  vA <- fv0

%def op_mul_double():
%  fbinopWide(instr="mul.d fv0, fa0, fa1")

%def op_mul_double_2addr():
%  fbinopWide2addr(instr="mul.d fv0, fa0, fa1")

%def op_mul_float():
%  fbinop(instr="mul.s fv0, fa0, fa1")

%def op_mul_float_2addr():
%  fbinop2addr(instr="mul.s fv0, fa0, fa1")

%def op_neg_double():
%  unopWide(instr="addu a1, a1, 0x80000000")

%def op_neg_float():
%  unop(instr="addu a0, a0, 0x80000000")

%def op_rem_double():
%  fbinopWide(instr="JAL(fmod)")

%def op_rem_double_2addr():
%  fbinopWide2addr(instr="JAL(fmod)")

%def op_rem_float():
%  fbinop(instr="JAL(fmodf)")

%def op_rem_float_2addr():
%  fbinop2addr(instr="JAL(fmodf)")

%def op_sub_double():
%  fbinopWide(instr="sub.d fv0, fa0, fa1")

%def op_sub_double_2addr():
%  fbinopWide2addr(instr="sub.d fv0, fa0, fa1")

%def op_sub_float():
%  fbinop(instr="sub.s fv0, fa0, fa1")

%def op_sub_float_2addr():
%  fbinop2addr(instr="sub.s fv0, fa0, fa1")
