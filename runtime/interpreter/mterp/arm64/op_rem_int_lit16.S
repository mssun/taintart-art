%def op_rem_int_lit16():
%  binopLit16(preinstr="sdiv w3, w0, w1", instr="msub w0, w3, w1, w0", chkzero="1")
