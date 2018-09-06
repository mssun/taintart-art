%def op_and_long():
%  binopWide(instr1="andl    (rFP,%ecx,4), rIBASE", instr2="andl    4(rFP,%ecx,4), %eax")
