%def op_neg_double():
%  fcvtHeader(suffix="_DOUBLE", valreg="f0")
    neg.d   f0, f0
%  fcvtFooter(suffix="_DOUBLE", valreg="f0")
