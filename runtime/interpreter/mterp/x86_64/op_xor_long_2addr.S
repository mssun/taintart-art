%def op_xor_long_2addr():
%  binopWide2addr(instr="xorq    %rax, (rFP,%rcx,4)")
