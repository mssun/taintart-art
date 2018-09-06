%def op_and_long_2addr():
%  binopWide2addr(instr="andq    %rax, (rFP,%rcx,4)")
