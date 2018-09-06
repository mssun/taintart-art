%def op_mul_int_lit16():
/* must be "mul w0, w1, w0" -- "w0, w0, w1" is illegal */
%  binopLit16(instr="mul     w0, w1, w0")
