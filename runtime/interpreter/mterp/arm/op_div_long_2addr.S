%def op_div_long_2addr():
%  binopWide2addr(instr="bl      __aeabi_ldivmod", chkzero="1")
