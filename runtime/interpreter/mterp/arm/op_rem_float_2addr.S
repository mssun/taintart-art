%def op_rem_float_2addr():
/* EABI doesn't define a float remainder function, but libm does */
%  binop2addr(instr="bl      fmodf")
