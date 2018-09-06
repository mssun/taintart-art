%def op_xor_long_2addr():
%  binopWide2addr(preinstr="xor a0, a0, a2", instr="xor a1, a1, a3")
