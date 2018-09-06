%def op_sub_long_2addr():
%  binopWide2addr(preinstr="subs    r0, r0, r2", instr="sbc     r1, r1, r3")
