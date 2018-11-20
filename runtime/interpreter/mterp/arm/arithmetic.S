%def binop(preinstr="", result="r0", chkzero="0", instr=""):
    /*
     * Generic 32-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = r0 op r1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (INT_MIN / -1) here, because the ARM math lib
     * handles it correctly.
     *
     * For: add-int, sub-int, mul-int, div-int, rem-int, and-int, or-int,
     *      xor-int, shl-int, shr-int, ushr-int, add-float, sub-float,
     *      mul-float, div-float, rem-float
     */
    /* binop vAA, vBB, vCC */
    FETCH r0, 1                         @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    mov     r3, r0, lsr #8              @ r3<- CC
    and     r2, r0, #255                @ r2<- BB
    GET_VREG r1, r3                     @ r1<- vCC
    GET_VREG r0, r2                     @ r0<- vBB
    .if $chkzero
    cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    .endif

    FETCH_ADVANCE_INST 2                @ advance rPC, load rINST
    $preinstr                           @ optional op; may set condition codes
    $instr                              @ $result<- op, r0-r3 changed
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    SET_VREG $result, r9                @ vAA<- $result
    GOTO_OPCODE ip                      @ jump to next instruction
    /* 11-14 instructions */

%def binop2addr(preinstr="", result="r0", chkzero="0", instr=""):
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0 op r1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr, add-float/2addr,
     *      sub-float/2addr, mul-float/2addr, div-float/2addr, rem-float/2addr
     */
    /* binop/2addr vA, vB */
    mov     r3, rINST, lsr #12          @ r3<- B
    ubfx    r9, rINST, #8, #4           @ r9<- A
    GET_VREG r1, r3                     @ r1<- vB
    GET_VREG r0, r9                     @ r0<- vA
    .if $chkzero
    cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST 1                @ advance rPC, load rINST

    $preinstr                           @ optional op; may set condition codes
    $instr                              @ $result<- op, r0-r3 changed
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    SET_VREG $result, r9                @ vAA<- $result
    GOTO_OPCODE ip                      @ jump to next instruction
    /* 10-13 instructions */

%def binopLit16(result="r0", chkzero="0", instr=""):
    /*
     * Generic 32-bit "lit16" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0 op r1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * For: add-int/lit16, rsub-int, mul-int/lit16, div-int/lit16,
     *      rem-int/lit16, and-int/lit16, or-int/lit16, xor-int/lit16
     */
    /* binop/lit16 vA, vB, #+CCCC */
    FETCH_S r1, 1                       @ r1<- ssssCCCC (sign-extended)
    mov     r2, rINST, lsr #12          @ r2<- B
    ubfx    r9, rINST, #8, #4           @ r9<- A
    GET_VREG r0, r2                     @ r0<- vB
    .if $chkzero
    cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST 2                @ advance rPC, load rINST

    $instr                              @ $result<- op, r0-r3 changed
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    SET_VREG $result, r9                @ vAA<- $result
    GOTO_OPCODE ip                      @ jump to next instruction
    /* 10-13 instructions */

%def binopLit8(extract="asr     r1, r3, #8", result="r0", chkzero="0", instr=""):
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0 op r1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * You can override "extract" if the extraction of the literal value
     * from r3 to r1 is not the default "asr r1, r3, #8". The extraction
     * can be omitted completely if the shift is embedded in "instr".
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    /* binop/lit8 vAA, vBB, #+CC */
    FETCH_S r3, 1                       @ r3<- ssssCCBB (sign-extended for CC)
    mov     r9, rINST, lsr #8           @ r9<- AA
    and     r2, r3, #255                @ r2<- BB
    GET_VREG r0, r2                     @ r0<- vBB
    $extract                            @ optional; typically r1<- ssssssCC (sign extended)
    .if $chkzero
    @cmp     r1, #0                     @ is second operand zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST 2                @ advance rPC, load rINST

    $instr                              @ $result<- op, r0-r3 changed
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    SET_VREG $result, r9                @ vAA<- $result
    GOTO_OPCODE ip                      @ jump to next instruction
    /* 10-12 instructions */

%def binopWide(preinstr="", result0="r0", result1="r1", chkzero="0", instr=""):
    /*
     * Generic 64-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = r0-r1 op r2-r3".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * for: add-long, sub-long, div-long, rem-long, and-long, or-long,
     *      xor-long, add-double, sub-double, mul-double, div-double,
     *      rem-double
     *
     * IMPORTANT: you may specify "chkzero" or "preinstr" but not both.
     */
    /* binop vAA, vBB, vCC */
    FETCH r0, 1                         @ r0<- CCBB
    mov     rINST, rINST, lsr #8        @ rINST<- AA
    and     r2, r0, #255                @ r2<- BB
    mov     r3, r0, lsr #8              @ r3<- CC
    VREG_INDEX_TO_ADDR r9, rINST        @ r9<- &fp[AA]
    VREG_INDEX_TO_ADDR r2, r2           @ r2<- &fp[BB]
    VREG_INDEX_TO_ADDR r3, r3           @ r3<- &fp[CC]
    GET_VREG_WIDE_BY_ADDR r0, r1, r2    @ r0/r1<- vBB/vBB+1
    GET_VREG_WIDE_BY_ADDR r2, r3, r3    @ r2/r3<- vCC/vCC+1
    .if $chkzero
    orrs    ip, r2, r3                  @ second arg (r2-r3) is zero?
    beq     common_errDivideByZero
    .endif
    CLEAR_SHADOW_PAIR rINST, lr, ip     @ Zero out the shadow regs
    FETCH_ADVANCE_INST 2                @ advance rPC, load rINST
    $preinstr                           @ optional op; may set condition codes
    $instr                              @ result<- op, r0-r3 changed
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    SET_VREG_WIDE_BY_ADDR $result0,$result1,r9  @ vAA/vAA+1<,  $result0/$result1
    GOTO_OPCODE ip                      @ jump to next instruction
    /* 14-17 instructions */

