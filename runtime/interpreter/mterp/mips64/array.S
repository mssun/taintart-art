%def op_aget(load="lw", shift="2", data_offset="MIRROR_INT_ARRAY_DATA_OFFSET"):
    /*
     * Array get, 32 bits or less.  vAA <- vBB[vCC].
     *
     * for: aget, aget-boolean, aget-byte, aget-char, aget-short
     *
     * NOTE: assumes data offset for arrays is the same for all non-wide types.
     * If this changes, specialize.
     */
    /* op vAA, vBB, vCC */
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    srl     a4, rINST, 8                # a4 <- AA
    GET_VREG_U a0, a2                   # a0 <- vBB (array object)
    GET_VREG a1, a3                     # a1 <- vCC (requested index)
    beqz    a0, common_errNullObject    # bail if null array object
    lw      a3, MIRROR_ARRAY_LENGTH_OFFSET(a0)  # a3 <- arrayObj->length
    .if $shift
    # [d]lsa does not support shift count of 0.
    dlsa    a0, a1, a0, $shift          # a0 <- arrayObj + index*width
    .else
    daddu   a0, a1, a0                  # a0 <- arrayObj + index*width
    .endif
    bgeu    a1, a3, common_errArrayIndex  # unsigned compare: index >= length, bail
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    $load   a2, $data_offset(a0)        # a2 <- vBB[vCC]
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a2, a4                     # vAA <- a2
    GOTO_OPCODE v0                      # jump to next instruction

%def op_aget_boolean():
%  op_aget(load="lbu", shift="0", data_offset="MIRROR_BOOLEAN_ARRAY_DATA_OFFSET")

%def op_aget_byte():
%  op_aget(load="lb", shift="0", data_offset="MIRROR_BYTE_ARRAY_DATA_OFFSET")

%def op_aget_char():
%  op_aget(load="lhu", shift="1", data_offset="MIRROR_CHAR_ARRAY_DATA_OFFSET")

%def op_aget_object():
    /*
     * Array object get.  vAA <- vBB[vCC].
     *
     * for: aget-object
     */
    /* op vAA, vBB, vCC */
    .extern artAGetObjectFromMterp
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    EXPORT_PC
    GET_VREG_U a0, a2                   # a0 <- vBB (array object)
    GET_VREG a1, a3                     # a1 <- vCC (requested index)
    jal     artAGetObjectFromMterp      # (array, index)
    ld      a1, THREAD_EXCEPTION_OFFSET(rSELF)
    srl     a4, rINST, 8                # a4 <- AA
    PREFETCH_INST 2
    bnez    a1, MterpException
    SET_VREG_OBJECT v0, a4              # vAA <- v0
    ADVANCE 2
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction

%def op_aget_short():
%  op_aget(load="lh", shift="1", data_offset="MIRROR_SHORT_ARRAY_DATA_OFFSET")

%def op_aget_wide():
    /*
     * Array get, 64 bits.  vAA <- vBB[vCC].
     *
     */
    /* aget-wide vAA, vBB, vCC */
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    srl     a4, rINST, 8                # a4 <- AA
    GET_VREG_U a0, a2                   # a0 <- vBB (array object)
    GET_VREG a1, a3                     # a1 <- vCC (requested index)
    beqz    a0, common_errNullObject    # bail if null array object
    lw      a3, MIRROR_ARRAY_LENGTH_OFFSET(a0)  # a3 <- arrayObj->length
    dlsa    a0, a1, a0, 3               # a0 <- arrayObj + index*width
    bgeu    a1, a3, common_errArrayIndex  # unsigned compare: index >= length, bail
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    lw      a2, MIRROR_WIDE_ARRAY_DATA_OFFSET(a0)
    lw      a3, (MIRROR_WIDE_ARRAY_DATA_OFFSET+4)(a0)
    dinsu   a2, a3, 32, 32              # a2 <- vBB[vCC]
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_WIDE a2, a4                # vAA <- a2
    GOTO_OPCODE v0                      # jump to next instruction

%def op_aput(store="sw", shift="2", data_offset="MIRROR_INT_ARRAY_DATA_OFFSET"):
    /*
     * Array put, 32 bits or less.  vBB[vCC] <- vAA.
     *
     * for: aput, aput-boolean, aput-byte, aput-char, aput-short
     *
     * NOTE: this assumes data offset for arrays is the same for all non-wide types.
     * If this changes, specialize.
     */
    /* op vAA, vBB, vCC */
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    srl     a4, rINST, 8                # a4 <- AA
    GET_VREG_U a0, a2                   # a0 <- vBB (array object)
    GET_VREG a1, a3                     # a1 <- vCC (requested index)
    beqz    a0, common_errNullObject    # bail if null array object
    lw      a3, MIRROR_ARRAY_LENGTH_OFFSET(a0)  # a3 <- arrayObj->length
    .if $shift
    # [d]lsa does not support shift count of 0.
    dlsa    a0, a1, a0, $shift          # a0 <- arrayObj + index*width
    .else
    daddu   a0, a1, a0                  # a0 <- arrayObj + index*width
    .endif
    bgeu    a1, a3, common_errArrayIndex  # unsigned compare: index >= length, bail
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_VREG a2, a4                     # a2 <- vAA
    GET_INST_OPCODE v0                  # extract opcode from rINST
    $store  a2, $data_offset(a0)        # vBB[vCC] <- a2
    GOTO_OPCODE v0                      # jump to next instruction

