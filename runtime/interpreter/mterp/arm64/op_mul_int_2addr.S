%def op_mul_int_2addr():
/* must be "mul w0, w1, w0" -- "w0, w0, w1" is illegal */
%  binop2addr(instr="mul     w0, w1, w0")