%def binopWide2addr(preinstr="", result0="r0", result1="r1", chkzero="0", instr=""):
    /*
     * Generic 64-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0-r1 op r2-r3".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * For: add-long/2addr, sub-long/2addr, div-long/2addr, rem-long/2addr,
     *      and-long/2addr, or-long/2addr, xor-long/2addr, add-double/2addr,
     *      sub-double/2addr, mul-double/2addr, div-double/2addr,
     *      rem-double/2addr
     */
    /* binop/2addr vA, vB */
    mov     r1, rINST, lsr #12          @ r1<- B
    ubfx    rINST, rINST, #8, #4        @ rINST<- A
    VREG_INDEX_TO_ADDR r1, r1           @ r1<- &fp[B]
    VREG_INDEX_TO_ADDR r9, rINST        @ r9<- &fp[A]
    GET_VREG_WIDE_BY_ADDR r2, r3, r1    @ r2/r3<- vBB/vBB+1
    GET_VREG_WIDE_BY_ADDR r0, r1, r9    @ r0/r1<- vAA/vAA+1
    .if $chkzero
    orrs    ip, r2, r3                  @ second arg (r2-r3) is zero?
    beq     common_errDivideByZero
    .endif
    CLEAR_SHADOW_PAIR rINST, ip, lr     @ Zero shadow regs
    FETCH_ADVANCE_INST 1                @ advance rPC, load rINST
    $preinstr                           @ optional op; may set condition codes
    $instr                              @ result<- op, r0-r3 changed
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    SET_VREG_WIDE_BY_ADDR $result0,$result1,r9  @ vAA/vAA+1<- $result0/$result1
    GOTO_OPCODE ip                      @ jump to next instruction
    /* 12-15 instructions */

%def unop(preinstr="", instr=""):
    /*
     * Generic 32-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = op r0".
     * This could be an ARM instruction or a function call.
     *
     * for: neg-int, not-int, neg-float, int-to-float, float-to-int,
     *      int-to-byte, int-to-char, int-to-short
     */
    /* unop vA, vB */
    mov     r3, rINST, lsr #12          @ r3<- B
    ubfx    r9, rINST, #8, #4           @ r9<- A
    GET_VREG r0, r3                     @ r0<- vB
    $preinstr                           @ optional op; may set condition codes
    FETCH_ADVANCE_INST 1                @ advance rPC, load rINST
    $instr                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    SET_VREG r0, r9                     @ vAA<- r0
    GOTO_OPCODE ip                      @ jump to next instruction
    /* 8-9 instructions */

%def unopNarrower(preinstr="", instr=""):
    /*
     * Generic 64bit-to-32bit unary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = op r0/r1", where
     * "result" is a 32-bit quantity in r0.
     *
     * For: long-to-float
     *
     * (This would work for long-to-int, but that instruction is actually
     * an exact match for op_move.)
     */
    /* unop vA, vB */
    mov     r3, rINST, lsr #12          @ r3<- B
    ubfx    r9, rINST, #8, #4           @ r9<- A
    VREG_INDEX_TO_ADDR r3, r3           @ r3<- &fp[B]
    GET_VREG_WIDE_BY_ADDR r0, r1, r3    @ r0/r1<- vB/vB+1
    FETCH_ADVANCE_INST 1                @ advance rPC, load rINST
    $preinstr                           @ optional op; may set condition codes
    $instr                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    SET_VREG r0, r9                     @ vA<- r0
    GOTO_OPCODE ip                      @ jump to next instruction
    /* 9-10 instructions */

%def unopWide(preinstr="", instr=""):
    /*
     * Generic 64-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = op r0/r1".
     * This could be an ARM instruction or a function call.
     *
     * For: neg-long, not-long, neg-double, long-to-double, double-to-long
     */
    /* unop vA, vB */
    mov     r3, rINST, lsr #12          @ r3<- B
    ubfx    rINST, rINST, #8, #4        @ rINST<- A
    VREG_INDEX_TO_ADDR r3, r3           @ r3<- &fp[B]
    VREG_INDEX_TO_ADDR r9, rINST        @ r9<- &fp[A]
    GET_VREG_WIDE_BY_ADDR r0, r1, r3    @ r0/r1<- vAA
    CLEAR_SHADOW_PAIR rINST, ip, lr     @ Zero shadow regs
    FETCH_ADVANCE_INST 1                @ advance rPC, load rINST
    $preinstr                           @ optional op; may set condition codes
    $instr                              @ r0/r1<- op, r2-r3 changed
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    SET_VREG_WIDE_BY_ADDR r0, r1, r9    @ vAA<- r0/r1
    GOTO_OPCODE ip                      @ jump to next instruction
    /* 10-11 instructions */

