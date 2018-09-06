%def op_shr_int_lit8():
%  binopLit8(extract="ubfx    w1, w3, #8, #5", instr="asr     w0, w0, w1")
