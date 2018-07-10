/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    ldr    rIBASE, [rSELF, #THREAD_CURRENT_IBASE_OFFSET]            @ refresh IBASE.
    adr    lr, .L_ALT_${opcode}
    sub    lr, lr, #(.L_ALT_${opcode} - .L_${opcode})               @ Addr of primary handler.
    mov    r0, rSELF
    add    r1, rFP, #OFF_FP_SHADOWFRAME
    mov    r2, rPC
    b      MterpCheckBefore     @ (self, shadow_frame, dex_pc_ptr)  @ Tail call.
