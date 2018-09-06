%def op_div_double_2addr():
%  binopWide2addr(instr="fdiv     d0, d0, d1", r0="d0", r1="d1")
