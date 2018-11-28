%def fpcmp(suff="d", nanval="pos"):
/*
 * Compare two floating-point values.  Puts 0, 1, or -1 into the
 * destination register based on the results of the comparison.
 *
 * int compare(x, y) {
 *     if (x == y) {
 *         return 0;
 *     } else if (x < y) {
 *         return -1;
 *     } else if (x > y) {
 *         return 1;
 *     } else {
 *         return nanval ? 1 : -1;
 *     }
 * }
 */
    /* op vAA, vBB, vCC */
    movzbl  3(rPC), %ecx                    # ecx<- CC
    movzbl  2(rPC), %eax                    # eax<- BB
    GET_VREG_XMM${suff} %xmm0, %eax
    xor     %eax, %eax
    ucomis${suff} VREG_ADDRESS(%ecx), %xmm0
    jp      .L${opcode}_nan_is_${nanval}
    je      .L${opcode}_finish
    jb      .L${opcode}_less
.L${opcode}_nan_is_pos:
    incl    %eax
    jmp     .L${opcode}_finish
.L${opcode}_nan_is_neg:
.L${opcode}_less:
    decl    %eax
.L${opcode}_finish:
    SET_VREG %eax, rINST
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def fpcvt(instr="", load="", store="", wide="0"):
/*
 * Generic 32-bit FP conversion operation.
 */
    /* unop vA, vB */
    movzbl  rINSTbl, %ecx                   # ecx <- A+
    sarl    $$4, rINST                      # rINST <- B
    $load   VREG_ADDRESS(rINST)             # %st0 <- vB
    andb    $$0xf, %cl                      # ecx <- A
    $instr
    $store  VREG_ADDRESS(%ecx)              # vA <- %st0
    .if $wide
    CLEAR_WIDE_REF %ecx
    .else
    CLEAR_REF %ecx
    .endif
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 1

%def sseBinop(instr="", suff=""):
    movzbl  2(rPC), %ecx                    # ecx <- BB
    movzbl  3(rPC), %eax                    # eax <- CC
    GET_VREG_XMM${suff} %xmm0, %ecx         # %xmm0 <- 1st src
    ${instr}${suff} VREG_ADDRESS(%eax), %xmm0
    SET_VREG_XMM${suff} %xmm0, rINST        # vAA <- %xmm0
    pxor    %xmm0, %xmm0
    movs${suff}   %xmm0, VREG_REF_ADDRESS(rINST) # clear ref
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def sseBinop2Addr(instr="", suff=""):
    movzx   rINSTbl, %ecx                   # ecx <- A+
    andl    $$0xf, %ecx                     # ecx <- A
    GET_VREG_XMM${suff} %xmm0, %ecx         # %xmm0 <- 1st src
    sarl    $$4, rINST                      # rINST<- B
    ${instr}${suff} VREG_ADDRESS(rINST), %xmm0
    SET_VREG_XMM${suff} %xmm0, %ecx         # vAA<- %xmm0
    pxor    %xmm0, %xmm0
    movs${suff} %xmm0, VREG_REF_ADDRESS(rINST)  # clear ref
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 1

%def op_add_double():
%  sseBinop(instr="adds", suff="d")

%def op_add_double_2addr():
%  sseBinop2Addr(instr="adds", suff="d")

%def op_add_float():
%  sseBinop(instr="adds", suff="s")

%def op_add_float_2addr():
%  sseBinop2Addr(instr="adds", suff="s")

%def op_cmpg_double():
%  fpcmp(suff="d", nanval="pos")

%def op_cmpg_float():
%  fpcmp(suff="s", nanval="pos")

%def op_cmpl_double():
%  fpcmp(suff="d", nanval="neg")

%def op_cmpl_float():
%  fpcmp(suff="s", nanval="neg")

%def op_div_double():
%  sseBinop(instr="divs", suff="d")

%def op_div_double_2addr():
%  sseBinop2Addr(instr="divs", suff="d")

%def op_div_float():
%  sseBinop(instr="divs", suff="s")

%def op_div_float_2addr():
%  sseBinop2Addr(instr="divs", suff="s")

%def op_double_to_float():
%  fpcvt(load="fldl", store="fstps")

