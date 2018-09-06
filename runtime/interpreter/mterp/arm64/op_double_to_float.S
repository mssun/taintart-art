%def op_double_to_float():
%  funopNarrower(instr="fcvt s0, d0", srcreg="d0", tgtreg="s0")
