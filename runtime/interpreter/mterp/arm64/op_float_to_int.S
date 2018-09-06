%def op_float_to_int():
%  funopNarrow(instr="fcvtzs w0, s0", srcreg="s0", tgtreg="w0")
