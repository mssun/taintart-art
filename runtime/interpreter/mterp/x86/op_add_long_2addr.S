%def op_add_long_2addr():
%  binopWide2addr(instr1="addl    %eax, (rFP,rINST,4)", instr2="adcl    %ecx, 4(rFP,rINST,4)")
