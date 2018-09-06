%def invoke(helper="UndefinedInvokeHandler"):
    /*
     * Generic invoke handler wrapper.
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    .extern $helper
    EXPORT_PC()
    move    a0, rSELF
    addu    a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    move    a3, rINST
    JAL($helper)
    beqz    v0, MterpException
    FETCH_ADVANCE_INST(3)
    JAL(MterpShouldSwitchInterpreters)
    bnez    v0, MterpFallback
    GET_INST_OPCODE(t0)
    GOTO_OPCODE(t0)

%def invoke_polymorphic(helper="UndefinedInvokeHandler"):
    /*
     * invoke-polymorphic handler wrapper.
     */
    /* op {vC, vD, vE, vF, vG}, meth@BBBB, proto@HHHH */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB, proto@HHHH */
    .extern $helper
    EXPORT_PC()
    move    a0, rSELF
    addu    a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    move    a3, rINST
    JAL($helper)
    beqz    v0, MterpException
    FETCH_ADVANCE_INST(4)
    JAL(MterpShouldSwitchInterpreters)
    bnez    v0, MterpFallback
    GET_INST_OPCODE(t0)
    GOTO_OPCODE(t0)

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

%def op_invoke_super_range():
%  invoke(helper="MterpInvokeSuperRange")

%def op_invoke_virtual():
%  invoke(helper="MterpInvokeVirtual")

%def op_invoke_virtual_quick():
%  invoke(helper="MterpInvokeVirtualQuick")

%def op_invoke_virtual_range():
%  invoke(helper="MterpInvokeVirtualRange")

%def op_invoke_virtual_range_quick():
%  invoke(helper="MterpInvokeVirtualQuickRange")
