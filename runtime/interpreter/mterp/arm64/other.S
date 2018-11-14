%def const(helper="UndefinedConstHandler"):
    /* const/class vAA, type@BBBB */
    /* const/method-handle vAA, method_handle@BBBB */
    /* const/method-type vAA, proto@BBBB */
    /* const/string vAA, string@@BBBB */
    .extern $helper
    EXPORT_PC
    FETCH w0, 1                         // w0<- BBBB
    lsr     w1, wINST, #8               // w1<- AA
    add     x2, xFP, #OFF_FP_SHADOWFRAME
    mov     x3, xSELF
    bl      $helper                     // (index, tgt_reg, shadow_frame, self)
    PREFETCH_INST 2                     // load rINST
    cbnz    w0, MterpPossibleException  // let reference interpreter deal with it.
    ADVANCE 2                           // advance rPC
    GET_INST_OPCODE ip                  // extract opcode from rINST
    GOTO_OPCODE ip                      // jump to next instruction

%def unused():
/*
 * Bail to reference interpreter to throw.
 */
  b MterpFallback

%def op_const():
    /* const vAA, #+BBBBbbbb */
    lsr     w3, wINST, #8               // w3<- AA
    FETCH w0, 1                         // w0<- bbbb (low
    FETCH w1, 2                         // w1<- BBBB (high
    FETCH_ADVANCE_INST 3                // advance rPC, load wINST
    orr     w0, w0, w1, lsl #16         // w0<- BBBBbbbb
    GET_INST_OPCODE ip                  // extract opcode from wINST
    SET_VREG w0, w3                     // vAA<- w0
    GOTO_OPCODE ip                      // jump to next instruction

%def op_const_16():
    /* const/16 vAA, #+BBBB */
    FETCH_S w0, 1                       // w0<- ssssBBBB (sign-extended)
    lsr     w3, wINST, #8               // w3<- AA
    FETCH_ADVANCE_INST 2                // advance xPC, load wINST
    SET_VREG w0, w3                     // vAA<- w0
    GET_INST_OPCODE ip                  // extract opcode from wINST
    GOTO_OPCODE ip                      // jump to next instruction

%def op_const_4():
    /* const/4 vA, #+B */
    sbfx    w1, wINST, #12, #4          // w1<- sssssssB
    ubfx    w0, wINST, #8, #4           // w0<- A
    FETCH_ADVANCE_INST 1                // advance xPC, load wINST
    GET_INST_OPCODE ip                  // ip<- opcode from xINST
    SET_VREG w1, w0                     // fp[A]<- w1
    GOTO_OPCODE ip                      // execute next instruction

%def op_const_class():
%  const(helper="MterpConstClass")

%def op_const_high16():
    /* const/high16 vAA, #+BBBB0000 */
    FETCH   w0, 1                       // r0<- 0000BBBB (zero-extended)
    lsr     w3, wINST, #8               // r3<- AA
    lsl     w0, w0, #16                 // r0<- BBBB0000
    FETCH_ADVANCE_INST 2                // advance rPC, load rINST
    SET_VREG w0, w3                     // vAA<- r0
    GET_INST_OPCODE ip                  // extract opcode from rINST
    GOTO_OPCODE ip                      // jump to next instruction

%def op_const_method_handle():
%  const(helper="MterpConstMethodHandle")

%def op_const_method_type():
%  const(helper="MterpConstMethodType")

%def op_const_string():
%  const(helper="MterpConstString")

%def op_const_string_jumbo():
    /* const/string vAA, String//BBBBBBBB */
    EXPORT_PC
    FETCH w0, 1                         // w0<- bbbb (low
    FETCH w2, 2                         // w2<- BBBB (high
    lsr     w1, wINST, #8               // w1<- AA
    orr     w0, w0, w2, lsl #16         // w1<- BBBBbbbb
    add     x2, xFP, #OFF_FP_SHADOWFRAME
    mov     x3, xSELF
    bl      MterpConstString            // (index, tgt_reg, shadow_frame, self)
    PREFETCH_INST 3                     // advance rPC
    cbnz    w0, MterpPossibleException      // let reference interpreter deal with it.
    ADVANCE 3                           // advance rPC
    GET_INST_OPCODE ip                  // extract opcode from rINST
    GOTO_OPCODE ip                      // jump to next instruction

