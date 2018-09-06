%def op_sub_long_2addr():
/*
 * See op_sub_long.S for more details
 */
%  binopWide2addr(result0="v0", result1="v1", preinstr="subu v0, a0, a2", instr="subu v1, a1, a3; sltu a0, a0, v0; subu v1, v1, a0")
