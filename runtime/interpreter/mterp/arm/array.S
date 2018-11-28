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
    FETCH_B r2, 1, 0                    @ r2<- BB
    mov     r9, rINST, lsr #8           @ r9<- AA
    FETCH_B r3, 1, 1                    @ r3<- CC
    GET_VREG r0, r2                     @ r0<- vBB (array object)
    GET_VREG r1, r3                     @ r1<- vCC (requested index)
    cmp     r0, #0                      @ null array object?
    beq     common_errNullObject        @ yes, bail
    ldr     r3, [r0, #MIRROR_ARRAY_LENGTH_OFFSET]    @ r3<- arrayObj->length
    add     r0, r0, r1, lsl #$shift     @ r0<- arrayObj + index*width
    cmp     r1, r3                      @ compare unsigned index, length
    bcs     common_errArrayIndex        @ index >= length, bail
    FETCH_ADVANCE_INST 2                @ advance rPC, load rINST
    $load   r2, [r0, #$data_offset]     @ r2<- vBB[vCC]
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    SET_VREG r2, r9                     @ vAA<- r2
    GOTO_OPCODE ip                      @ jump to next instruction

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
    FETCH_B r2, 1, 0                    @ r2<- BB
    mov     r9, rINST, lsr #8           @ r9<- AA
    FETCH_B r3, 1, 1                    @ r3<- CC
    EXPORT_PC
    GET_VREG r0, r2                     @ r0<- vBB (array object)
    GET_VREG r1, r3                     @ r1<- vCC (requested index)
    bl       artAGetObjectFromMterp     @ (array, index)
    ldr      r1, [rSELF, #THREAD_EXCEPTION_OFFSET]
    PREFETCH_INST 2
    cmp      r1, #0
    bne      MterpException
    SET_VREG_OBJECT r0, r9
    ADVANCE 2
    GET_INST_OPCODE ip
    GOTO_OPCODE ip                      @ jump to next instruction

%def op_aget_short():
%  op_aget(load="ldrsh", shift="1", data_offset="MIRROR_SHORT_ARRAY_DATA_OFFSET")

%def op_aget_wide():
    /*
     * Array get, 64 bits.  vAA <- vBB[vCC].
     *
     * Arrays of long/double are 64-bit aligned, so it's okay to use LDRD.
     */
    /* aget-wide vAA, vBB, vCC */
    FETCH r0, 1                         @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    and     r2, r0, #255                @ r2<- BB
    mov     r3, r0, lsr #8              @ r3<- CC
    GET_VREG r0, r2                     @ r0<- vBB (array object)
    GET_VREG r1, r3                     @ r1<- vCC (requested index)
    cmp     r0, #0                      @ null array object?
    beq     common_errNullObject        @ yes, bail
    ldr     r3, [r0, #MIRROR_ARRAY_LENGTH_OFFSET]    @ r3<- arrayObj->length
    add     r0, r0, r1, lsl #3          @ r0<- arrayObj + index*width
    cmp     r1, r3                      @ compare unsigned index, length
    bcs     common_errArrayIndex        @ index >= length, bail
    FETCH_ADVANCE_INST 2                @ advance rPC, load rINST
    CLEAR_SHADOW_PAIR r9, lr, ip        @ Zero out the shadow regs
    ldrd    r2, [r0, #MIRROR_WIDE_ARRAY_DATA_OFFSET]  @ r2/r3<- vBB[vCC]
    VREG_INDEX_TO_ADDR r9, r9           @ r9<- &fp[AA]
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    SET_VREG_WIDE_BY_ADDR r2, r3, r9    @ vAA/vAA+1<- r2/r3
    GOTO_OPCODE ip                      @ jump to next instruction

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
    FETCH_B r2, 1, 0                    @ r2<- BB
    mov     r9, rINST, lsr #8           @ r9<- AA
    FETCH_B r3, 1, 1                    @ r3<- CC
    GET_VREG r0, r2                     @ r0<- vBB (array object)
    GET_VREG r1, r3                     @ r1<- vCC (requested index)
    cmp     r0, #0                      @ null array object?
    beq     common_errNullObject        @ yes, bail
    ldr     r3, [r0, #MIRROR_ARRAY_LENGTH_OFFSET]     @ r3<- arrayObj->length
    add     r0, r0, r1, lsl #$shift     @ r0<- arrayObj + index*width
    cmp     r1, r3                      @ compare unsigned index, length
    bcs     common_errArrayIndex        @ index >= length, bail
    FETCH_ADVANCE_INST 2                @ advance rPC, load rINST
    GET_VREG r2, r9                     @ r2<- vAA
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    $store  r2, [r0, #$data_offset]     @ vBB[vCC]<- r2
    GOTO_OPCODE ip                      @ jump to next instruction

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
    add     r0, rFP, #OFF_FP_SHADOWFRAME
    mov     r1, rPC
    mov     r2, rINST
    bl      MterpAputObject
    cmp     r0, #0
    beq     MterpPossibleException
    FETCH_ADVANCE_INST 2                @ advance rPC, load rINST
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    GOTO_OPCODE ip                      @ jump to next instruction

%def op_aput_short():
%  op_aput(store="strh", shift="1", data_offset="MIRROR_SHORT_ARRAY_DATA_OFFSET")

%def op_aput_wide():
    /*
     * Array put, 64 bits.  vBB[vCC] <- vAA.
     *
     * Arrays of long/double are 64-bit aligned, so it's okay to use STRD.
     */
    /* aput-wide vAA, vBB, vCC */
    FETCH r0, 1                         @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    and     r2, r0, #255                @ r2<- BB
    mov     r3, r0, lsr #8              @ r3<- CC
    GET_VREG r0, r2                     @ r0<- vBB (array object)
    GET_VREG r1, r3                     @ r1<- vCC (requested index)
    cmp     r0, #0                      @ null array object?
    beq     common_errNullObject        @ yes, bail
    ldr     r3, [r0, #MIRROR_ARRAY_LENGTH_OFFSET]    @ r3<- arrayObj->length
    add     r0, r0, r1, lsl #3          @ r0<- arrayObj + index*width
    cmp     r1, r3                      @ compare unsigned index, length
    VREG_INDEX_TO_ADDR r9, r9           @ r9<- &fp[AA]
    bcs     common_errArrayIndex        @ index >= length, bail
    FETCH_ADVANCE_INST 2                @ advance rPC, load rINST
    GET_VREG_WIDE_BY_ADDR r2, r3, r9    @ r2/r3<- vAA/vAA+1
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    strd    r2, [r0, #MIRROR_WIDE_ARRAY_DATA_OFFSET]  @ r2/r3<- vBB[vCC]
    GOTO_OPCODE ip                      @ jump to next instruction

%def op_array_length():
    /*
     * Return the length of an array.
     */
    mov     r1, rINST, lsr #12          @ r1<- B
    ubfx    r2, rINST, #8, #4           @ r2<- A
    GET_VREG r0, r1                     @ r0<- vB (object ref)
    cmp     r0, #0                      @ is object null?
    beq     common_errNullObject        @ yup, fail
    FETCH_ADVANCE_INST 1                @ advance rPC, load rINST
    ldr     r3, [r0, #MIRROR_ARRAY_LENGTH_OFFSET]    @ r3<- array length
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    SET_VREG r3, r2                     @ vB<- length
    GOTO_OPCODE ip                      @ jump to next instruction

%def op_fill_array_data():
    /* fill-array-data vAA, +BBBBBBBB */
    EXPORT_PC
    FETCH r0, 1                         @ r0<- bbbb (lo)
    FETCH r1, 2                         @ r1<- BBBB (hi)
    mov     r3, rINST, lsr #8           @ r3<- AA
    orr     r1, r0, r1, lsl #16         @ r1<- BBBBbbbb
    GET_VREG r0, r3                     @ r0<- vAA (array object)
    add     r1, rPC, r1, lsl #1         @ r1<- PC + BBBBbbbb*2 (array data off.)
    bl      MterpFillArrayData          @ (obj, payload)
    cmp     r0, #0                      @ 0 means an exception is thrown
    beq     MterpPossibleException      @ exception?
    FETCH_ADVANCE_INST 3                @ advance rPC, load rINST
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    GOTO_OPCODE ip                      @ jump to next instruction

%def op_filled_new_array(helper="MterpFilledNewArray"):
    /*
     * Create a new array with elements filled from registers.
     *
     * for: filled-new-array, filled-new-array/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, type@BBBB */
    .extern $helper
    EXPORT_PC
    add     r0, rFP, #OFF_FP_SHADOWFRAME
    mov     r1, rPC
    mov     r2, rSELF
    bl      $helper
    cmp     r0, #0
    beq     MterpPossibleException
    FETCH_ADVANCE_INST 3                @ advance rPC, load rINST
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    GOTO_OPCODE ip                      @ jump to next instruction

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
    /* new-array vA, vB, class@CCCC */
    EXPORT_PC
    add     r0, rFP, #OFF_FP_SHADOWFRAME
    mov     r1, rPC
    mov     r2, rINST
    mov     r3, rSELF
    bl      MterpNewArray
    cmp     r0, #0
    beq     MterpPossibleException
    FETCH_ADVANCE_INST 2                @ advance rPC, load rINST
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    GOTO_OPCODE ip                      @ jump to next instruction