%def op_const_wide():
    /* const-wide vAA, #+HHHHhhhhBBBBbbbb */
    FETCH w0, 1                         // w0<- bbbb (low)
    FETCH w1, 2                         // w1<- BBBB (low middle)
    FETCH w2, 3                         // w2<- hhhh (high middle)
    FETCH w3, 4                         // w3<- HHHH (high)
    lsr     w4, wINST, #8               // r4<- AA
    FETCH_ADVANCE_INST 5                // advance rPC, load wINST
    GET_INST_OPCODE ip                  // extract opcode from wINST
    orr     w0, w0, w1, lsl #16         // w0<-         BBBBbbbb
    orr     x0, x0, x2, lsl #32         // w0<-     hhhhBBBBbbbb
    orr     x0, x0, x3, lsl #48         // w0<- HHHHhhhhBBBBbbbb
    SET_VREG_WIDE x0, w4
    GOTO_OPCODE ip                      // jump to next instruction

%def op_const_wide_16():
    /* const-wide/16 vAA, #+BBBB */
    FETCH_S x0, 1                       // x0<- ssssssssssssBBBB (sign-extended)
    lsr     w3, wINST, #8               // w3<- AA
    FETCH_ADVANCE_INST 2                // advance rPC, load rINST
    GET_INST_OPCODE ip                  // extract opcode from rINST
    SET_VREG_WIDE x0, w3
    GOTO_OPCODE ip                      // jump to next instruction

%def op_const_wide_32():
    /* const-wide/32 vAA, #+BBBBbbbb */
    FETCH   w0, 1                       // x0<- 000000000000bbbb (low)
    lsr     w3, wINST, #8               // w3<- AA
    FETCH_S x2, 2                       // x2<- ssssssssssssBBBB (high)
    FETCH_ADVANCE_INST 3                // advance rPC, load wINST
    GET_INST_OPCODE ip                  // extract opcode from wINST
    orr     x0, x0, x2, lsl #16         // x0<- ssssssssBBBBbbbb
    SET_VREG_WIDE x0, w3
    GOTO_OPCODE ip                      // jump to next instruction

%def op_const_wide_high16():
    /* const-wide/high16 vAA, #+BBBB000000000000 */
    FETCH w0, 1                         // w0<- 0000BBBB (zero-extended)
    lsr     w1, wINST, #8               // w1<- AA
    FETCH_ADVANCE_INST 2                // advance rPC, load wINST
    lsl     x0, x0, #48
    SET_VREG_WIDE x0, w1
    GET_INST_OPCODE ip                  // extract opcode from wINST
    GOTO_OPCODE ip                      // jump to next instruction

