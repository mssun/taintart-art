    /*
     * SPUT_WIDE handler wrapper.
     *
     */
    /* sput-wide vAA, field//BBBB */
    .extern MterpSPutU64
    EXPORT_PC
    FETCH   w0, 1                       // w0<- field ref BBBB
    lsr     w1, wINST, #8               // w1<- AA
    VREG_INDEX_TO_ADDR x1, w1
    ldr     x2, [xFP, #OFF_FP_METHOD]
    mov     x3, xSELF
    PREFETCH_INST 2                     // Get next inst, but don't advance rPC
    bl      MterpSPutU64
    cbnz    w0, MterpException          // 0 on success, -1 on failure
    ADVANCE 2                           // Past exception point - now advance rPC
    GET_INST_OPCODE ip                  // extract opcode from wINST
    GOTO_OPCODE ip                      // jump to next instruction
