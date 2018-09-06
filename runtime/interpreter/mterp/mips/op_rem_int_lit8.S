%def op_rem_int_lit8():
#ifdef MIPS32REVGE6
%  binopLit8(instr="mod a0, a0, a1", chkzero="1")
#else
%  binopLit8(preinstr="div zero, a0, a1", instr="mfhi a0", chkzero="1")
#endif
