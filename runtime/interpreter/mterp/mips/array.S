%def op_aget(load="lw", shift="2", data_offset="MIRROR_INT_ARRAY_DATA_OFFSET"):
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
    FETCH_B(a2, 1, 0)                      #  a2 <- BB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    FETCH_B(a3, 1, 1)                      #  a3 <- CC
    GET_VREG(a0, a2)                       #  a0 <- vBB (array object)
    GET_VREG(a1, a3)                       #  a1 <- vCC (requested index)
    # null array object?
    beqz      a0, common_errNullObject     #  yes, bail
    LOAD_base_offMirrorArray_length(a3, a0) #  a3 <- arrayObj->length
    EASN(a0, a0, a1, $shift)               #  a0 <- arrayObj + index*width
    # a1 >= a3; compare unsigned index
    bgeu      a1, a3, common_errArrayIndex #  index >= length, bail
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    $load a2, $data_offset(a0)             #  a2 <- vBB[vCC]
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a2, rOBJ, t0)            #  vAA <- a2

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
    FETCH_B(a2, 1, 0)                      #  a2 <- BB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    FETCH_B(a3, 1, 1)                      #  a3 <- CC
    EXPORT_PC()
    GET_VREG(a0, a2)                       #  a0 <- vBB (array object)
    GET_VREG(a1, a3)                       #  a1 <- vCC (requested index)
    JAL(artAGetObjectFromMterp)            #  v0 <- GetObj(array, index)
    lw   a1, THREAD_EXCEPTION_OFFSET(rSELF)
    PREFETCH_INST(2)                       #  load rINST
    bnez a1, MterpException
    ADVANCE(2)                             #  advance rPC
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_OBJECT_GOTO(v0, rOBJ, t0)     #  vAA <- v0

%def op_aget_short():
%  op_aget(load="lh", shift="1", data_offset="MIRROR_SHORT_ARRAY_DATA_OFFSET")

%def op_aget_wide():
    /*
     * Array get, 64 bits.  vAA <- vBB[vCC].
     *
     * Arrays of long/double are 64-bit aligned.
     */
    /* aget-wide vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a0, 255                  #  a2 <- BB
    srl       a3, a0, 8                    #  a3 <- CC
    GET_VREG(a0, a2)                       #  a0 <- vBB (array object)
    GET_VREG(a1, a3)                       #  a1 <- vCC (requested index)
    # null array object?
    beqz      a0, common_errNullObject     #  yes, bail
    LOAD_base_offMirrorArray_length(a3, a0) #  a3 <- arrayObj->length
    EAS3(a0, a0, a1)                       #  a0 <- arrayObj + index*width
    bgeu      a1, a3, common_errArrayIndex #  index >= length, bail

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    LOAD64_off(a2, a3, a0, MIRROR_WIDE_ARRAY_DATA_OFFSET)
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_GOTO(a2, a3, rOBJ, t0)      #  vAA/vAA+1 <- a2/a3

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
    FETCH_B(a2, 1, 0)                      #  a2 <- BB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    FETCH_B(a3, 1, 1)                      #  a3 <- CC
    GET_VREG(a0, a2)                       #  a0 <- vBB (array object)
    GET_VREG(a1, a3)                       #  a1 <- vCC (requested index)
    # null array object?
    beqz      a0, common_errNullObject     #  yes, bail
    LOAD_base_offMirrorArray_length(a3, a0) #  a3 <- arrayObj->length
    EASN(a0, a0, a1, $shift)               #  a0 <- arrayObj + index*width
    bgeu      a1, a3, common_errArrayIndex #  index >= length, bail
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_VREG(a2, rOBJ)                     #  a2 <- vAA
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GET_OPCODE_TARGET(t0)
    $store a2, $data_offset(a0)            #  vBB[vCC] <- a2
    JR(t0)                                 #  jump to next instruction

%def op_aput_boolean():
%  op_aput(store="sb", shift="0", data_offset="MIRROR_BOOLEAN_ARRAY_DATA_OFFSET")

%def op_aput_byte():
%  op_aput(store="sb", shift="0", data_offset="MIRROR_BYTE_ARRAY_DATA_OFFSET")

%def op_aput_char():
%  op_aput(store="sh", shift="1", data_offset="MIRROR_CHAR_ARRAY_DATA_OFFSET")

%def op_aput_object():
    /*
     * Store an object into an array.  vBB[vCC] <- vAA.
     *
     */
    /* op vAA, vBB, vCC */
    EXPORT_PC()
    addu   a0, rFP, OFF_FP_SHADOWFRAME
    move   a1, rPC
    move   a2, rINST
    JAL(MterpAputObject)
    beqz   v0, MterpPossibleException
    FETCH_ADVANCE_INST(2)               # advance rPC, load rINST
    GET_INST_OPCODE(t0)                 # extract opcode from rINST
    GOTO_OPCODE(t0)                     # jump to next instruction