%def unopWider(preinstr="", instr=""):
    /*
     * Generic 32bit-to-64bit unary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = op r0", where
     * "result" is a 64-bit quantity in r0/r1.
     *
     * For: int-to-long, int-to-double, float-to-long, float-to-double
     */
    /* unop vA, vB */
    mov     r3, rINST, lsr #12          @ r3<- B
    ubfx    rINST, rINST, #8, #4        @ rINST<- A
    GET_VREG r0, r3                     @ r0<- vB
    VREG_INDEX_TO_ADDR r9, rINST        @ r9<- &fp[A]
    $preinstr                           @ optional op; may set condition codes
    CLEAR_SHADOW_PAIR rINST, ip, lr     @ Zero shadow regs
    FETCH_ADVANCE_INST 1                @ advance rPC, load rINST
    $instr                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    SET_VREG_WIDE_BY_ADDR r0, r1, r9    @ vA/vA+1<- r0/r1
    GOTO_OPCODE ip                      @ jump to next instruction
    /* 9-10 instructions */

%def op_add_int():
%  binop(instr="add     r0, r0, r1")

%def op_add_int_2addr():
%  binop2addr(instr="add     r0, r0, r1")

%def op_add_int_lit16():
%  binopLit16(instr="add     r0, r0, r1")

%def op_add_int_lit8():
%  binopLit8(extract="", instr="add     r0, r0, r3, asr #8")

%def op_add_long():
%  binopWide(preinstr="adds    r0, r0, r2", instr="adc     r1, r1, r3")

%def op_add_long_2addr():
%  binopWide2addr(preinstr="adds    r0, r0, r2", instr="adc     r1, r1, r3")

%def op_and_int():
%  binop(instr="and     r0, r0, r1")

%def op_and_int_2addr():
%  binop2addr(instr="and     r0, r0, r1")

%def op_and_int_lit16():
%  binopLit16(instr="and     r0, r0, r1")

%def op_and_int_lit8():
%  binopLit8(extract="", instr="and     r0, r0, r3, asr #8")

%def op_and_long():
%  binopWide(preinstr="and     r0, r0, r2", instr="and     r1, r1, r3")

%def op_and_long_2addr():
%  binopWide2addr(preinstr="and     r0, r0, r2", instr="and     r1, r1, r3")

%def op_cmp_long():
    /*
     * Compare two 64-bit values.  Puts 0, 1, or -1 into the destination
     * register based on the results of the comparison.
     */
    /* cmp-long vAA, vBB, vCC */
    FETCH r0, 1                         @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    and     r2, r0, #255                @ r2<- BB
    mov     r3, r0, lsr #8              @ r3<- CC
    VREG_INDEX_TO_ADDR r2, r2           @ r2<- &fp[BB]
    VREG_INDEX_TO_ADDR r3, r3           @ r3<- &fp[CC]
    GET_VREG_WIDE_BY_ADDR r0, r1, r2    @ r0/r1<- vBB/vBB+1
    GET_VREG_WIDE_BY_ADDR r2, r3, r3    @ r2/r3<- vCC/vCC+1
    cmp     r0, r2
    sbcs    ip, r1, r3                  @ Sets correct CCs for checking LT (but not EQ/NE)
    mov     ip, #0
    mvnlt   ip, #0                      @ -1
    cmpeq   r0, r2                      @ For correct EQ/NE, we may need to repeat the first CMP
    orrne   ip, #1
    FETCH_ADVANCE_INST 2                @ advance rPC, load rINST
    SET_VREG ip, r9                     @ vAA<- ip
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    GOTO_OPCODE ip                      @ jump to next instruction

%def op_div_int():
    /*
     * Specialized 32-bit binary operation
     *
     * Performs "r0 = r0 div r1". The selection between sdiv or the gcc helper
     * depends on the compile time value of __ARM_ARCH_EXT_IDIV__ (defined for
     * ARMv7 CPUs that have hardware division support).
     *
     * div-int
     *
     */
    FETCH r0, 1                         @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    mov     r3, r0, lsr #8              @ r3<- CC
    and     r2, r0, #255                @ r2<- BB
    GET_VREG r1, r3                     @ r1<- vCC
    GET_VREG r0, r2                     @ r0<- vBB
    cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero

    FETCH_ADVANCE_INST 2                @ advance rPC, load rINST
#ifdef __ARM_ARCH_EXT_IDIV__
    sdiv    r0, r0, r1                  @ r0<- op
#else
    bl    __aeabi_idiv                  @ r0<- op, r0-r3 changed
#endif
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    SET_VREG r0, r9                     @ vAA<- r0
    GOTO_OPCODE ip                      @ jump to next instruction
    /* 11-14 instructions */

%def op_div_int_2addr():
    /*
     * Specialized 32-bit binary operation
     *
     * Performs "r0 = r0 div r1". The selection between sdiv or the gcc helper
     * depends on the compile time value of __ARM_ARCH_EXT_IDIV__ (defined for
     * ARMv7 CPUs that have hardware division support).
     *
     * div-int/2addr
     *
     */
    mov     r3, rINST, lsr #12          @ r3<- B
    ubfx    r9, rINST, #8, #4           @ r9<- A
    GET_VREG r1, r3                     @ r1<- vB
    GET_VREG r0, r9                     @ r0<- vA
    cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    FETCH_ADVANCE_INST 1                @ advance rPC, load rINST

