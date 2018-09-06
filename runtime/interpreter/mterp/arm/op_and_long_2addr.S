%def op_and_long_2addr():
%  binopWide2addr(preinstr="and     r0, r0, r2", instr="and     r1, r1, r3")
