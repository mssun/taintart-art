%def op_xor_long_2addr():
%  binopWide2addr(preinstr="eor     r0, r0, r2", instr="eor     r1, r1, r3")
