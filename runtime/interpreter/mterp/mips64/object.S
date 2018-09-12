%def field(helper=""):
TODO

%def op_check_cast():
    /*
     * Check to see if a cast from one class to another is allowed.
     */
    /* check-cast vAA, class//BBBB */
    .extern MterpCheckCast
    EXPORT_PC
    lhu     a0, 2(rPC)                  # a0 <- BBBB
    srl     a1, rINST, 8                # a1 <- AA
    dlsa    a1, a1, rFP, 2              # a1 <- &object
    ld      a2, OFF_FP_METHOD(rFP)      # a2 <- method
    move    a3, rSELF                   # a3 <- self
    jal     MterpCheckCast              # (index, &obj, method, self)
    PREFETCH_INST 2
    bnez    v0, MterpPossibleException
    ADVANCE 2
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction

%def op_iget(is_object="0", helper="MterpIGetU32"):
%  field(helper=helper)

%def op_iget_boolean():
%  op_iget(helper="MterpIGetU8")

%def op_iget_boolean_quick():
%  op_iget_quick(load="lbu")

%def op_iget_byte():
%  op_iget(helper="MterpIGetI8")

%def op_iget_byte_quick():
%  op_iget_quick(load="lb")

%def op_iget_char():
%  op_iget(helper="MterpIGetU16")

%def op_iget_char_quick():
%  op_iget_quick(load="lhu")

%def op_iget_object():
%  op_iget(is_object="1", helper="MterpIGetObj")

%def op_iget_object_quick():
    /* For: iget-object-quick */
    /* op vA, vB, offset//CCCC */
    .extern artIGetObjectFromMterp
    srl     a2, rINST, 12               # a2 <- B
    lhu     a1, 2(rPC)                  # a1 <- field byte offset
    EXPORT_PC
    GET_VREG_U a0, a2                   # a0 <- object we're operating on
    jal     artIGetObjectFromMterp      # (obj, offset)
    ld      a3, THREAD_EXCEPTION_OFFSET(rSELF)
    ext     a2, rINST, 8, 4             # a2 <- A
    PREFETCH_INST 2
    bnez    a3, MterpPossibleException  # bail out
    SET_VREG_OBJECT v0, a2              # fp[A] <- v0
    ADVANCE 2                           # advance rPC
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction

%def op_iget_quick(load="lw"):
    /* For: iget-quick, iget-boolean-quick, iget-byte-quick, iget-char-quick, iget-short-quick */
    /* op vA, vB, offset//CCCC */
    srl     a2, rINST, 12               # a2 <- B
    lhu     a1, 2(rPC)                  # a1 <- field byte offset
    GET_VREG_U a3, a2                   # a3 <- object we're operating on
    ext     a4, rINST, 8, 4             # a4 <- A
    daddu   a1, a1, a3
    beqz    a3, common_errNullObject    # object was null
    $load   a0, 0(a1)                   # a0 <- obj.field
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    SET_VREG a0, a4                     # fp[A] <- a0
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction

%def op_iget_short():
%  op_iget(helper="MterpIGetI16")

%def op_iget_short_quick():
%  op_iget_quick(load="lh")

%def op_iget_wide():
%  op_iget(helper="MterpIGetU64")

%def op_iget_wide_quick():
    /* iget-wide-quick vA, vB, offset//CCCC */
    srl     a2, rINST, 12               # a2 <- B
    lhu     a4, 2(rPC)                  # a4 <- field byte offset
    GET_VREG_U a3, a2                   # a3 <- object we're operating on
    ext     a2, rINST, 8, 4             # a2 <- A
    beqz    a3, common_errNullObject    # object was null
    daddu   a4, a3, a4                  # create direct pointer
    lw      a0, 0(a4)
    lw      a1, 4(a4)
    dinsu   a0, a1, 32, 32
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    SET_VREG_WIDE a0, a2
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction

%def op_instance_of():
    /*
     * Check to see if an object reference is an instance of a class.
     *
     * Most common situation is a non-null object, being compared against
     * an already-resolved class.
     */
    /* instance-of vA, vB, class//CCCC */
    .extern MterpInstanceOf
    EXPORT_PC
    lhu     a0, 2(rPC)                  # a0 <- CCCC
    srl     a1, rINST, 12               # a1 <- B
    dlsa    a1, a1, rFP, 2              # a1 <- &object
    ld      a2, OFF_FP_METHOD(rFP)      # a2 <- method
    move    a3, rSELF                   # a3 <- self
    jal     MterpInstanceOf             # (index, &obj, method, self)
    ld      a1, THREAD_EXCEPTION_OFFSET(rSELF)
    ext     a2, rINST, 8, 4             # a2 <- A
    PREFETCH_INST 2
    bnez    a1, MterpException
    ADVANCE 2                           # advance rPC
    SET_VREG v0, a2                     # vA <- v0
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction

