%def op_sub_long_2addr():
%  binopWide2addr(instr1="subl    %eax, (rFP,rINST,4)", instr2="sbbl    %ecx, 4(rFP,rINST,4)")
