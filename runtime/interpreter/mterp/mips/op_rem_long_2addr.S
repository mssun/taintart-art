%def op_rem_long_2addr():
%  binopWide2addr(result0="v0", result1="v1", instr="JAL(__moddi3)", chkzero="1")