#ifdef __ARM_ARCH_EXT_IDIV__
    sdiv    r0, r0, r1                  @ r0<- op
#else
    bl       __aeabi_idiv               @ r0<- op, r0-r3 changed
#endif
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    SET_VREG r0, r9                     @ vAA<- r0
    GOTO_OPCODE ip                      @ jump to next instruction
    /* 10-13 instructions */


%def op_div_int_lit16():
    /*
     * Specialized 32-bit binary operation
     *
     * Performs "r0 = r0 div r1". The selection between sdiv or the gcc helper
     * depends on the compile time value of __ARM_ARCH_EXT_IDIV__ (defined for
     * ARMv7 CPUs that have hardware division support).
     *
     * div-int/lit16
     *
     */
    FETCH_S r1, 1                       @ r1<- ssssCCCC (sign-extended)
    mov     r2, rINST, lsr #12          @ r2<- B
    ubfx    r9, rINST, #8, #4           @ r9<- A
    GET_VREG r0, r2                     @ r0<- vB
    cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    FETCH_ADVANCE_INST 2                @ advance rPC, load rINST

#ifdef __ARM_ARCH_EXT_IDIV__
    sdiv    r0, r0, r1                  @ r0<- op
#else
    bl       __aeabi_idiv               @ r0<- op, r0-r3 changed
#endif
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    SET_VREG r0, r9                     @ vAA<- r0
    GOTO_OPCODE ip                      @ jump to next instruction
    /* 10-13 instructions */

