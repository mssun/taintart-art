%def op_mul_double_2addr():
%  binopWide2addr(instr="fmul     d0, d0, d1", r0="d0", r1="d1")
