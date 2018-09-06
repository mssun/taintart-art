%def op_div_long():
%  binopWide(instr="bl      __aeabi_ldivmod", chkzero="1")
