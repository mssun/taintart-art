%def op_sub_long():
%  binopWide(instr1="subl    (rFP,%ecx,4), rIBASE", instr2="sbbl    4(rFP,%ecx,4), %eax")