%def op_double_to_int():
%  cvtfp_int(srcdouble="1", tgtlong="0")

%def op_double_to_long():
%  cvtfp_int(srcdouble="1", tgtlong="1")

%def op_float_to_double():
%  fpcvt(load="flds", store="fstpl", wide="1")

%def op_float_to_int():
%  cvtfp_int(srcdouble="0", tgtlong="0")

%def op_float_to_long():
%  cvtfp_int(srcdouble="0", tgtlong="1")

%def op_int_to_double():
%  fpcvt(load="fildl", store="fstpl", wide="1")

%def op_int_to_float():
%  fpcvt(load="fildl", store="fstps")

%def op_long_to_double():
%  fpcvt(load="fildll", store="fstpl", wide="1")

%def op_long_to_float():
%  fpcvt(load="fildll", store="fstps")

%def op_mul_double():
%  sseBinop(instr="muls", suff="d")

%def op_mul_double_2addr():
%  sseBinop2Addr(instr="muls", suff="d")

%def op_mul_float():
%  sseBinop(instr="muls", suff="s")

%def op_mul_float_2addr():
%  sseBinop2Addr(instr="muls", suff="s")

%def op_neg_double():
%  fpcvt(instr="fchs", load="fldl", store="fstpl", wide="1")

%def op_neg_float():
%  fpcvt(instr="fchs", load="flds", store="fstps")

%def op_rem_double():
    /* rem_double vAA, vBB, vCC */
    movzbl  3(rPC), %ecx                    # ecx <- BB
    movzbl  2(rPC), %eax                    # eax <- CC
    fldl    VREG_ADDRESS(%ecx)              # %st1 <- fp[vBB]
    fldl    VREG_ADDRESS(%eax)              # %st0 <- fp[vCC]
1:
    fprem
    fstsw   %ax
    sahf
    jp      1b
    fstp    %st(1)
    fstpl   VREG_ADDRESS(rINST)             # fp[vAA] <- %st
    CLEAR_WIDE_REF rINST
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def op_rem_double_2addr():
    /* rem_double/2addr vA, vB */
    movzx   rINSTbl, %ecx                   # ecx <- A+
    sarl    $$4, rINST                      # rINST <- B
    fldl    VREG_ADDRESS(rINST)             # vB to fp stack
    andb    $$0xf, %cl                      # ecx <- A
    fldl    VREG_ADDRESS(%ecx)              # vA to fp stack
1:
    fprem
    fstsw   %ax
    sahf
    jp      1b
    fstp    %st(1)
    fstpl   VREG_ADDRESS(%ecx)              # %st to vA
    CLEAR_WIDE_REF %ecx
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 1

%def op_rem_float():
    /* rem_float vAA, vBB, vCC */
    movzbl  3(rPC), %ecx                    # ecx <- BB
    movzbl  2(rPC), %eax                    # eax <- CC
    flds    VREG_ADDRESS(%ecx)              # vBB to fp stack
    flds    VREG_ADDRESS(%eax)              # vCC to fp stack
1:
    fprem
    fstsw   %ax
    sahf
    jp      1b
    fstp    %st(1)
    fstps   VREG_ADDRESS(rINST)             # %st to vAA
    CLEAR_REF rINST
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def op_rem_float_2addr():
    /* rem_float/2addr vA, vB */
    movzx   rINSTbl, %ecx                   # ecx <- A+
    sarl    $$4, rINST                      # rINST <- B
    flds    VREG_ADDRESS(rINST)             # vB to fp stack
    andb    $$0xf, %cl                      # ecx <- A
    flds    VREG_ADDRESS(%ecx)              # vA to fp stack
1:
    fprem
    fstsw   %ax
    sahf
    jp      1b
    fstp    %st(1)
    fstps   VREG_ADDRESS(%ecx)              # %st to vA
    CLEAR_REF %ecx
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 1

%def op_sub_double():
%  sseBinop(instr="subs", suff="d")

%def op_sub_double_2addr():
%  sseBinop2Addr(instr="subs", suff="d")

%def op_sub_float():
%  sseBinop(instr="subs", suff="s")

%def op_sub_float_2addr():
%  sseBinop2Addr(instr="subs", suff="s")
