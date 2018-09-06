%def op_or_long_2addr():
%  binopWide2addr(instr1="orl     %eax, (rFP,rINST,4)", instr2="orl     %ecx, 4(rFP,rINST,4)")
