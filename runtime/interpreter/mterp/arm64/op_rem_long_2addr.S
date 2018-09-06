%def op_rem_long_2addr():
%  binopWide2addr(preinstr="sdiv x3, x0, x1", instr="msub x0, x3, x1, x0", chkzero="1")
