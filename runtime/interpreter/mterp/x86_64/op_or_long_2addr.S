%def op_or_long_2addr():
%  binopWide2addr(instr="orq     %rax, (rFP,%rcx,4)")
