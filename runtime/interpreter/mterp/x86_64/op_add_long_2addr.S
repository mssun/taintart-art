%def op_add_long_2addr():
%  binopWide2addr(instr="addq    %rax, (rFP,%rcx,4)")
