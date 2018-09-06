%def op_shr_int_2addr():
%  binop2addr(preinstr="and     r1, r1, #31", instr="mov     r0, r0, asr r1")
