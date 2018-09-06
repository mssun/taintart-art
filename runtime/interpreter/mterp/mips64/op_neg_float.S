%def op_neg_float():
%  fcvtHeader(suffix="_FLOAT", valreg="f0")
    neg.s   f0, f0
%  fcvtFooter(suffix="_FLOAT", valreg="f0")
