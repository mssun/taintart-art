%def op_rem_long_2addr():
%  bindiv2addr(result="%rdx", second="%rcx", wide="1", suffix="q", rem="1", ext="cqo")
