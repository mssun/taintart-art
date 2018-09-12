%def field(helper=""):
    /*
     * General field read / write (iget-* iput-* sget-* sput-*).
     */
    .extern $helper
    REFRESH_INST ${opnum}                   # fix rINST to include opcode
    movl    rPC, OUT_ARG0(%esp)             # arg0: Instruction* inst
    movl    rINST, OUT_ARG1(%esp)           # arg1: uint16_t inst_data
    leal    OFF_FP_SHADOWFRAME(rFP), %eax
    movl    %eax, OUT_ARG2(%esp)            # arg2: ShadowFrame* sf
    movl    rSELF, %eax
    movl    %eax, OUT_ARG3(%esp)            # arg3: Thread* self
    call    SYMBOL($helper)
    testb   %al, %al
    jz      MterpPossibleException
    RESTORE_IBASE
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def op_check_cast():
/*
 * Check to see if a cast from one class to another is allowed.
 */
    /* check-cast vAA, class@BBBB */
    EXPORT_PC
    movzwl  2(rPC), %eax                    # eax <- BBBB
    movl    %eax, OUT_ARG0(%esp)
    leal    VREG_ADDRESS(rINST), %ecx
    movl    %ecx, OUT_ARG1(%esp)
    movl    OFF_FP_METHOD(rFP),%eax
    movl    %eax, OUT_ARG2(%esp)
    movl    rSELF, %ecx
    movl    %ecx, OUT_ARG3(%esp)
    call    SYMBOL(MterpCheckCast)          # (index, &obj, method, self)
    RESTORE_IBASE
    testb   %al, %al
    jnz     MterpPossibleException
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def op_iget(is_object="0", helper="MterpIGetU32"):
%  field(helper=helper)

%def op_iget_boolean():
%  op_iget(helper="MterpIGetU8")

%def op_iget_boolean_quick():
%  op_iget_quick(load="movsbl")

%def op_iget_byte():
%  op_iget(helper="MterpIGetI8")

%def op_iget_byte_quick():
%  op_iget_quick(load="movsbl")

%def op_iget_char():
%  op_iget(helper="MterpIGetU16")

%def op_iget_char_quick():
%  op_iget_quick(load="movzwl")

%def op_iget_object():
%  op_iget(is_object="1", helper="MterpIGetObj")

%def op_iget_object_quick():
    /* For: iget-object-quick */
    /* op vA, vB, offset@CCCC */
    movzbl  rINSTbl, %ecx                   # ecx <- BA
    sarl    $$4, %ecx                       # ecx <- B
    GET_VREG %ecx, %ecx                     # vB (object we're operating on)
    movzwl  2(rPC), %eax                    # eax <- field byte offset
    movl    %ecx, OUT_ARG0(%esp)
    movl    %eax, OUT_ARG1(%esp)
    EXPORT_PC
    call    SYMBOL(artIGetObjectFromMterp)  # (obj, offset)
    movl    rSELF, %ecx
    RESTORE_IBASE_FROM_SELF %ecx
    cmpl    $$0, THREAD_EXCEPTION_OFFSET(%ecx)
    jnz     MterpException                  # bail out
    andb    $$0xf,rINSTbl                   # rINST <- A
    SET_VREG_OBJECT %eax, rINST             # fp[A] <- value
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def op_iget_quick(load="movl"):
    /* For: iget-quick, iget-boolean-quick, iget-byte-quick, iget-char-quick, iget-short-quick */
    /* op vA, vB, offset@CCCC */
    movzbl  rINSTbl, %ecx                   # ecx <- BA
    sarl    $$4, %ecx                       # ecx <- B
    GET_VREG %ecx, %ecx                     # vB (object we're operating on)
    movzwl  2(rPC), %eax                    # eax <- field byte offset
    testl   %ecx, %ecx                      # is object null?
    je      common_errNullObject
    ${load} (%ecx,%eax,1), %eax
    andb    $$0xf,rINSTbl                   # rINST <- A
    SET_VREG %eax, rINST                    # fp[A] <- value
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def op_iget_short():
%  op_iget(helper="MterpIGetI16")

%def op_iget_short_quick():
%  op_iget_quick(load="movswl")

%def op_iget_wide():
%  op_iget(helper="MterpIGetU64")

%def op_iget_wide_quick():
    /* iget-wide-quick vA, vB, offset@CCCC */
    movzbl  rINSTbl, %ecx                   # ecx <- BA
    sarl    $$4, %ecx                       # ecx <- B
    GET_VREG %ecx, %ecx                     # vB (object we're operating on)
    movzwl  2(rPC), %eax                    # eax <- field byte offset
    testl   %ecx, %ecx                      # is object null?
    je      common_errNullObject
    movq    (%ecx,%eax,1), %xmm0
    andb    $$0xf, rINSTbl                  # rINST <- A
    SET_WIDE_FP_VREG %xmm0, rINST
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def op_instance_of():
/*
 * Check to see if an object reference is an instance of a class.
 *
 * Most common situation is a non-null object, being compared against
 * an already-resolved class.
 */
    /* instance-of vA, vB, class@CCCC */
    EXPORT_PC
    movzwl  2(rPC), %eax                    # eax <- BBBB
    movl    %eax, OUT_ARG0(%esp)
    movl    rINST, %eax                     # eax <- BA
    sarl    $$4, %eax                       # eax <- B
    leal    VREG_ADDRESS(%eax), %ecx        # Get object address
    movl    %ecx, OUT_ARG1(%esp)
    movl    OFF_FP_METHOD(rFP),%eax
    movl    %eax, OUT_ARG2(%esp)
    movl    rSELF, %ecx
    movl    %ecx, OUT_ARG3(%esp)
    call    SYMBOL(MterpInstanceOf)         # (index, &obj, method, self)
    movl    rSELF, %ecx
    RESTORE_IBASE_FROM_SELF %ecx
    cmpl    $$0, THREAD_EXCEPTION_OFFSET(%ecx)
    jnz     MterpException
    andb    $$0xf, rINSTbl                  # rINSTbl <- A
    SET_VREG %eax, rINST
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def op_iput(is_object="0", helper="MterpIPutU32"):
%  field(helper=helper)

