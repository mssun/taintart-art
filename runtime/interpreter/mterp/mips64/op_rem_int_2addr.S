%def op_rem_int_2addr():
%  binop2addr(instr="mod a0, a0, a1", chkzero="1")
