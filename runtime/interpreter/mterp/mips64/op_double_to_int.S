%def op_double_to_int():
%  fcvtHeader(suffix="_DOUBLE", valreg="f0")
    trunc.w.d f0, f0
%  fcvtFooter(suffix="_FLOAT", valreg="f0")
