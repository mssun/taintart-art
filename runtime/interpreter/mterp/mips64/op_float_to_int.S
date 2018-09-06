%def op_float_to_int():
%  fcvtHeader(suffix="_FLOAT", valreg="f0")
    trunc.w.s f0, f0
%  fcvtFooter(suffix="_FLOAT", valreg="f0")
