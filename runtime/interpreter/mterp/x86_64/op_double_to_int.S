%def op_double_to_int():
%  cvtfp_int(fp_suffix="d", i_suffix="l", max_const="$0x7fffffff", result_reg="%eax", wide="0")
