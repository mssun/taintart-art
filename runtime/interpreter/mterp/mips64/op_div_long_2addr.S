%def op_div_long_2addr():
%  binopWide2addr(instr="ddiv a0, a0, a1", chkzero="1")
