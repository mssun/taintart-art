%def op_mul_int():
/* must be "mul w0, w1, w0" -- "w0, w0, w1" is illegal */
%  binop(instr="mul     w0, w1, w0")
