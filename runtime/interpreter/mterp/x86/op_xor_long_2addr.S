%def op_xor_long_2addr():
%  binopWide2addr(instr1="xorl    %eax, (rFP,rINST,4)", instr2="xorl    %ecx, 4(rFP,rINST,4)")
