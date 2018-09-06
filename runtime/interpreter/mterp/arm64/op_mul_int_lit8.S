%def op_mul_int_lit8():
/* must be "mul w0, w1, w0" -- "w0, w0, w1" is illegal */
%  binopLit8(instr="mul     w0, w1, w0")
