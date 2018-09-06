%def op_double_to_long():
%  fcvtHeader(suffix="_DOUBLE", valreg="f0")
    trunc.l.d f0, f0
%  fcvtFooter(suffix="_DOUBLE", valreg="f0")
