%def op_mul_int():
/* must be "mul r0, r1, r0" -- "r0, r0, r1" is illegal */
%  binop(instr="mul     r0, r1, r0")
