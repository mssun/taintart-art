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
    movzbq  3(rPC), %rcx                    # ecx<- CC
    movzbq  2(rPC), %rax                    # eax<- BB
    movs${suff} VREG_ADDRESS(%rax), %xmm0
    xor     %eax, %eax
    ucomis${suff} VREG_ADDRESS(%rcx), %xmm0
    jp      .L${opcode}_nan_is_${nanval}
    je      .L${opcode}_finish
    jb      .L${opcode}_less
.L${opcode}_nan_is_pos:
    addb    $$1, %al
    jmp     .L${opcode}_finish
.L${opcode}_nan_is_neg:
.L${opcode}_less:
    movl    $$-1, %eax
.L${opcode}_finish:
    SET_VREG %eax, rINSTq
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def fpcvt(source_suffix="", dest_suffix="", wide=""):
/*
 * Generic 32-bit FP conversion operation.
 */
    /* unop vA, vB */
    movl    rINST, %ecx                     # rcx <- A+
    sarl    $$4, rINST                      # rINST <- B
    andb    $$0xf, %cl                      # ecx <- A
    cvts${source_suffix}2s${dest_suffix}    VREG_ADDRESS(rINSTq), %xmm0
    .if $wide
    movsd   %xmm0, VREG_ADDRESS(%rcx)
    CLEAR_WIDE_REF %rcx
    .else
    movss   %xmm0, VREG_ADDRESS(%rcx)
    CLEAR_REF %rcx
    .endif
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 1

%def sseBinop(instr="", suff=""):
    movzbq  2(rPC), %rcx                    # ecx <- BB
    movzbq  3(rPC), %rax                    # eax <- CC
    movs${suff}   VREG_ADDRESS(%rcx), %xmm0       # %xmm0 <- 1st src
    ${instr}${suff} VREG_ADDRESS(%rax), %xmm0
    movs${suff}   %xmm0, VREG_ADDRESS(rINSTq)     # vAA <- %xmm0
    pxor    %xmm0, %xmm0
    movs${suff}   %xmm0, VREG_REF_ADDRESS(rINSTq) # clear ref
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def sseBinop2Addr(instr="", suff=""):
    movl    rINST, %ecx                     # ecx <- A+
    andl    $$0xf, %ecx                     # ecx <- A
    movs${suff} VREG_ADDRESS(%rcx), %xmm0        # %xmm0 <- 1st src
    sarl    $$4, rINST                      # rINST<- B
    ${instr}${suff} VREG_ADDRESS(rINSTq), %xmm0
    movs${suff} %xmm0, VREG_ADDRESS(%rcx)   # vAA<- %xmm0
    pxor    %xmm0, %xmm0
    movs${suff} %xmm0, VREG_REF_ADDRESS(rINSTq)  # clear ref
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
%  fpcvt(source_suffix="d", dest_suffix="s", wide="0")

%def op_double_to_int():
%  cvtfp_int(fp_suffix="d", i_suffix="l", max_const="$0x7fffffff", result_reg="%eax", wide="0")

%def op_double_to_long():
%  cvtfp_int(fp_suffix="d", i_suffix="q", max_const="$0x7fffffffffffffff", result_reg="%rax", wide="1")

%def op_float_to_double():
%  fpcvt(source_suffix="s", dest_suffix="d", wide="1")

%def op_float_to_int():
%  cvtfp_int(fp_suffix="s", i_suffix="l", max_const="$0x7fffffff", result_reg="%eax", wide="0")

%def op_float_to_long():
%  cvtfp_int(fp_suffix="s", i_suffix="q", max_const="$0x7fffffffffffffff", result_reg="%rax", wide="1")

%def op_int_to_double():
%  fpcvt(source_suffix="i", dest_suffix="dl", wide="1")

%def op_int_to_float():
%  fpcvt(source_suffix="i", dest_suffix="sl", wide="0")

%def op_long_to_double():
%  fpcvt(source_suffix="i", dest_suffix="dq", wide="1")

%def op_long_to_float():
%  fpcvt(source_suffix="i", dest_suffix="sq", wide="0")

%def op_mul_double():
%  sseBinop(instr="muls", suff="d")

%def op_mul_double_2addr():
%  sseBinop2Addr(instr="muls", suff="d")

%def op_mul_float():
%  sseBinop(instr="muls", suff="s")

%def op_mul_float_2addr():
%  sseBinop2Addr(instr="muls", suff="s")

%def op_neg_double():
%  unop(preinstr="    movq    $0x8000000000000000, %rsi", instr="    xorq    %rsi, %rax", wide="1")

%def op_neg_float():
%  unop(instr="    xorl    $0x80000000, %eax")

%def op_rem_double():
    /* rem_double vAA, vBB, vCC */
    movzbq  3(rPC), %rcx                    # ecx <- BB
    movzbq  2(rPC), %rax                    # eax <- CC
    fldl    VREG_ADDRESS(%rcx)              # %st1 <- fp[vBB]
    fldl    VREG_ADDRESS(%rax)              # %st0 <- fp[vCC]
1:
    fprem
    fstsw   %ax
    sahf
    jp      1b
    fstp    %st(1)
    fstpl   VREG_ADDRESS(rINSTq)            # fp[vAA] <- %st
    CLEAR_WIDE_REF rINSTq
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def op_rem_double_2addr():
    /* rem_double/2addr vA, vB */
    movzbq  rINSTbl, %rcx                   # ecx <- A+
    sarl    $$4, rINST                      # rINST <- B
    fldl    VREG_ADDRESS(rINSTq)            # vB to fp stack
    andb    $$0xf, %cl                      # ecx <- A
    fldl    VREG_ADDRESS(%rcx)              # vA to fp stack
1:
    fprem
    fstsw   %ax
    sahf
    jp      1b
    fstp    %st(1)
    fstpl   VREG_ADDRESS(%rcx)              # %st to vA
    CLEAR_WIDE_REF %rcx
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 1

%def op_rem_float():
    /* rem_float vAA, vBB, vCC */
    movzbq  3(rPC), %rcx                    # ecx <- BB
    movzbq  2(rPC), %rax                    # eax <- CC
    flds    VREG_ADDRESS(%rcx)              # vBB to fp stack
    flds    VREG_ADDRESS(%rax)              # vCC to fp stack
1:
    fprem
    fstsw   %ax
    sahf
    jp      1b
    fstp    %st(1)
    fstps   VREG_ADDRESS(rINSTq)            # %st to vAA
    CLEAR_REF rINSTq
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def op_rem_float_2addr():
    /* rem_float/2addr vA, vB */
    movzbq  rINSTbl, %rcx                   # ecx <- A+
    sarl    $$4, rINST                      # rINST <- B
    flds    VREG_ADDRESS(rINSTq)            # vB to fp stack
    andb    $$0xf, %cl                      # ecx <- A
    flds    VREG_ADDRESS(%rcx)              # vA to fp stack
1:
    fprem
    fstsw   %ax
    sahf
    jp      1b
    fstp    %st(1)
    fstps   VREG_ADDRESS(%rcx)              # %st to vA
    CLEAR_REF %rcx
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 1

%def op_sub_double():
%  sseBinop(instr="subs", suff="d")

%def op_sub_double_2addr():
%  sseBinop2Addr(instr="subs", suff="d")

%def op_sub_float():
%  sseBinop(instr="subs", suff="s")

%def op_sub_float_2addr():
%  sseBinop2Addr(instr="subs", suff="s")
