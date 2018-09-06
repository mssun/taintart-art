%def op_neg_float():
%  fpcvt(instr="fchs", load="flds", store="fstps")
