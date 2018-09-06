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
