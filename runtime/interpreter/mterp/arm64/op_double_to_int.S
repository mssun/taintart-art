%def op_double_to_int():
%  funopNarrower(instr="fcvtzs w0, d0", srcreg="d0", tgtreg="w0")
