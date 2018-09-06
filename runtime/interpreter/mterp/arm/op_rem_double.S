%def op_rem_double():
/* EABI doesn't define a double remainder function, but libm does */
%  binopWide(instr="bl      fmod")
