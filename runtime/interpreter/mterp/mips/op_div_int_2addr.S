%def op_div_int_2addr():
#ifdef MIPS32REVGE6
%  binop2addr(instr="div a0, a0, a1", chkzero="1")
#else
%  binop2addr(preinstr="div zero, a0, a1", instr="mflo a0", chkzero="1")
#endif
