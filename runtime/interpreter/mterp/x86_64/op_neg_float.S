%def op_neg_float():
%  unop(instr="    xorl    $0x80000000, %eax")
