%def op_float_to_long():
%  cvtfp_int(fp_suffix="s", i_suffix="q", max_const="$0x7fffffffffffffff", result_reg="%rax", wide="1")
