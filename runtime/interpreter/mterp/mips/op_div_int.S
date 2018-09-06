%def op_div_int():
#ifdef MIPS32REVGE6
%  binop(instr="div a0, a0, a1", chkzero="1")
#else
%  binop(preinstr="div zero, a0, a1", instr="mflo a0", chkzero="1")
#endif
