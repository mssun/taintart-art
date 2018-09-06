%def field(helper=""):
    /*
     * General field read / write (iget-* iput-* sget-* sput-*).
     */
    .extern $helper
    mov      r0, rPC                       @ arg0: Instruction* inst
    mov      r1, rINST                     @ arg1: uint16_t inst_data
    add      r2, rFP, #OFF_FP_SHADOWFRAME  @ arg2: ShadowFrame* sf
    mov      r3, rSELF                     @ arg3: Thread* self
    PREFETCH_INST 2                        @ prefetch next opcode
    bl       $helper
    cmp      r0, #0
    beq      MterpPossibleException
    ADVANCE 2
    GET_INST_OPCODE ip                     @ extract opcode from rINST
    GOTO_OPCODE ip                         @ jump to next instruction
