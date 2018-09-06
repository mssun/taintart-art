%def op_rem_long():
%  bindiv(result="%rdx", second="%rcx", wide="1", suffix="q", ext="cqo", rem="1")
