%def op_neg_double():
%  fpcvt(instr="fchs", load="fldl", store="fstpl", wide="1")
