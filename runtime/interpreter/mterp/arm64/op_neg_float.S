%def op_neg_float():
%  unop(instr="eor     w0, w0, #0x80000000")
