%def op_xor_int_2addr():
%  binop2addr(instr="xorl    %eax, (rFP,%ecx,4)")