%def op_aput_short():
%  op_aput(store="sh", shift="1", data_offset="MIRROR_SHORT_ARRAY_DATA_OFFSET")

%def op_aput_wide():
    /*
     * Array put, 64 bits.  vBB[vCC] <- vAA.
     */
    /* aput-wide vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(t0)                            #  t0 <- AA
    and       a2, a0, 255                  #  a2 <- BB
    srl       a3, a0, 8                    #  a3 <- CC
    GET_VREG(a0, a2)                       #  a0 <- vBB (array object)
    GET_VREG(a1, a3)                       #  a1 <- vCC (requested index)
    # null array object?
    beqz      a0, common_errNullObject     #  yes, bail
    LOAD_base_offMirrorArray_length(a3, a0) #  a3 <- arrayObj->length
    EAS3(a0, a0, a1)                       #  a0 <- arrayObj + index*width
    EAS2(rOBJ, rFP, t0)                    #  rOBJ <- &fp[AA]
    # compare unsigned index, length
    bgeu      a1, a3, common_errArrayIndex #  index >= length, bail

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    LOAD64(a2, a3, rOBJ)                   #  a2/a3 <- vAA/vAA+1
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GET_OPCODE_TARGET(t0)
    STORE64_off(a2, a3, a0, MIRROR_WIDE_ARRAY_DATA_OFFSET) #  a2/a3 <- vBB[vCC]
    JR(t0)                                 #  jump to next instruction

%def op_array_length():
    /*
     * Return the length of an array.
     */
    /* array-length vA, vB */
    GET_OPB(a1)                            #  a1 <- B
    GET_OPA4(a2)                           #  a2 <- A+
    GET_VREG(a0, a1)                       #  a0 <- vB (object ref)
    # is object null?
    beqz      a0, common_errNullObject     #  yup, fail
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    LOAD_base_offMirrorArray_length(a3, a0) #  a3 <- array length
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a3, a2, t0)              #  vA <- length

%def op_fill_array_data():
    /* fill-array-data vAA, +BBBBBBBB */
    EXPORT_PC()
    FETCH(a1, 1)                           #  a1 <- bbbb (lo)
    FETCH(a0, 2)                           #  a0 <- BBBB (hi)
    GET_OPA(a3)                            #  a3 <- AA
    INSERT_HIGH_HALF(a1, a0)               #  a1 <- BBBBbbbb
    GET_VREG(a0, a3)                       #  a0 <- vAA (array object)
    EAS1(a1, rPC, a1)                      #  a1 <- PC + BBBBbbbb*2 (array data off.)
    JAL(MterpFillArrayData)                #  v0 <- Mterp(obj, payload)
    beqz      v0,  MterpPossibleException  #  has exception
    FETCH_ADVANCE_INST(3)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction

%def op_filled_new_array(helper="MterpFilledNewArray"):
    /*
     * Create a new array with elements filled from registers.
     *
     * for: filled-new-array, filled-new-array/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, type@BBBB */
    .extern $helper
    EXPORT_PC()
    addu   a0, rFP, OFF_FP_SHADOWFRAME     # a0 <- shadow frame
    move   a1, rPC
    move   a2, rSELF
    JAL($helper)                           #  v0 <- helper(shadow_frame, pc, self)
    beqz      v0,  MterpPossibleException  #  has exception
    FETCH_ADVANCE_INST(3)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction

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
    EXPORT_PC()
    addu   a0, rFP, OFF_FP_SHADOWFRAME
    move   a1, rPC
    move   a2, rINST
    move   a3, rSELF
    JAL(MterpNewArray)
    beqz   v0, MterpPossibleException
    FETCH_ADVANCE_INST(2)               # advance rPC, load rINST
    GET_INST_OPCODE(t0)                 # extract opcode from rINST
    GOTO_OPCODE(t0)                     # jump to next instruction
