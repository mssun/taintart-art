%def op_rem_long():
%  binopWide(preinstr="sdiv x3, x1, x2", instr="msub x0, x3, x2, x1", chkzero="1")
