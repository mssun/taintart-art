%def op_or_long():
%  binopWide(instr1="orl     (rFP,%ecx,4), rIBASE", instr2="orl     4(rFP,%ecx,4), %eax")
