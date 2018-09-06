%def op_rem_int_2addr():
#ifdef MIPS32REVGE6
%  binop2addr(instr="mod a0, a0, a1", chkzero="1")
#else
%  binop2addr(preinstr="div zero, a0, a1", instr="mfhi a0", chkzero="1")
#endif
