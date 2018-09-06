%def op_rem_long_2addr():
%  binopWide2addr(instr="dmod a0, a0, a1", chkzero="1")
