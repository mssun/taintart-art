%def op_div_long_2addr():
%  binopWide2addr(result0="v0", result1="v1", instr="JAL(__divdi3)", chkzero="1")