%def op_aput_boolean():
%  op_aput(store="sb", shift="0", data_offset="MIRROR_BOOLEAN_ARRAY_DATA_OFFSET")

%def op_aput_byte():
%  op_aput(store="sb", shift="0", data_offset="MIRROR_BYTE_ARRAY_DATA_OFFSET")

%def op_aput_char():
%  op_aput(store="sh", shift="1", data_offset="MIRROR_CHAR_ARRAY_DATA_OFFSET")

%def op_aput_object():
    /*
     * Store an object into an array.  vBB[vCC] <- vAA.
     */
    /* op vAA, vBB, vCC */
    .extern MterpAputObject
    EXPORT_PC
    daddu   a0, rFP, OFF_FP_SHADOWFRAME
    move    a1, rPC
    move    a2, rINST
    jal     MterpAputObject
    beqzc   v0, MterpPossibleException
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction

%def op_aput_short():
%  op_aput(store="sh", shift="1", data_offset="MIRROR_SHORT_ARRAY_DATA_OFFSET")

%def op_aput_wide():
    /*
     * Array put, 64 bits.  vBB[vCC] <- vAA.
     *
     */
    /* aput-wide vAA, vBB, vCC */
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    srl     a4, rINST, 8                # a4 <- AA
    GET_VREG_U a0, a2                   # a0 <- vBB (array object)
    GET_VREG a1, a3                     # a1 <- vCC (requested index)
    beqz    a0, common_errNullObject    # bail if null array object
    lw      a3, MIRROR_ARRAY_LENGTH_OFFSET(a0)  # a3 <- arrayObj->length
    dlsa    a0, a1, a0, 3               # a0 <- arrayObj + index*width
    bgeu    a1, a3, common_errArrayIndex  # unsigned compare: index >= length, bail
    GET_VREG_WIDE a2, a4                # a2 <- vAA
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    sw      a2, MIRROR_WIDE_ARRAY_DATA_OFFSET(a0)
    dsrl32  a2, a2, 0
    sw      a2, (MIRROR_WIDE_ARRAY_DATA_OFFSET+4)(a0)  # vBB[vCC] <- a2
    GOTO_OPCODE v0                      # jump to next instruction

%def op_array_length():
    /*
     * Return the length of an array.
     */
    srl     a1, rINST, 12               # a1 <- B
    GET_VREG_U a0, a1                   # a0 <- vB (object ref)
    ext     a2, rINST, 8, 4             # a2 <- A
    beqz    a0, common_errNullObject    # yup, fail
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
    lw      a3, MIRROR_ARRAY_LENGTH_OFFSET(a0)  # a3 <- array length
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a3, a2                     # vB <- length
    GOTO_OPCODE v0                      # jump to next instruction

%def op_fill_array_data():
    /* fill-array-data vAA, +BBBBBBBB */
    .extern MterpFillArrayData
    EXPORT_PC
    lh      a1, 2(rPC)                  # a1 <- bbbb (lo)
    lh      a0, 4(rPC)                  # a0 <- BBBB (hi)
    srl     a3, rINST, 8                # a3 <- AA
    ins     a1, a0, 16, 16              # a1 <- BBBBbbbb
    GET_VREG_U a0, a3                   # a0 <- vAA (array object)
    dlsa    a1, a1, rPC, 1              # a1 <- PC + BBBBbbbb*2 (array data off.)
    jal     MterpFillArrayData          # (obj, payload)
    beqzc   v0, MterpPossibleException  # exception?
    FETCH_ADVANCE_INST 3                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction

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
    daddu   a0, rFP, OFF_FP_SHADOWFRAME
    move    a1, rPC
    move    a2, rSELF
    jal     $helper
    beqzc   v0, MterpPossibleException
    FETCH_ADVANCE_INST 3                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction

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
    .extern MterpNewArray
    EXPORT_PC
    daddu   a0, rFP, OFF_FP_SHADOWFRAME
    move    a1, rPC
    move    a2, rINST
    move    a3, rSELF
    jal     MterpNewArray
    beqzc   v0, MterpPossibleException
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction
