%def op_int_to_float():
    /*
     * Conversion from or to floating-point happens in a floating-point register.
     * Therefore we load the input and store the output into or from a
     * floating-point register irrespective of the type.
     */
%  fcvtHeader(suffix="_FLOAT", valreg="f0")
    cvt.s.w f0, f0
%  fcvtFooter(suffix="_FLOAT", valreg="f0")
