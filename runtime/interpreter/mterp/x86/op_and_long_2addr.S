%def op_and_long_2addr():
%  binopWide2addr(instr1="andl    %eax, (rFP,rINST,4)", instr2="andl    %ecx, 4(rFP,rINST,4)")
