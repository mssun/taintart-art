%def op_div_long_2addr():
%  binopWide2addr(instr="sdiv     x0, x0, x1", chkzero="1")
