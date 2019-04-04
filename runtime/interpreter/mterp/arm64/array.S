%def op_aget(load="ldr", shift="2", data_offset="MIRROR_INT_ARRAY_DATA_OFFSET"):
    /*
     * Array get, 32 bits or less.  vAA <- vBB[vCC].
     *
     * Note: using the usual FETCH/and/shift stuff, this fits in exactly 17
     * instructions.  We use a pair of FETCH_Bs instead.
     *
     * for: aget, aget-boolean, aget-byte, aget-char, aget-short
     *
     * NOTE: assumes data offset for arrays is the same for all non-wide types.
     * If this changes, specialize.
     */
    /* op vAA, vBB, vCC */
    FETCH_B w2, 1, 0                    // w2<- BB
    lsr     w9, wINST, #8               // w9<- AA
    FETCH_B w3, 1, 1                    // w3<- CC
    GET_VREG w0, w2                     // w0<- vBB (array object)
    GET_VREG w1, w3                     // w1<- vCC (requested index)
    cbz     x0, common_errNullObject    // bail if null array object.
    ldr     w3, [x0, #MIRROR_ARRAY_LENGTH_OFFSET]    // w3<- arrayObj->length
    add     x0, x0, w1, uxtw #$shift    // w0<- arrayObj + index*width
    cmp     w1, w3                      // compare unsigned index, length
    bcs     common_errArrayIndex        // index >= length, bail
    FETCH_ADVANCE_INST 2                // advance rPC, load rINST
    $load   w2, [x0, #$data_offset]     // w2<- vBB[vCC]
    GET_INST_OPCODE ip                  // extract opcode from rINST
    SET_VREG w2, w9                     // vAA<- w2
    GOTO_OPCODE ip                      // jump to next instruction

%def op_aget_boolean():
%  op_aget(load="ldrb", shift="0", data_offset="MIRROR_BOOLEAN_ARRAY_DATA_OFFSET")

%def op_aget_byte():
%  op_aget(load="ldrsb", shift="0", data_offset="MIRROR_BYTE_ARRAY_DATA_OFFSET")

%def op_aget_char():
%  op_aget(load="ldrh", shift="1", data_offset="MIRROR_CHAR_ARRAY_DATA_OFFSET")

%def op_aget_object():
    /*
     * Array object get.  vAA <- vBB[vCC].
     *
     * for: aget-object
     */
    /* op vAA, vBB, vCC */
    FETCH_B w2, 1, 0                    // w2<- BB
    FETCH_B w3, 1, 1                    // w3<- CC
    EXPORT_PC
    GET_VREG w0, w2                     // w0<- vBB (array object)
    GET_VREG w1, w3                     // w1<- vCC (requested index)
    bl       artAGetObjectFromMterp     // (array, index)
    ldr      x1, [xSELF, #THREAD_EXCEPTION_OFFSET]
    lsr      w2, wINST, #8               // w9<- AA
    PREFETCH_INST 2
    cbnz     w1, MterpException
    SET_VREG_OBJECT w0, w2
    ADVANCE 2
    GET_INST_OPCODE ip
    GOTO_OPCODE ip                      // jump to next instruction

%def op_aget_short():
%  op_aget(load="ldrsh", shift="1", data_offset="MIRROR_SHORT_ARRAY_DATA_OFFSET")

%def op_aget_wide():
    /*
     * Array get, 64 bits.  vAA <- vBB[vCC].
     *
     */
    /* aget-wide vAA, vBB, vCC */
    FETCH w0, 1                         // w0<- CCBB
    lsr     w4, wINST, #8               // w4<- AA
    and     w2, w0, #255                // w2<- BB
    lsr     w3, w0, #8                  // w3<- CC
    GET_VREG w0, w2                     // w0<- vBB (array object)
    GET_VREG w1, w3                     // w1<- vCC (requested index)
    cbz     w0, common_errNullObject        // yes, bail
    ldr     w3, [x0, #MIRROR_ARRAY_LENGTH_OFFSET]    // w3<- arrayObj->length
    add     x0, x0, w1, uxtw #3         // w0<- arrayObj + index*width
    cmp     w1, w3                      // compare unsigned index, length
    bcs     common_errArrayIndex        // index >= length, bail
    FETCH_ADVANCE_INST 2                // advance rPC, load wINST
    ldr     x2, [x0, #MIRROR_WIDE_ARRAY_DATA_OFFSET]  // x2<- vBB[vCC]
    GET_INST_OPCODE ip                  // extract opcode from wINST
    SET_VREG_WIDE x2, w4
    GOTO_OPCODE ip                      // jump to next instruction

%def op_aput(store="str", shift="2", data_offset="MIRROR_INT_ARRAY_DATA_OFFSET"):
    /*
     * Array put, 32 bits or less.  vBB[vCC] <- vAA.
     *
     * Note: using the usual FETCH/and/shift stuff, this fits in exactly 17
     * instructions.  We use a pair of FETCH_Bs instead.
     *
     * for: aput, aput-boolean, aput-byte, aput-char, aput-short
     *
     * NOTE: this assumes data offset for arrays is the same for all non-wide types.
     * If this changes, specialize.
     */
    /* op vAA, vBB, vCC */
    FETCH_B w2, 1, 0                    // w2<- BB
    lsr     w9, wINST, #8               // w9<- AA
    FETCH_B w3, 1, 1                    // w3<- CC
    GET_VREG w0, w2                     // w0<- vBB (array object)
    GET_VREG w1, w3                     // w1<- vCC (requested index)
    cbz     w0, common_errNullObject    // bail if null
    ldr     w3, [x0, #MIRROR_ARRAY_LENGTH_OFFSET]     // w3<- arrayObj->length
    add     x0, x0, w1, uxtw #$shift    // w0<- arrayObj + index*width
    cmp     w1, w3                      // compare unsigned index, length
    bcs     common_errArrayIndex        // index >= length, bail
    FETCH_ADVANCE_INST 2                // advance rPC, load rINST
    GET_VREG w2, w9                     // w2<- vAA
    GET_INST_OPCODE ip                  // extract opcode from rINST
    $store  w2, [x0, #$data_offset]     // vBB[vCC]<- w2
    GOTO_OPCODE ip                      // jump to next instruction

%def op_aput_boolean():
%  op_aput(store="strb", shift="0", data_offset="MIRROR_BOOLEAN_ARRAY_DATA_OFFSET")

%def op_aput_byte():
%  op_aput(store="strb", shift="0", data_offset="MIRROR_BYTE_ARRAY_DATA_OFFSET")

%def op_aput_char():
%  op_aput(store="strh", shift="1", data_offset="MIRROR_CHAR_ARRAY_DATA_OFFSET")

%def op_aput_object():
    /*
     * Store an object into an array.  vBB[vCC] <- vAA.
     */
    /* op vAA, vBB, vCC */
    EXPORT_PC
    add     x0, xFP, #OFF_FP_SHADOWFRAME
    mov     x1, xPC
    mov     w2, wINST
    bl      MterpAputObject
    cbz     w0, MterpPossibleException
    FETCH_ADVANCE_INST 2                // advance rPC, load rINST
    GET_INST_OPCODE ip                  // extract opcode from rINST
    GOTO_OPCODE ip                      // jump to next instruction

%def op_aput_short():
%  op_aput(store="strh", shift="1", data_offset="MIRROR_SHORT_ARRAY_DATA_OFFSET")

%def op_aput_wide():
    /*
     * Array put, 64 bits.  vBB[vCC] <- vAA.
     *
     */
    /* aput-wide vAA, vBB, vCC */
    FETCH w0, 1                         // w0<- CCBB
    lsr     w4, wINST, #8               // w4<- AA
    and     w2, w0, #255                // w2<- BB
    lsr     w3, w0, #8                  // w3<- CC
    GET_VREG w0, w2                     // w0<- vBB (array object)
    GET_VREG w1, w3                     // w1<- vCC (requested index)
    cbz     w0, common_errNullObject    // bail if null
    ldr     w3, [x0, #MIRROR_ARRAY_LENGTH_OFFSET]    // w3<- arrayObj->length
    add     x0, x0, w1, uxtw #3         // w0<- arrayObj + index*width
    cmp     w1, w3                      // compare unsigned index, length
    bcs     common_errArrayIndex        // index >= length, bail
    GET_VREG_WIDE x1, w4
    FETCH_ADVANCE_INST 2                // advance rPC, load wINST
    GET_INST_OPCODE ip                  // extract opcode from wINST
    str     x1, [x0, #MIRROR_WIDE_ARRAY_DATA_OFFSET]
    GOTO_OPCODE ip                      // jump to next instruction

%def op_array_length():
    /*
     * Return the length of an array.
     */
    lsr     w1, wINST, #12              // w1<- B
    ubfx    w2, wINST, #8, #4           // w2<- A
    GET_VREG w0, w1                     // w0<- vB (object ref)
    cbz     w0, common_errNullObject    // yup, fail
    FETCH_ADVANCE_INST 1                // advance rPC, load rINST
    ldr     w3, [x0, #MIRROR_ARRAY_LENGTH_OFFSET]    // w3<- array length
    GET_INST_OPCODE ip                  // extract opcode from rINST
    SET_VREG w3, w2                     // vB<- length
    GOTO_OPCODE ip                      // jump to next instruction

%def op_fill_array_data():
    /* fill-array-data vAA, +BBBBBBBB */
    EXPORT_PC
    FETCH   w0, 1                       // x0<- 000000000000bbbb (lo)
    FETCH_S x1, 2                       // x1<- ssssssssssssBBBB (hi)
    lsr     w3, wINST, #8               // w3<- AA
    orr     x1, x0, x1, lsl #16         // x1<- ssssssssBBBBbbbb
    GET_VREG w0, w3                     // w0<- vAA (array object)
    add     x1, xPC, x1, lsl #1         // x1<- PC + ssssssssBBBBbbbb*2 (array data off.)
    bl      MterpFillArrayData          // (obj, payload)
    cbz     w0, MterpPossibleException      // exception?
    FETCH_ADVANCE_INST 3                // advance rPC, load rINST
    GET_INST_OPCODE ip                  // extract opcode from rINST
    GOTO_OPCODE ip                      // jump to next instruction

%def op_filled_new_array(helper="MterpFilledNewArray"):
    /*
     * Create a new array with elements filled from registers.
     *
     * for: filled-new-array, filled-new-array/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class//CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, type//BBBB */
    .extern $helper
    EXPORT_PC
    add     x0, xFP, #OFF_FP_SHADOWFRAME
    mov     x1, xPC
    mov     x2, xSELF
    bl      $helper
    cbz     w0, MterpPossibleException
    FETCH_ADVANCE_INST 3                // advance rPC, load rINST
    GET_INST_OPCODE ip                  // extract opcode from rINST
    GOTO_OPCODE ip                      // jump to next instruction

%def op_filled_new_array_range():
%  op_filled_new_array(helper="MterpFilledNewArrayRange")

%def op_new_array():
    /*
     * Allocate an array of objects, specified with the array class
     * and a count.
     *
     * The verifier guarantees that this is an array class, so we don't
     * check for it here.
     */
    /* new-array vA, vB, class//CCCC */
    EXPORT_PC
    add     x0, xFP, #OFF_FP_SHADOWFRAME
    mov     x1, xPC
    mov     w2, wINST
    mov     x3, xSELF
    bl      MterpNewArray
    cbz     w0, MterpPossibleException
    FETCH_ADVANCE_INST 2                // advance rPC, load rINST
    GET_INST_OPCODE ip                  // extract opcode from rINST
    GOTO_OPCODE ip                      // jump to next instruction
