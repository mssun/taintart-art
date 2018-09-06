%def op_div_int_2addr():
%  binop2addr(instr="sdiv     w0, w0, w1", chkzero="1")
