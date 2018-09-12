%def field(helper=""):
TODO

%def op_check_cast():
    /*
     * Check to see if a cast from one class to another is allowed.
     */
    /* check-cast vAA, class@BBBB */
    EXPORT_PC()
    FETCH(a0, 1)                           #  a0 <- BBBB
    GET_OPA(a1)                            #  a1 <- AA
    EAS2(a1, rFP, a1)                      #  a1 <- &object
    lw     a2, OFF_FP_METHOD(rFP)          #  a2 <- method
    move   a3, rSELF                       #  a3 <- self
    JAL(MterpCheckCast)                    #  v0 <- CheckCast(index, &obj, method, self)
    PREFETCH_INST(2)
    bnez   v0, MterpPossibleException
    ADVANCE(2)
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction

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
    /* op vA, vB, offset@CCCC */
    GET_OPB(a2)                            #  a2 <- B
    FETCH(a1, 1)                           #  a1 <- field byte offset
    EXPORT_PC()
    GET_VREG(a0, a2)                       #  a0 <- object we're operating on
    JAL(artIGetObjectFromMterp)            #  v0 <- GetObj(obj, offset)
    lw   a3, THREAD_EXCEPTION_OFFSET(rSELF)
    GET_OPA4(a2)                           #  a2<- A+
    PREFETCH_INST(2)                       #  load rINST
    bnez a3, MterpPossibleException        #  bail out
    ADVANCE(2)                             #  advance rPC
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_OBJECT_GOTO(v0, a2, t0)       #  fp[A] <- v0

%def op_iget_quick(load="lw"):
    /* For: iget-quick, iget-boolean-quick, iget-byte-quick, iget-char-quick, iget-short-quick */
    /* op vA, vB, offset@CCCC */
    GET_OPB(a2)                            #  a2 <- B
    GET_VREG(a3, a2)                       #  a3 <- object we're operating on
    FETCH(a1, 1)                           #  a1 <- field byte offset
    GET_OPA4(a2)                           #  a2 <- A(+)
    # check object for null
    beqz      a3, common_errNullObject     #  object was null
    addu      t0, a3, a1
    $load     a0, 0(t0)                    #  a0 <- obj.field (8/16/32 bits)
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, a2, t0)              #  fp[A] <- a0

%def op_iget_short():
%  op_iget(helper="MterpIGetI16")

%def op_iget_short_quick():
%  op_iget_quick(load="lh")

%def op_iget_wide():
%  op_iget(helper="MterpIGetU64")

%def op_iget_wide_quick():
    /* iget-wide-quick vA, vB, offset@CCCC */
    GET_OPB(a2)                            #  a2 <- B
    GET_VREG(a3, a2)                       #  a3 <- object we're operating on
    FETCH(a1, 1)                           #  a1 <- field byte offset
    GET_OPA4(a2)                           #  a2 <- A(+)
    # check object for null
    beqz      a3, common_errNullObject     #  object was null
    addu      t0, a3, a1                   #  t0 <- a3 + a1
    LOAD64(a0, a1, t0)                     #  a0 <- obj.field (64 bits, aligned)
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_GOTO(a0, a1, a2, t0)        #  fp[A] <- a0/a1

%def op_instance_of():
    /*
     * Check to see if an object reference is an instance of a class.
     *
     * Most common situation is a non-null object, being compared against
     * an already-resolved class.
     */
    /* instance-of vA, vB, class@CCCC */
    EXPORT_PC()
    FETCH(a0, 1)                           # a0 <- CCCC
    GET_OPB(a1)                            # a1 <- B
    EAS2(a1, rFP, a1)                      # a1 <- &object
    lw    a2, OFF_FP_METHOD(rFP)           # a2 <- method
    move  a3, rSELF                        # a3 <- self
    GET_OPA4(rOBJ)                         # rOBJ <- A+
    JAL(MterpInstanceOf)                   # v0 <- Mterp(index, &obj, method, self)
    lw   a1, THREAD_EXCEPTION_OFFSET(rSELF)
    PREFETCH_INST(2)                       # load rINST
    bnez a1, MterpException
    ADVANCE(2)                             # advance rPC
    GET_INST_OPCODE(t0)                    # extract opcode from rINST
    SET_VREG_GOTO(v0, rOBJ, t0)            # vA <- v0

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
    /* For: iput-object-quick */
    /* op vA, vB, offset@CCCC */
    EXPORT_PC()
    addu   a0, rFP, OFF_FP_SHADOWFRAME
    move   a1, rPC
    move   a2, rINST
    JAL(MterpIputObjectQuick)
    beqz   v0, MterpException
    FETCH_ADVANCE_INST(2)               # advance rPC, load rINST
    GET_INST_OPCODE(t0)                 # extract opcode from rINST
    GOTO_OPCODE(t0)                     # jump to next instruction

%def op_iput_quick(store="sw"):
    /* For: iput-quick, iput-object-quick */
    /* op vA, vB, offset@CCCC */
    GET_OPB(a2)                            #  a2 <- B
    GET_VREG(a3, a2)                       #  a3 <- fp[B], the object pointer
    FETCH(a1, 1)                           #  a1 <- field byte offset
    GET_OPA4(a2)                           #  a2 <- A(+)
    beqz      a3, common_errNullObject     #  object was null
    GET_VREG(a0, a2)                       #  a0 <- fp[A]
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    addu      t0, a3, a1
    GET_INST_OPCODE(t1)                    #  extract opcode from rINST
    GET_OPCODE_TARGET(t1)
    $store    a0, 0(t0)                    #  obj.field (8/16/32 bits) <- a0
    JR(t1)                                 #  jump to next instruction

%def op_iput_short():
%  op_iput(helper="MterpIPutI16")

%def op_iput_short_quick():
%  op_iput_quick(store="sh")

%def op_iput_wide():
%  op_iput(helper="MterpIPutU64")

%def op_iput_wide_quick():
    /* iput-wide-quick vA, vB, offset@CCCC */
    GET_OPA4(a0)                           #  a0 <- A(+)
    GET_OPB(a1)                            #  a1 <- B
    GET_VREG(a2, a1)                       #  a2 <- fp[B], the object pointer
    # check object for null
    beqz      a2, common_errNullObject     #  object was null
    EAS2(a3, rFP, a0)                      #  a3 <- &fp[A]
    LOAD64(a0, a1, a3)                     #  a0/a1 <- fp[A]
    FETCH(a3, 1)                           #  a3 <- field byte offset
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    addu      a2, a2, a3                   #  obj.field (64 bits, aligned) <- a0/a1
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GET_OPCODE_TARGET(t0)
    STORE64(a0, a1, a2)                    #  obj.field (64 bits, aligned) <- a0/a1
    JR(t0)                                 #  jump to next instruction

%def op_new_instance():
    /*
     * Create a new instance of a class.
     */
    /* new-instance vAA, class@BBBB */
    EXPORT_PC()
    addu   a0, rFP, OFF_FP_SHADOWFRAME
    move   a1, rSELF
    move   a2, rINST
    JAL(MterpNewInstance)
    beqz   v0, MterpPossibleException
    FETCH_ADVANCE_INST(2)               # advance rPC, load rINST
    GET_INST_OPCODE(t0)                 # extract opcode from rINST
    GOTO_OPCODE(t0)                     # jump to next instruction

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
