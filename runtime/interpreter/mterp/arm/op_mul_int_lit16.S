%def op_mul_int_lit16():
/* must be "mul r0, r1, r0" -- "r0, r0, r1" is illegal */
%  binopLit16(instr="mul     r0, r1, r0")
