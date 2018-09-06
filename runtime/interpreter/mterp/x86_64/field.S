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
