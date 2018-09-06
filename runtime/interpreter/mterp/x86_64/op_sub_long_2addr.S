%def op_sub_long_2addr():
%  binopWide2addr(instr="subq    %rax, (rFP,%rcx,4)")
