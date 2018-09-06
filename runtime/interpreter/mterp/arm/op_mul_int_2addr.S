%def op_mul_int_2addr():
/* must be "mul r0, r1, r0" -- "r0, r0, r1" is illegal */
%  binop2addr(instr="mul     r0, r1, r0")
