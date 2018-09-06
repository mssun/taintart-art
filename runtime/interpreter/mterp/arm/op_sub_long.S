%def op_sub_long():
%  binopWide(preinstr="subs    r0, r0, r2", instr="sbc     r1, r1, r3")
