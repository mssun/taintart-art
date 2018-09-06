%def op_float_to_long():
%  fcvtHeader(suffix="_FLOAT", valreg="f0")
    trunc.l.s f0, f0
%  fcvtFooter(suffix="_DOUBLE", valreg="f0")
