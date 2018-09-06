%def op_invoke_super():
%  invoke(helper="MterpInvokeSuper")
/*
 * Handle a "super" method call.
 *
 * for: invoke-super, invoke-super/range
 */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op vAA, {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