%def op_div_int_lit8():
    /*
     * Specialized 32-bit binary operation
     *
     * Performs "r0 = r0 div r1". The selection between sdiv or the gcc helper
     * depends on the compile time value of __ARM_ARCH_EXT_IDIV__ (defined for
     * ARMv7 CPUs that have hardware division support).
     *
     * div-int/lit8
     *
     */
    FETCH_S r3, 1                       @ r3<- ssssCCBB (sign-extended for CC
    mov     r9, rINST, lsr #8           @ r9<- AA
    and     r2, r3, #255                @ r2<- BB
    GET_VREG r0, r2                     @ r0<- vBB
    movs    r1, r3, asr #8              @ r1<- ssssssCC (sign extended)
    @cmp     r1, #0                     @ is second operand zero?
    beq     common_errDivideByZero
    FETCH_ADVANCE_INST 2                @ advance rPC, load rINST

#ifdef __ARM_ARCH_EXT_IDIV__
    sdiv    r0, r0, r1                  @ r0<- op
#else
    bl   __aeabi_idiv                   @ r0<- op, r0-r3 changed
#endif
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    SET_VREG r0, r9                     @ vAA<- r0
    GOTO_OPCODE ip                      @ jump to next instruction
    /* 10-12 instructions */

%def op_div_long():
%  binopWide(instr="bl      __aeabi_ldivmod", chkzero="1")

%def op_div_long_2addr():
%  binopWide2addr(instr="bl      __aeabi_ldivmod", chkzero="1")

%def op_int_to_byte():
%  unop(instr="sxtb    r0, r0")

%def op_int_to_char():
%  unop(instr="uxth    r0, r0")

%def op_int_to_long():
%  unopWider(instr="mov     r1, r0, asr #31")

%def op_int_to_short():
%  unop(instr="sxth    r0, r0")

%def op_long_to_int():
/* we ignore the high word, making this equivalent to a 32-bit reg move */
%  op_move()

%def op_mul_int():
/* must be "mul r0, r1, r0" -- "r0, r0, r1" is illegal */
%  binop(instr="mul     r0, r1, r0")

%def op_mul_int_2addr():
/* must be "mul r0, r1, r0" -- "r0, r0, r1" is illegal */
%  binop2addr(instr="mul     r0, r1, r0")

%def op_mul_int_lit16():
/* must be "mul r0, r1, r0" -- "r0, r0, r1" is illegal */
%  binopLit16(instr="mul     r0, r1, r0")

%def op_mul_int_lit8():
/* must be "mul r0, r1, r0" -- "r0, r0, r1" is illegal */
%  binopLit8(instr="mul     r0, r1, r0")

%def op_mul_long():
    /*
     * Signed 64-bit integer multiply.
     *
     * Consider WXxYZ (r1r0 x r3r2) with a long multiply:
     *        WX
     *      x YZ
     *  --------
     *     ZW ZX
     *  YW YX
     *
     * The low word of the result holds ZX, the high word holds
     * (ZW+YX) + (the high overflow from ZX).  YW doesn't matter because
     * it doesn't fit in the low 64 bits.
     *
     * Unlike most ARM math operations, multiply instructions have
     * restrictions on using the same register more than once (Rd and Rm
     * cannot be the same).
     */
    /* mul-long vAA, vBB, vCC */
    FETCH r0, 1                         @ r0<- CCBB
    and     r2, r0, #255                @ r2<- BB
    mov     r3, r0, lsr #8              @ r3<- CC
    VREG_INDEX_TO_ADDR r2, r2           @ r2<- &fp[BB]
    VREG_INDEX_TO_ADDR r3, r3           @ r3<- &fp[CC]
    GET_VREG_WIDE_BY_ADDR r0, r1, r2    @ r0/r1<- vBB/vBB+1
    GET_VREG_WIDE_BY_ADDR r2, r3, r3    @ r2/r3<- vCC/vCC+1
    mul     ip, r2, r1                  @ ip<- ZxW
    umull   r1, lr, r2, r0              @ r1/lr <- ZxX
    mla     r2, r0, r3, ip              @ r2<- YxX + (ZxW)
    mov     r0, rINST, lsr #8           @ r0<- AA
    add     r2, r2, lr                  @ r2<- lr + low(ZxW + (YxX))
    CLEAR_SHADOW_PAIR r0, lr, ip        @ Zero out the shadow regs
    VREG_INDEX_TO_ADDR r0, r0           @ r0<- &fp[AA]
    FETCH_ADVANCE_INST 2                @ advance rPC, load rINST
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    SET_VREG_WIDE_BY_ADDR r1, r2 , r0   @ vAA/vAA+1<- r1/r2
    GOTO_OPCODE ip                      @ jump to next instruction

%def op_mul_long_2addr():
    /*
     * Signed 64-bit integer multiply, "/2addr" version.
     *
     * See op_mul_long for an explanation.
     *
     * We get a little tight on registers, so to avoid looking up &fp[A]
     * again we stuff it into rINST.
     */
    /* mul-long/2addr vA, vB */
    mov     r1, rINST, lsr #12          @ r1<- B
    ubfx    r9, rINST, #8, #4           @ r9<- A
    VREG_INDEX_TO_ADDR r1, r1           @ r1<- &fp[B]
    VREG_INDEX_TO_ADDR rINST, r9        @ rINST<- &fp[A]
    GET_VREG_WIDE_BY_ADDR r2, r3, r1    @ r2/r3<- vBB/vBB+1
    GET_VREG_WIDE_BY_ADDR r0, r1, rINST @ r0/r1<- vAA/vAA+1
    mul     ip, r2, r1                  @ ip<- ZxW
    umull   r1, lr, r2, r0              @ r1/lr <- ZxX
    mla     r2, r0, r3, ip              @ r2<- YxX + (ZxW)
    mov     r0, rINST                   @ r0<- &fp[A] (free up rINST)
    FETCH_ADVANCE_INST 1                @ advance rPC, load rINST
    add     r2, r2, lr                  @ r2<- r2 + low(ZxW + (YxX))
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    SET_VREG_WIDE_BY_ADDR r1, r2, r0    @ vAA/vAA+1<- r1/r2
    GOTO_OPCODE ip                      @ jump to next instruction

%def op_neg_int():
%  unop(instr="rsb     r0, r0, #0")

%def op_neg_long():
%  unopWide(preinstr="rsbs    r0, r0, #0", instr="rsc     r1, r1, #0")

%def op_not_int():
%  unop(instr="mvn     r0, r0")

%def op_not_long():
%  unopWide(preinstr="mvn     r0, r0", instr="mvn     r1, r1")

%def op_or_int():
%  binop(instr="orr     r0, r0, r1")

%def op_or_int_2addr():
%  binop2addr(instr="orr     r0, r0, r1")

%def op_or_int_lit16():
%  binopLit16(instr="orr     r0, r0, r1")

%def op_or_int_lit8():
%  binopLit8(extract="", instr="orr     r0, r0, r3, asr #8")

%def op_or_long():
%  binopWide(preinstr="orr     r0, r0, r2", instr="orr     r1, r1, r3")

%def op_or_long_2addr():
%  binopWide2addr(preinstr="orr     r0, r0, r2", instr="orr     r1, r1, r3")

%def op_rem_int():
    /*
     * Specialized 32-bit binary operation
     *
     * Performs "r1 = r0 rem r1". The selection between sdiv block or the gcc helper
     * depends on the compile time value of __ARM_ARCH_EXT_IDIV__ (defined for
     * ARMv7 CPUs that have hardware division support).
     *
     * NOTE: idivmod returns quotient in r0 and remainder in r1
     *
     * rem-int
     *
     */
    FETCH r0, 1                         @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    mov     r3, r0, lsr #8              @ r3<- CC
    and     r2, r0, #255                @ r2<- BB
    GET_VREG r1, r3                     @ r1<- vCC
    GET_VREG r0, r2                     @ r0<- vBB
    cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero

    FETCH_ADVANCE_INST 2                @ advance rPC, load rINST
#ifdef __ARM_ARCH_EXT_IDIV__
    sdiv    r2, r0, r1
    mls  r1, r1, r2, r0                 @ r1<- op, r0-r2 changed
#else
    bl   __aeabi_idivmod                @ r1<- op, r0-r3 changed
#endif
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    SET_VREG r1, r9                     @ vAA<- r1
    GOTO_OPCODE ip                      @ jump to next instruction
    /* 11-14 instructions */

%def op_rem_int_2addr():
    /*
     * Specialized 32-bit binary operation
     *
     * Performs "r1 = r0 rem r1". The selection between sdiv block or the gcc helper
     * depends on the compile time value of __ARM_ARCH_EXT_IDIV__ (defined for
     * ARMv7 CPUs that have hardware division support).
     *
     * NOTE: idivmod returns quotient in r0 and remainder in r1
     *
     * rem-int/2addr
     *
     */
    mov     r3, rINST, lsr #12          @ r3<- B
    ubfx    r9, rINST, #8, #4           @ r9<- A
    GET_VREG r1, r3                     @ r1<- vB
    GET_VREG r0, r9                     @ r0<- vA
    cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    FETCH_ADVANCE_INST 1                @ advance rPC, load rINST

#ifdef __ARM_ARCH_EXT_IDIV__
    sdiv    r2, r0, r1
    mls     r1, r1, r2, r0              @ r1<- op
#else
    bl      __aeabi_idivmod             @ r1<- op, r0-r3 changed
#endif
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    SET_VREG r1, r9                     @ vAA<- r1
    GOTO_OPCODE ip                      @ jump to next instruction
    /* 10-13 instructions */


%def op_rem_int_lit16():
    /*
     * Specialized 32-bit binary operation
     *
     * Performs "r1 = r0 rem r1". The selection between sdiv block or the gcc helper
     * depends on the compile time value of __ARM_ARCH_EXT_IDIV__ (defined for
     * ARMv7 CPUs that have hardware division support).
     *
     * NOTE: idivmod returns quotient in r0 and remainder in r1
     *
     * rem-int/lit16
     *
     */
    FETCH_S r1, 1                       @ r1<- ssssCCCC (sign-extended)
    mov     r2, rINST, lsr #12          @ r2<- B
    ubfx    r9, rINST, #8, #4           @ r9<- A
    GET_VREG r0, r2                     @ r0<- vB
    cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    FETCH_ADVANCE_INST 2                @ advance rPC, load rINST

#ifdef __ARM_ARCH_EXT_IDIV__
    sdiv    r2, r0, r1
    mls     r1, r1, r2, r0              @ r1<- op
#else
    bl     __aeabi_idivmod              @ r1<- op, r0-r3 changed
#endif
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    SET_VREG r1, r9                     @ vAA<- r1
    GOTO_OPCODE ip                      @ jump to next instruction
    /* 10-13 instructions */

%def op_rem_int_lit8():
    /*
     * Specialized 32-bit binary operation
     *
     * Performs "r1 = r0 rem r1". The selection between sdiv block or the gcc helper
     * depends on the compile time value of __ARM_ARCH_EXT_IDIV__ (defined for
     * ARMv7 CPUs that have hardware division support).
     *
     * NOTE: idivmod returns quotient in r0 and remainder in r1
     *
     * rem-int/lit8
     *
     */
    FETCH_S r3, 1                       @ r3<- ssssCCBB (sign-extended for CC)
    mov     r9, rINST, lsr #8           @ r9<- AA
    and     r2, r3, #255                @ r2<- BB
    GET_VREG r0, r2                     @ r0<- vBB
    movs    r1, r3, asr #8              @ r1<- ssssssCC (sign extended)
    @cmp     r1, #0                     @ is second operand zero?
    beq     common_errDivideByZero
    FETCH_ADVANCE_INST 2                @ advance rPC, load rINST

#ifdef __ARM_ARCH_EXT_IDIV__
    sdiv    r2, r0, r1
    mls     r1, r1, r2, r0              @ r1<- op
#else
    bl       __aeabi_idivmod            @ r1<- op, r0-r3 changed
#endif
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    SET_VREG r1, r9                     @ vAA<- r1
    GOTO_OPCODE ip                      @ jump to next instruction
    /* 10-12 instructions */

%def op_rem_long():
/* ldivmod returns quotient in r0/r1 and remainder in r2/r3 */
%  binopWide(instr="bl      __aeabi_ldivmod", result0="r2", result1="r3", chkzero="1")

%def op_rem_long_2addr():
/* ldivmod returns quotient in r0/r1 and remainder in r2/r3 */
%  binopWide2addr(instr="bl      __aeabi_ldivmod", result0="r2", result1="r3", chkzero="1")

%def op_rsub_int():
/* this op is "rsub-int", but can be thought of as "rsub-int/lit16" */
%  binopLit16(instr="rsb     r0, r0, r1")

%def op_rsub_int_lit8():
%  binopLit8(extract="", instr="rsb     r0, r0, r3, asr #8")

%def op_shl_int():
%  binop(preinstr="and     r1, r1, #31", instr="mov     r0, r0, asl r1")

%def op_shl_int_2addr():
%  binop2addr(preinstr="and     r1, r1, #31", instr="mov     r0, r0, asl r1")

%def op_shl_int_lit8():
%  binopLit8(extract="ubfx    r1, r3, #8, #5", instr="mov     r0, r0, asl r1")

%def op_shl_long():
    /*
     * Long integer shift.  This is different from the generic 32/64-bit
     * binary operations because vAA/vBB are 64-bit but vCC (the shift
     * distance) is 32-bit.  Also, Dalvik requires us to mask off the low
     * 6 bits of the shift distance.
     */
    /* shl-long vAA, vBB, vCC */
    FETCH r0, 1                         @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    and     r3, r0, #255                @ r3<- BB
    mov     r0, r0, lsr #8              @ r0<- CC
    VREG_INDEX_TO_ADDR r3, r3           @ r3<- &fp[BB]
    GET_VREG r2, r0                     @ r2<- vCC
    GET_VREG_WIDE_BY_ADDR r0, r1, r3    @ r0/r1<- vBB/vBB+1
    CLEAR_SHADOW_PAIR r9, lr, ip        @ Zero out the shadow regs
    and     r2, r2, #63                 @ r2<- r2 & 0x3f
    VREG_INDEX_TO_ADDR r9, r9           @ r9<- &fp[AA]
    mov     r1, r1, asl r2              @ r1<- r1 << r2
    rsb     r3, r2, #32                 @ r3<- 32 - r2
    orr     r1, r1, r0, lsr r3          @ r1<- r1 | (r0 << (32-r2))
    subs    ip, r2, #32                 @ ip<- r2 - 32
    movpl   r1, r0, asl ip              @ if r2 >= 32, r1<- r0 << (r2-32)
    FETCH_ADVANCE_INST 2                @ advance rPC, load rINST
    mov     r0, r0, asl r2              @ r0<- r0 << r2
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    SET_VREG_WIDE_BY_ADDR r0, r1, r9    @ vAA/vAA+1<- r0/r1
    GOTO_OPCODE ip                      @ jump to next instruction

%def op_shl_long_2addr():
    /*
     * Long integer shift, 2addr version.  vA is 64-bit value/result, vB is
     * 32-bit shift distance.
     */
    /* shl-long/2addr vA, vB */
    mov     r3, rINST, lsr #12          @ r3<- B
    ubfx    r9, rINST, #8, #4           @ r9<- A
    GET_VREG r2, r3                     @ r2<- vB
    CLEAR_SHADOW_PAIR r9, lr, ip        @ Zero out the shadow regs
    VREG_INDEX_TO_ADDR r9, r9           @ r9<- &fp[A]
    and     r2, r2, #63                 @ r2<- r2 & 0x3f
    GET_VREG_WIDE_BY_ADDR r0, r1, r9    @ r0/r1<- vAA/vAA+1
    mov     r1, r1, asl r2              @ r1<- r1 << r2
    rsb     r3, r2, #32                 @ r3<- 32 - r2
    orr     r1, r1, r0, lsr r3          @ r1<- r1 | (r0 << (32-r2))
    subs    ip, r2, #32                 @ ip<- r2 - 32
    FETCH_ADVANCE_INST 1                @ advance rPC, load rINST
    movpl   r1, r0, asl ip              @ if r2 >= 32, r1<- r0 << (r2-32)
    mov     r0, r0, asl r2              @ r0<- r0 << r2
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    SET_VREG_WIDE_BY_ADDR r0, r1, r9    @ vAA/vAA+1<- r0/r1
    GOTO_OPCODE ip                      @ jump to next instruction

%def op_shr_int():
%  binop(preinstr="and     r1, r1, #31", instr="mov     r0, r0, asr r1")

%def op_shr_int_2addr():
%  binop2addr(preinstr="and     r1, r1, #31", instr="mov     r0, r0, asr r1")

%def op_shr_int_lit8():
%  binopLit8(extract="ubfx    r1, r3, #8, #5", instr="mov     r0, r0, asr r1")

%def op_shr_long():
    /*
     * Long integer shift.  This is different from the generic 32/64-bit
     * binary operations because vAA/vBB are 64-bit but vCC (the shift
     * distance) is 32-bit.  Also, Dalvik requires us to mask off the low
     * 6 bits of the shift distance.
     */
    /* shr-long vAA, vBB, vCC */
    FETCH r0, 1                         @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    and     r3, r0, #255                @ r3<- BB
    mov     r0, r0, lsr #8              @ r0<- CC
    VREG_INDEX_TO_ADDR r3, r3           @ r3<- &fp[BB]
    GET_VREG r2, r0                     @ r2<- vCC
    GET_VREG_WIDE_BY_ADDR r0, r1, r3    @ r0/r1<- vBB/vBB+1
    CLEAR_SHADOW_PAIR r9, lr, ip        @ Zero out the shadow regs
    and     r2, r2, #63                 @ r0<- r0 & 0x3f
    VREG_INDEX_TO_ADDR r9, r9           @ r9<- &fp[AA]
    mov     r0, r0, lsr r2              @ r0<- r2 >> r2
    rsb     r3, r2, #32                 @ r3<- 32 - r2
    orr     r0, r0, r1, asl r3          @ r0<- r0 | (r1 << (32-r2))
    subs    ip, r2, #32                 @ ip<- r2 - 32
    movpl   r0, r1, asr ip              @ if r2 >= 32, r0<-r1 >> (r2-32)
    FETCH_ADVANCE_INST 2                @ advance rPC, load rINST
    mov     r1, r1, asr r2              @ r1<- r1 >> r2
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    SET_VREG_WIDE_BY_ADDR r0, r1, r9    @ vAA/vAA+1<- r0/r1
    GOTO_OPCODE ip                      @ jump to next instruction

%def op_shr_long_2addr():
    /*
     * Long integer shift, 2addr version.  vA is 64-bit value/result, vB is
     * 32-bit shift distance.
     */
    /* shr-long/2addr vA, vB */
    mov     r3, rINST, lsr #12          @ r3<- B
    ubfx    r9, rINST, #8, #4           @ r9<- A
    GET_VREG r2, r3                     @ r2<- vB
    CLEAR_SHADOW_PAIR r9, lr, ip        @ Zero out the shadow regs
    VREG_INDEX_TO_ADDR r9, r9           @ r9<- &fp[A]
    and     r2, r2, #63                 @ r2<- r2 & 0x3f
    GET_VREG_WIDE_BY_ADDR r0, r1, r9    @ r0/r1<- vAA/vAA+1
    mov     r0, r0, lsr r2              @ r0<- r2 >> r2
    rsb     r3, r2, #32                 @ r3<- 32 - r2
    orr     r0, r0, r1, asl r3          @ r0<- r0 | (r1 << (32-r2))
    subs    ip, r2, #32                 @ ip<- r2 - 32
    FETCH_ADVANCE_INST 1                @ advance rPC, load rINST
    movpl   r0, r1, asr ip              @ if r2 >= 32, r0<-r1 >> (r2-32)
    mov     r1, r1, asr r2              @ r1<- r1 >> r2
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    SET_VREG_WIDE_BY_ADDR r0, r1, r9    @ vAA/vAA+1<- r0/r1
    GOTO_OPCODE ip                      @ jump to next instruction

%def op_sub_int():
%  binop(instr="sub     r0, r0, r1")

%def op_sub_int_2addr():
%  binop2addr(instr="sub     r0, r0, r1")

%def op_sub_long():
%  binopWide(preinstr="subs    r0, r0, r2", instr="sbc     r1, r1, r3")

%def op_sub_long_2addr():
%  binopWide2addr(preinstr="subs    r0, r0, r2", instr="sbc     r1, r1, r3")

%def op_ushr_int():
%  binop(preinstr="and     r1, r1, #31", instr="mov     r0, r0, lsr r1")

%def op_ushr_int_2addr():
%  binop2addr(preinstr="and     r1, r1, #31", instr="mov     r0, r0, lsr r1")

%def op_ushr_int_lit8():
%  binopLit8(extract="ubfx    r1, r3, #8, #5", instr="mov     r0, r0, lsr r1")

%def op_ushr_long():
    /*
     * Long integer shift.  This is different from the generic 32/64-bit
     * binary operations because vAA/vBB are 64-bit but vCC (the shift
     * distance) is 32-bit.  Also, Dalvik requires us to mask off the low
     * 6 bits of the shift distance.
     */
    /* ushr-long vAA, vBB, vCC */
    FETCH r0, 1                         @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    and     r3, r0, #255                @ r3<- BB
    mov     r0, r0, lsr #8              @ r0<- CC
    VREG_INDEX_TO_ADDR r3, r3           @ r3<- &fp[BB]
    GET_VREG r2, r0                     @ r2<- vCC
    GET_VREG_WIDE_BY_ADDR r0, r1, r3    @ r0/r1<- vBB/vBB+1
    CLEAR_SHADOW_PAIR r9, lr, ip        @ Zero out the shadow regs
    and     r2, r2, #63                 @ r0<- r0 & 0x3f
    VREG_INDEX_TO_ADDR r9, r9           @ r9<- &fp[AA]
    mov     r0, r0, lsr r2              @ r0<- r2 >> r2
    rsb     r3, r2, #32                 @ r3<- 32 - r2
    orr     r0, r0, r1, asl r3          @ r0<- r0 | (r1 << (32-r2))
    subs    ip, r2, #32                 @ ip<- r2 - 32
    movpl   r0, r1, lsr ip              @ if r2 >= 32, r0<-r1 >>> (r2-32)
    FETCH_ADVANCE_INST 2                @ advance rPC, load rINST
    mov     r1, r1, lsr r2              @ r1<- r1 >>> r2
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    SET_VREG_WIDE_BY_ADDR r0, r1, r9    @ vAA/vAA+1<- r0/r1
    GOTO_OPCODE ip                      @ jump to next instruction

%def op_ushr_long_2addr():
    /*
     * Long integer shift, 2addr version.  vA is 64-bit value/result, vB is
     * 32-bit shift distance.
     */
    /* ushr-long/2addr vA, vB */
    mov     r3, rINST, lsr #12          @ r3<- B
    ubfx    r9, rINST, #8, #4           @ r9<- A
    GET_VREG r2, r3                     @ r2<- vB
    CLEAR_SHADOW_PAIR r9, lr, ip        @ Zero out the shadow regs
    VREG_INDEX_TO_ADDR r9, r9           @ r9<- &fp[A]
    and     r2, r2, #63                 @ r2<- r2 & 0x3f
    GET_VREG_WIDE_BY_ADDR r0, r1, r9    @ r0/r1<- vAA/vAA+1
    mov     r0, r0, lsr r2              @ r0<- r2 >> r2
    rsb     r3, r2, #32                 @ r3<- 32 - r2
    orr     r0, r0, r1, asl r3          @ r0<- r0 | (r1 << (32-r2))
    subs    ip, r2, #32                 @ ip<- r2 - 32
    FETCH_ADVANCE_INST 1                @ advance rPC, load rINST
    movpl   r0, r1, lsr ip              @ if r2 >= 32, r0<-r1 >>> (r2-32)
    mov     r1, r1, lsr r2              @ r1<- r1 >>> r2
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    SET_VREG_WIDE_BY_ADDR r0, r1, r9    @ vAA/vAA+1<- r0/r1
    GOTO_OPCODE ip                      @ jump to next instruction

%def op_xor_int():
%  binop(instr="eor     r0, r0, r1")

%def op_xor_int_2addr():
%  binop2addr(instr="eor     r0, r0, r1")

%def op_xor_int_lit16():
%  binopLit16(instr="eor     r0, r0, r1")

%def op_xor_int_lit8():
%  binopLit8(extract="", instr="eor     r0, r0, r3, asr #8")

%def op_xor_long():
%  binopWide(preinstr="eor     r0, r0, r2", instr="eor     r1, r1, r3")

%def op_xor_long_2addr():
%  binopWide2addr(preinstr="eor     r0, r0, r2", instr="eor     r1, r1, r3")
