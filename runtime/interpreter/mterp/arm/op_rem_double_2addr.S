%def op_rem_double_2addr():
/* EABI doesn't define a double remainder function, but libm does */
%  binopWide2addr(instr="bl      fmod")