%def op_monitor_enter():
    /*
     * Synchronize on an object.
     */
    /* monitor-enter vAA */
    EXPORT_PC
    lsr      w2, wINST, #8               // w2<- AA
    GET_VREG w0, w2                      // w0<- vAA (object)
    mov      x1, xSELF                   // w1<- self
    bl       artLockObjectFromCode
    cbnz     w0, MterpException
    FETCH_ADVANCE_INST 1
    ldr      w0, [xSELF, #THREAD_USE_MTERP_OFFSET]
    cbz      w0, MterpFallback
    GET_INST_OPCODE ip                   // extract opcode from rINST
    GOTO_OPCODE ip                       // jump to next instruction

%def op_monitor_exit():
    /*
     * Unlock an object.
     *
     * Exceptions that occur when unlocking a monitor need to appear as
     * if they happened at the following instruction.  See the Dalvik
     * instruction spec.
     */
    /* monitor-exit vAA */
    EXPORT_PC
    lsr      w2, wINST, #8              // w2<- AA
    GET_VREG w0, w2                     // w0<- vAA (object)
    mov      x1, xSELF                  // w0<- self
    bl       artUnlockObjectFromCode    // w0<- success for unlock(self, obj)
    cbnz     w0, MterpException
    FETCH_ADVANCE_INST 1                // before throw: advance rPC, load rINST
    ldr      w0, [xSELF, #THREAD_USE_MTERP_OFFSET]
    cbz      w0, MterpFallback
    GET_INST_OPCODE ip                  // extract opcode from rINST
    GOTO_OPCODE ip                      // jump to next instruction

%def op_move(is_object="0"):
    /* for move, move-object, long-to-int */
    /* op vA, vB */
    lsr     w1, wINST, #12              // x1<- B from 15:12
    ubfx    w0, wINST, #8, #4           // x0<- A from 11:8
    FETCH_ADVANCE_INST 1                // advance rPC, load wINST
    GET_VREG w2, w1                     // x2<- fp[B]
    GET_INST_OPCODE ip                  // ip<- opcode from wINST
    .if $is_object
    SET_VREG_OBJECT w2, w0              // fp[A]<- x2
    .else
    SET_VREG w2, w0                     // fp[A]<- x2
    .endif
    GOTO_OPCODE ip                      // execute next instruction

%def op_move_16(is_object="0"):
    /* for: move/16, move-object/16 */
    /* op vAAAA, vBBBB */
    FETCH w1, 2                         // w1<- BBBB
    FETCH w0, 1                         // w0<- AAAA
    FETCH_ADVANCE_INST 3                // advance xPC, load xINST
    GET_VREG w2, w1                     // w2<- fp[BBBB]
    GET_INST_OPCODE ip                  // extract opcode from xINST
    .if $is_object
    SET_VREG_OBJECT w2, w0              // fp[AAAA]<- w2
    .else
    SET_VREG w2, w0                     // fp[AAAA]<- w2
    .endif
    GOTO_OPCODE ip                      // jump to next instruction

%def op_move_exception():
    /* move-exception vAA */
    lsr     w2, wINST, #8               // w2<- AA
    ldr     x3, [xSELF, #THREAD_EXCEPTION_OFFSET]
    mov     x1, #0                      // w1<- 0
    FETCH_ADVANCE_INST 1                // advance rPC, load rINST
    SET_VREG_OBJECT w3, w2              // fp[AA]<- exception obj
    GET_INST_OPCODE ip                  // extract opcode from rINST
    str     x1, [xSELF, #THREAD_EXCEPTION_OFFSET]  // clear exception
    GOTO_OPCODE ip                      // jump to next instruction

%def op_move_from16(is_object="0"):
    /* for: move/from16, move-object/from16 */
    /* op vAA, vBBBB */
    FETCH w1, 1                         // r1<- BBBB
    lsr     w0, wINST, #8               // r0<- AA
    FETCH_ADVANCE_INST 2                // advance rPC, load wINST
    GET_VREG w2, w1                     // r2<- fp[BBBB]
    GET_INST_OPCODE ip                  // extract opcode from wINST
    .if $is_object
    SET_VREG_OBJECT w2, w0              // fp[AA]<- r2
    .else
    SET_VREG w2, w0                     // fp[AA]<- r2
    .endif
    GOTO_OPCODE ip                      // jump to next instruction

%def op_move_object():
%  op_move(is_object="1")

%def op_move_object_16():
%  op_move_16(is_object="1")

%def op_move_object_from16():
%  op_move_from16(is_object="1")

%def op_move_result(is_object="0"):
    /* for: move-result, move-result-object */
    /* op vAA */
    lsr     w2, wINST, #8               // r2<- AA
    FETCH_ADVANCE_INST 1                // advance rPC, load wINST
    ldr     x0, [xFP, #OFF_FP_RESULT_REGISTER]  // get pointer to result JType.
    ldr     w0, [x0]                    // r0 <- result.i.
    GET_INST_OPCODE ip                  // extract opcode from wINST
    .if $is_object
    SET_VREG_OBJECT w0, w2, w1          // fp[AA]<- r0
    .else
    SET_VREG w0, w2                     // fp[AA]<- r0
    .endif
    GOTO_OPCODE ip                      // jump to next instruction

%def op_move_result_object():
%  op_move_result(is_object="1")

%def op_move_result_wide():
    /* for: move-result-wide */
    /* op vAA */
    lsr     w2, wINST, #8               // r2<- AA
    FETCH_ADVANCE_INST 1                // advance rPC, load wINST
    ldr     x0, [xFP, #OFF_FP_RESULT_REGISTER]  // get pointer to result JType.
    ldr     x0, [x0]                    // r0 <- result.i.
    GET_INST_OPCODE ip                  // extract opcode from wINST
    SET_VREG_WIDE x0, x2                // fp[AA]<- r0
    GOTO_OPCODE ip                      // jump to next instruction

%def op_move_wide():
    /* move-wide vA, vB */
    /* NOTE: regs can overlap, e.g. "move v6,v7" or "move v7,v6" */
    lsr     w3, wINST, #12              // w3<- B
    ubfx    w2, wINST, #8, #4           // w2<- A
    GET_VREG_WIDE  x3, w3
    FETCH_ADVANCE_INST 1                // advance rPC, load wINST
    GET_INST_OPCODE ip                  // extract opcode from wINST
    SET_VREG_WIDE  x3, w2
    GOTO_OPCODE ip                      // jump to next instruction

%def op_move_wide_16():
    /* move-wide/16 vAAAA, vBBBB */
    /* NOTE: regs can overlap, e.g. "move v6,v7" or "move v7,v6" */
    FETCH w3, 2                         // w3<- BBBB
    FETCH w2, 1                         // w2<- AAAA
    GET_VREG_WIDE x3, w3
    FETCH_ADVANCE_INST 3                // advance rPC, load rINST
    SET_VREG_WIDE x3, w2
    GET_INST_OPCODE ip                  // extract opcode from rINST
    GOTO_OPCODE ip                      // jump to next instruction

%def op_move_wide_from16():
    /* move-wide/from16 vAA, vBBBB */
    /* NOTE: regs can overlap, e.g. "move v6,v7" or "move v7,v6" */
    FETCH w3, 1                         // w3<- BBBB
    lsr     w2, wINST, #8               // w2<- AA
    GET_VREG_WIDE x3, w3
    FETCH_ADVANCE_INST 2                // advance rPC, load wINST
    GET_INST_OPCODE ip                  // extract opcode from wINST
    SET_VREG_WIDE x3, w2
    GOTO_OPCODE ip                      // jump to next instruction

%def op_nop():
    FETCH_ADVANCE_INST 1                // advance to next instr, load rINST
    GET_INST_OPCODE ip                  // ip<- opcode from rINST
    GOTO_OPCODE ip                      // execute it

%def op_unused_3e():
%  unused()

%def op_unused_3f():
%  unused()

%def op_unused_40():
%  unused()

%def op_unused_41():
%  unused()

%def op_unused_42():
%  unused()

%def op_unused_43():
%  unused()

%def op_unused_73():
%  unused()

%def op_unused_79():
%  unused()

%def op_unused_7a():
%  unused()

%def op_unused_f3():
%  unused()

%def op_unused_f4():
%  unused()

%def op_unused_f5():
%  unused()

%def op_unused_f6():
%  unused()

%def op_unused_f7():
%  unused()

%def op_unused_f8():
%  unused()

%def op_unused_f9():
%  unused()

%def op_unused_fc():
%  unused()

%def op_unused_fd():
%  unused()
