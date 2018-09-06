%def op_sub_double_2addr():
%  binopWide2addr(instr="fsub     d0, d0, d1", r0="d0", r1="d1")
