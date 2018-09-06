%def op_ushr_int_lit8():
%  binopLit8(extract="ubfx    w1, w3, #8, #5", instr="lsr     w0, w0, w1")
