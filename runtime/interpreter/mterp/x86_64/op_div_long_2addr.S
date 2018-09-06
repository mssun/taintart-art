%def op_div_long_2addr():
%  bindiv2addr(result="%rax", second="%rcx", wide="1", suffix="q", ext="cqo")
