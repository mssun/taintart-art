%def op_rem_int_lit16():
#ifdef MIPS32REVGE6
%  binopLit16(instr="mod a0, a0, a1", chkzero="1")
#else
%  binopLit16(preinstr="div zero, a0, a1", instr="mfhi a0", chkzero="1")
#endif
