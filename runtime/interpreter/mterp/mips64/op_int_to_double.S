%def op_int_to_double():
    /*
     * Conversion from or to floating-point happens in a floating-point register.
     * Therefore we load the input and store the output into or from a
     * floating-point register irrespective of the type.
     */
%  fcvtHeader(suffix="_FLOAT", valreg="f0")
    cvt.d.w f0, f0
%  fcvtFooter(suffix="_DOUBLE", valreg="f0")
