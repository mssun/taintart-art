%def field(helper=""):
    /*
     * General field read / write (iget-* iput-* sget-* sput-*).
     */
    .extern $helper
    REFRESH_INST ${opnum}                      # fix rINST to include opcode
    movq    rPC, OUT_ARG0                      # arg0: Instruction* inst
    movl    rINST, OUT_32_ARG1                 # arg1: uint16_t inst_data
    leaq    OFF_FP_SHADOWFRAME(rFP), OUT_ARG2  # arg2: ShadowFrame* sf
    movq    rSELF, OUT_ARG3                    # arg3: Thread* self
    call    SYMBOL($helper)
    testb   %al, %al
    jz      MterpPossibleException
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def op_check_cast():
/*
 * Check to see if a cast from one class to another is allowed.
 */
    /* check-cast vAA, class@BBBB */
    EXPORT_PC
    movzwq  2(rPC), OUT_ARG0                # OUT_ARG0 <- BBBB
    leaq    VREG_ADDRESS(rINSTq), OUT_ARG1
    movq    OFF_FP_METHOD(rFP), OUT_ARG2
    movq    rSELF, OUT_ARG3
    call    SYMBOL(MterpCheckCast)          # (index, &obj, method, self)
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
    .extern artIGetObjectFromMterp
    movzbq  rINSTbl, %rcx                   # rcx <- BA
    sarl    $$4, %ecx                       # ecx <- B
    GET_VREG OUT_32_ARG0, %rcx              # vB (object we're operating on)
    movzwl  2(rPC), OUT_32_ARG1             # eax <- field byte offset
    EXPORT_PC
    callq   SYMBOL(artIGetObjectFromMterp)  # (obj, offset)
    movq    rSELF, %rcx
    cmpq    $$0, THREAD_EXCEPTION_OFFSET(%rcx)
    jnz     MterpException                  # bail out
    andb    $$0xf, rINSTbl                  # rINST <- A
    SET_VREG_OBJECT %eax, rINSTq            # fp[A] <- value
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def op_iget_quick(load="movl", wide="0"):
    /* For: iget-quick, iget-boolean-quick, iget-byte-quick, iget-char-quick, iget-short-quick, iget-wide-quick */
    /* op vA, vB, offset@CCCC */
    movl    rINST, %ecx                     # rcx <- BA
    sarl    $$4, %ecx                       # ecx <- B
    GET_VREG %ecx, %rcx                     # vB (object we're operating on)
    movzwq  2(rPC), %rax                    # eax <- field byte offset
    testl   %ecx, %ecx                      # is object null?
    je      common_errNullObject
    andb    $$0xf,rINSTbl                   # rINST <- A
    .if $wide
    movq (%rcx,%rax,1), %rax
    SET_WIDE_VREG %rax, rINSTq              # fp[A] <- value
    .else
    ${load} (%rcx,%rax,1), %eax
    SET_VREG %eax, rINSTq                   # fp[A] <- value
    .endif
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def op_iget_short():
%  op_iget(helper="MterpIGetI16")

%def op_iget_short_quick():
%  op_iget_quick(load="movswl")

%def op_iget_wide():
%  op_iget(helper="MterpIGetU64")

%def op_iget_wide_quick():
%  op_iget_quick(load="movswl", wide="1")

%def op_instance_of():
/*
 * Check to see if an object reference is an instance of a class.
 *
 * Most common situation is a non-null object, being compared against
 * an already-resolved class.
 */
    /* instance-of vA, vB, class@CCCC */
    EXPORT_PC
    movzwl  2(rPC), OUT_32_ARG0             # OUT_32_ARG0 <- CCCC
    movl    rINST, %eax                     # eax <- BA
    sarl    $$4, %eax                       # eax <- B
    leaq    VREG_ADDRESS(%rax), OUT_ARG1    # Get object address
    movq    OFF_FP_METHOD(rFP), OUT_ARG2
    movq    rSELF, OUT_ARG3
    call    SYMBOL(MterpInstanceOf)         # (index, &obj, method, self)
    movsbl  %al, %eax
    movq    rSELF, %rcx
    cmpq    $$0, THREAD_EXCEPTION_OFFSET(%rcx)
    jnz     MterpException
    andb    $$0xf, rINSTbl                  # rINSTbl <- A
    SET_VREG %eax, rINSTq
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
    leaq    OFF_FP_SHADOWFRAME(rFP), OUT_ARG0
    movq    rPC, OUT_ARG1
    REFRESH_INST ${opnum}
    movl    rINST, OUT_32_ARG2
    call    SYMBOL(MterpIputObjectQuick)
    testb   %al, %al
    jz      MterpException
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def op_iput_quick(reg="rINST", store="movl"):
    /* For: iput-quick, iput-object-quick */
    /* op vA, vB, offset@CCCC */
    movzbq  rINSTbl, %rcx                   # rcx <- BA
    sarl    $$4, %ecx                       # ecx <- B
    GET_VREG %ecx, %rcx                     # vB (object we're operating on)
    testl   %ecx, %ecx                      # is object null?
    je      common_errNullObject
    andb    $$0xf, rINSTbl                  # rINST <- A
    GET_VREG rINST, rINSTq                  # rINST <- v[A]
    movzwq  2(rPC), %rax                    # rax <- field byte offset
    ${store}    ${reg}, (%rcx,%rax,1)
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def op_iput_short():
%  op_iput(helper="MterpIPutI16")

%def op_iput_short_quick():
%  op_iput_quick(reg="rINSTw", store="movw")

%def op_iput_wide():
%  op_iput(helper="MterpIPutU64")

%def op_iput_wide_quick():
    /* iput-wide-quick vA, vB, offset@CCCC */
    movzbq    rINSTbl, %rcx                 # rcx<- BA
    sarl      $$4, %ecx                     # ecx<- B
    GET_VREG  %ecx, %rcx                    # vB (object we're operating on)
    testl     %ecx, %ecx                    # is object null?
    je        common_errNullObject
    movzwq    2(rPC), %rax                  # rax<- field byte offset
    leaq      (%rcx,%rax,1), %rcx           # ecx<- Address of 64-bit target
    andb      $$0xf, rINSTbl                # rINST<- A
    GET_WIDE_VREG %rax, rINSTq              # rax<- fp[A]/fp[A+1]
    movq      %rax, (%rcx)                  # obj.field<- r0/r1
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def op_new_instance():
/*
 * Create a new instance of a class.
 */
    /* new-instance vAA, class@BBBB */
    EXPORT_PC
    leaq    OFF_FP_SHADOWFRAME(rFP), OUT_ARG0
    movq    rSELF, OUT_ARG1
    REFRESH_INST ${opnum}
    movq    rINSTq, OUT_ARG2
    call    SYMBOL(MterpNewInstance)
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
