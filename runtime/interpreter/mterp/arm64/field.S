%def field(helper=""):
    /*
     * General field read / write (iget-* iput-* sget-* sput-*).
     */
    .extern $helper
    mov      x0, xPC                       // arg0: Instruction* inst
    mov      x1, xINST                     // arg1: uint16_t inst_data
    add      x2, xFP, #OFF_FP_SHADOWFRAME  // arg2: ShadowFrame* sf
    mov      x3, xSELF                     // arg3: Thread* self
    PREFETCH_INST 2                        // prefetch next opcode
    bl       $helper
    cbz      x0, MterpPossibleException
    ADVANCE 2
    GET_INST_OPCODE ip                     // extract opcode from rINST
    GOTO_OPCODE ip                         // jump to next instruction
