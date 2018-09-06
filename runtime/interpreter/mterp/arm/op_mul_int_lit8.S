%def op_mul_int_lit8():
/* must be "mul r0, r1, r0" -- "r0, r0, r1" is illegal */
%  binopLit8(instr="mul     r0, r1, r0")
