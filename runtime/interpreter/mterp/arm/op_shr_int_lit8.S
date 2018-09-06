%def op_shr_int_lit8():
%  binopLit8(extract="ubfx    r1, r3, #8, #5", instr="mov     r0, r0, asr r1")
