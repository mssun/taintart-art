%def op_add_long_2addr():
%  binopWide2addr(preinstr="adds    r0, r0, r2", instr="adc     r1, r1, r3")
