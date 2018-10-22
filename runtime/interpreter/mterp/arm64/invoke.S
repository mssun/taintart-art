%def invoke(helper="UndefinedInvokeHandler"):
    /*
     * Generic invoke handler wrapper.
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    .extern $helper
    EXPORT_PC
    mov     x0, xSELF
    add     x1, xFP, #OFF_FP_SHADOWFRAME
    mov     x2, xPC
    mov     x3, xINST
    bl      $helper
    cbz     w0, MterpException
    FETCH_ADVANCE_INST 3
    ldr     w0, [xSELF, #THREAD_USE_MTERP_OFFSET]
    cbz     w0, MterpFallback
    GET_INST_OPCODE ip
    GOTO_OPCODE ip


%def invoke_polymorphic(helper="UndefinedInvokeHandler"):
    /*
     * invoke-polymorphic handler wrapper.
     */
    /* op {vC, vD, vE, vF, vG}, meth@BBBB, proto@HHHH */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB, proto@HHHH */
    .extern $helper
    EXPORT_PC
    mov     x0, xSELF
    add     x1, xFP, #OFF_FP_SHADOWFRAME
    mov     x2, xPC
    mov     x3, xINST
    bl      $helper
    cbz     w0, MterpException
    FETCH_ADVANCE_INST 4
    ldr     w0, [xSELF, #THREAD_USE_MTERP_OFFSET]
    cbz     w0, MterpFallback
    GET_INST_OPCODE ip
    GOTO_OPCODE ip

%def op_invoke_custom():
%  invoke(helper="MterpInvokeCustom")

%def op_invoke_custom_range():
%  invoke(helper="MterpInvokeCustomRange")

%def op_invoke_direct():
%  invoke(helper="MterpInvokeDirect")

%def op_invoke_direct_range():
%  invoke(helper="MterpInvokeDirectRange")

%def op_invoke_interface():
%  invoke(helper="MterpInvokeInterface")
    /*
     * Handle an interface method call.
     *
     * for: invoke-interface, invoke-interface/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB */

%def op_invoke_interface_range():
%  invoke(helper="MterpInvokeInterfaceRange")

%def op_invoke_polymorphic():
%  invoke_polymorphic(helper="MterpInvokePolymorphic")

%def op_invoke_polymorphic_range():
%  invoke_polymorphic(helper="MterpInvokePolymorphicRange")

%def op_invoke_static():
%  invoke(helper="MterpInvokeStatic")


%def op_invoke_static_range():
%  invoke(helper="MterpInvokeStaticRange")

%def op_invoke_super():
%  invoke(helper="MterpInvokeSuper")
    /*
     * Handle a "super" method call.
     *
     * for: invoke-super, invoke-super/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op vAA, {vCCCC..v(CCCC+AA-1)}, meth@BBBB */

%def op_invoke_super_range():
%  invoke(helper="MterpInvokeSuperRange")

%def op_invoke_virtual():
%  invoke(helper="MterpInvokeVirtual")
    /*
     * Handle a virtual method call.
     *
     * for: invoke-virtual, invoke-virtual/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op vAA, {vCCCC..v(CCCC+AA-1)}, meth@BBBB */

%def op_invoke_virtual_quick():
%  invoke(helper="MterpInvokeVirtualQuick")

%def op_invoke_virtual_range():
%  invoke(helper="MterpInvokeVirtualRange")

%def op_invoke_virtual_range_quick():
%  invoke(helper="MterpInvokeVirtualQuickRange")