%def op_iput(is_object="0", helper="MterpIPutU32"):
%  field(helper=helper)

%def op_iput_boolean():
%  op_iput(helper="MterpIPutU8")

%def op_iput_boolean_quick():
%  op_iput_quick(store="sb")

%def op_iput_byte():
%  op_iput(helper="MterpIPutI8")

%def op_iput_byte_quick():
%  op_iput_quick(store="sb")

%def op_iput_char():
%  op_iput(helper="MterpIPutU16")

%def op_iput_char_quick():
%  op_iput_quick(store="sh")

%def op_iput_object():
%  op_iput(is_object="1", helper="MterpIPutObj")

%def op_iput_object_quick():
    .extern MterpIputObjectQuick
    EXPORT_PC
    daddu   a0, rFP, OFF_FP_SHADOWFRAME
    move    a1, rPC
    move    a2, rINST
    jal     MterpIputObjectQuick
    beqzc   v0, MterpException
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction

%def op_iput_quick(store="sw"):
    /* For: iput-quick, iput-boolean-quick, iput-byte-quick, iput-char-quick, iput-short-quick */
    /* op vA, vB, offset//CCCC */
    srl     a2, rINST, 12               # a2 <- B
    lhu     a1, 2(rPC)                  # a1 <- field byte offset
    GET_VREG_U a3, a2                   # a3 <- fp[B], the object pointer
    ext     a2, rINST, 8, 4             # a2 <- A
    beqz    a3, common_errNullObject    # object was null
    GET_VREG a0, a2                     # a0 <- fp[A]
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    daddu   a1, a1, a3
    $store  a0, 0(a1)                   # obj.field <- a0
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction

%def op_iput_short():
%  op_iput(helper="MterpIPutI16")

%def op_iput_short_quick():
%  op_iput_quick(store="sh")

%def op_iput_wide():
%  op_iput(helper="MterpIPutU64")

%def op_iput_wide_quick():
    /* iput-wide-quick vA, vB, offset//CCCC */
    srl     a2, rINST, 12               # a2 <- B
    lhu     a3, 2(rPC)                  # a3 <- field byte offset
    GET_VREG_U a2, a2                   # a2 <- fp[B], the object pointer
    ext     a0, rINST, 8, 4             # a0 <- A
    beqz    a2, common_errNullObject    # object was null
    GET_VREG_WIDE a0, a0                # a0 <- fp[A]
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    daddu   a1, a2, a3                  # create a direct pointer
    sw      a0, 0(a1)
    dsrl32  a0, a0, 0
    sw      a0, 4(a1)
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction

%def op_new_instance():
    /*
     * Create a new instance of a class.
     */
    /* new-instance vAA, class//BBBB */
    .extern MterpNewInstance
    EXPORT_PC
    daddu   a0, rFP, OFF_FP_SHADOWFRAME
    move    a1, rSELF
    move    a2, rINST
    jal     MterpNewInstance            # (shadow_frame, self, inst_data)
    beqzc   v0, MterpPossibleException
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction

%def op_sget(is_object="0", helper="MterpSGetU32"):
%  field(helper=helper)

%def op_sget_boolean():
%  op_sget(helper="MterpSGetU8")

%def op_sget_byte():
%  op_sget(helper="MterpSGetI8")

%def op_sget_char():
%  op_sget(helper="MterpSGetU16")

%def op_sget_object():
%  op_sget(is_object="1", helper="MterpSGetObj")

%def op_sget_short():
%  op_sget(helper="MterpSGetI16")

%def op_sget_wide():
%  op_sget(helper="MterpSGetU64")

%def op_sput(is_object="0", helper="MterpSPutU32"):
%  field(helper=helper)

%def op_sput_boolean():
%  op_sput(helper="MterpSPutU8")

%def op_sput_byte():
%  op_sput(helper="MterpSPutI8")

%def op_sput_char():
%  op_sput(helper="MterpSPutU16")

%def op_sput_object():
%  op_sput(is_object="1", helper="MterpSPutObj")

%def op_sput_short():
%  op_sput(helper="MterpSPutI16")

%def op_sput_wide():
%  op_sput(helper="MterpSPutU64")
