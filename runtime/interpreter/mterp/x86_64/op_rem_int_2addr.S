%def op_rem_int_2addr():
%  bindiv2addr(result="%edx", second="%ecx", wide="0", suffix="l", rem="1")