%def op_iput_boolean():
%  op_iput(helper="MterpIPutU8")

%def op_iput_boolean_quick():
%  op_iput_quick(reg="rINSTbl", store="movb")

%def op_iput_byte():
%  op_iput(helper="MterpIPutI8")

%def op_iput_byte_quick():
%  op_iput_quick(reg="rINSTbl", store="movb")

%def op_iput_char():
%  op_iput(helper="MterpIPutU16")

%def op_iput_char_quick():
%  op_iput_quick(reg="rINSTw", store="movw")

%def op_iput_object():
%  op_iput(is_object="1", helper="MterpIPutObj")

%def op_iput_object_quick():
    EXPORT_PC
    leal    OFF_FP_SHADOWFRAME(rFP), %eax
    movl    %eax, OUT_ARG0(%esp)
    movl    rPC, OUT_ARG1(%esp)
    REFRESH_INST ${opnum}
    movl    rINST, OUT_ARG2(%esp)
    call    SYMBOL(MterpIputObjectQuick)
    testb   %al, %al
    jz      MterpException
    RESTORE_IBASE
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def op_iput_quick(reg="rINST", store="movl"):
    /* For: iput-quick, iput-object-quick */
    /* op vA, vB, offset@CCCC */
    movzbl  rINSTbl, %ecx                   # ecx <- BA
    sarl    $$4, %ecx                       # ecx <- B
    GET_VREG %ecx, %ecx                     # vB (object we're operating on)
    testl   %ecx, %ecx                      # is object null?
    je      common_errNullObject
    andb    $$0xf, rINSTbl                  # rINST <- A
    GET_VREG rINST, rINST                   # rINST <- v[A]
    movzwl  2(rPC), %eax                    # eax <- field byte offset
    ${store}    ${reg}, (%ecx,%eax,1)
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def op_iput_short():
%  op_iput(helper="MterpIPutI16")

%def op_iput_short_quick():
%  op_iput_quick(reg="rINSTw", store="movw")

%def op_iput_wide():
%  op_iput(helper="MterpIPutU64")

%def op_iput_wide_quick():
    /* iput-wide-quick vA, vB, offset@CCCC */
    movzbl    rINSTbl, %ecx                 # ecx<- BA
    sarl      $$4, %ecx                     # ecx<- B
    GET_VREG  %ecx, %ecx                    # vB (object we're operating on)
    testl     %ecx, %ecx                    # is object null?
    je        common_errNullObject
    movzwl    2(rPC), %eax                  # eax<- field byte offset
    leal      (%ecx,%eax,1), %ecx           # ecx<- Address of 64-bit target
    andb      $$0xf, rINSTbl                # rINST<- A
    GET_WIDE_FP_VREG %xmm0, rINST           # xmm0<- fp[A]/fp[A+1]
    movq      %xmm0, (%ecx)                 # obj.field<- r0/r1
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def op_new_instance():
/*
 * Create a new instance of a class.
 */
    /* new-instance vAA, class@BBBB */
    EXPORT_PC
    leal    OFF_FP_SHADOWFRAME(rFP), %eax
    movl    %eax, OUT_ARG0(%esp)
    movl    rSELF, %ecx
    movl    %ecx, OUT_ARG1(%esp)
    REFRESH_INST ${opnum}
    movl    rINST, OUT_ARG2(%esp)
    call    SYMBOL(MterpNewInstance)
    RESTORE_IBASE
    testb   %al, %al                        # 0 means an exception is thrown
    jz      MterpPossibleException
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def op_sget(is_object="0", helper="MterpSGetU32"):
%  field(helper=helper)

%def op_sget_boolean():
%  op_sget(helper="MterpSGetU8")

%def op_sget_byte():
%  op_sget(helper="MterpSGetI8")

%def op_sget_char():
%  op_sget(helper="MterpSGetU16")

%def op_sget_object():
%  op_sget(is_object="1", helper="MterpSGetObj")

%def op_sget_short():
%  op_sget(helper="MterpSGetI16")

%def op_sget_wide():
%  op_sget(helper="MterpSGetU64")

%def op_sput(is_object="0", helper="MterpSPutU32"):
%  field(helper=helper)

%def op_sput_boolean():
%  op_sput(helper="MterpSPutU8")

%def op_sput_byte():
%  op_sput(helper="MterpSPutI8")

%def op_sput_char():
%  op_sput(helper="MterpSPutU16")

%def op_sput_object():
%  op_sput(is_object="1", helper="MterpSPutObj")

%def op_sput_short():
%  op_sput(helper="MterpSPutI16")

%def op_sput_wide():
%  op_sput(helper="MterpSPutU64")
