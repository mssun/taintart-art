%def op_neg_double():
%  unop(preinstr="    movq    $0x8000000000000000, %rsi", instr="    xorq    %rsi, %rax", wide="1")
