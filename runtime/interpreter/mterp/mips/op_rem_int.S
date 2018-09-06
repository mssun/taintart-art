%def op_rem_int():
#ifdef MIPS32REVGE6
%  binop(instr="mod a0, a0, a1", chkzero="1")
#else
%  binop(preinstr="div zero, a0, a1", instr="mfhi a0", chkzero="1")
#endif
