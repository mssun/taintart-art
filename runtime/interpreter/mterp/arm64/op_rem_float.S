%def op_rem_float():
/* EABI doesn't define a float remainder function, but libm does */
%  fbinop(instr="bl      fmodf")
