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

%def op_check_cast():
    /*
     * Check to see if a cast from one class to another is allowed.
     */
    /* check-cast vAA, class@BBBB */
    EXPORT_PC
    FETCH    r0, 1                      @ r0<- BBBB
    mov      r1, rINST, lsr #8          @ r1<- AA
    VREG_INDEX_TO_ADDR r1, r1           @ r1<- &object
    ldr      r2, [rFP, #OFF_FP_METHOD]  @ r2<- method
    mov      r3, rSELF                  @ r3<- self
    bl       MterpCheckCast             @ (index, &obj, method, self)
    PREFETCH_INST 2
    cmp      r0, #0
    bne      MterpPossibleException
    ADVANCE  2
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    GOTO_OPCODE ip                      @ jump to next instruction

%def op_iget(is_object=False, is_wide=False, load="ldr", helper="MterpIGetU32"):
   @ Fast-path which gets the field offset from thread-local cache.
   add      r0, rSELF, #THREAD_INTERPRETER_CACHE_OFFSET       @ cache address
   ubfx     r1, rPC, #2, #THREAD_INTERPRETER_CACHE_SIZE_LOG2  @ entry index
   add      r0, r0, r1, lsl #3            @ entry address within the cache
   ldrd     r0, r1, [r0]                  @ entry key (pc) and value (offset)
   mov      r2, rINST, lsr #12            @ B
   GET_VREG r2, r2                        @ object we're operating on
   cmp      r0, rPC
%  slow_path_label = add_helper(lambda: field(helper))
   bne      ${slow_path_label}            @ cache miss
   cmp      r2, #0
   beq      common_errNullObject          @ null object
%  if is_wide:
     ldrd     r0, r1, [r1, r2]            @ r0,r1 <- obj.field
%  else:
     ${load}  r0, [r2, r1]                @ r0 <- obj.field
%  #endif
%  if is_object:
     UNPOISON_HEAP_REF r0
#if defined(USE_READ_BARRIER)
# if defined(USE_BAKER_READ_BARRIER)
     ldr    ip, [rSELF, #THREAD_IS_GC_MARKING_OFFSET]
     cmp    ip, #0
     bne    .L_${opcode}_mark             @ GC is active
.L_${opcode}_marked:
# else
     bl artReadBarrierMark                @ r0 <- artReadBarrierMark(r0)
# endif
#endif
%  #endif
   ubfx     r2, rINST, #8, #4             @ A
   FETCH_ADVANCE_INST 2                   @ advance rPC, load rINST
%  if is_object:
     SET_VREG_OBJECT r0, r2               @ fp[A]<- r0
%  elif is_wide:
     SET_VREG_WIDE r0, r1, r2             @ fp[A]<- r0, r1
%  else:
     SET_VREG r0, r2                      @ fp[A]<- r0
%  #endif
   GET_INST_OPCODE ip                     @ extract opcode from rINST
   GOTO_OPCODE ip                         @ jump to next instruction
%  if is_object:
#if defined(USE_READ_BARRIER) && defined(USE_BAKER_READ_BARRIER)
.L_${opcode}_mark:
     bl artReadBarrierMark                @ r0 <- artReadBarrierMark(r0)
     b .L_${opcode}_marked
#endif
%  #endif

%def op_iget_boolean():
%  op_iget(load="ldrb", helper="MterpIGetU8")

%def op_iget_boolean_quick():
%  op_iget_quick(load="ldrb")

%def op_iget_byte():
%  op_iget(load="ldrsb", helper="MterpIGetI8")

%def op_iget_byte_quick():
%  op_iget_quick(load="ldrsb")

%def op_iget_char():
%  op_iget(load="ldrh", helper="MterpIGetU16")

%def op_iget_char_quick():
%  op_iget_quick(load="ldrh")

%def op_iget_object():
%  op_iget(is_object=True, helper="MterpIGetObj")

%def op_iget_object_quick():
    /* For: iget-object-quick */
    /* op vA, vB, offset@CCCC */
    mov     r2, rINST, lsr #12          @ r2<- B
    FETCH r1, 1                         @ r1<- field byte offset
    EXPORT_PC
    GET_VREG r0, r2                     @ r0<- object we're operating on
    bl      artIGetObjectFromMterp      @ (obj, offset)
    ldr     r3, [rSELF, #THREAD_EXCEPTION_OFFSET]
    ubfx    r2, rINST, #8, #4           @ r2<- A
    PREFETCH_INST 2
    cmp     r3, #0
    bne     MterpPossibleException      @ bail out
    SET_VREG_OBJECT r0, r2              @ fp[A]<- r0
    ADVANCE 2                           @ advance rPC
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    GOTO_OPCODE ip                      @ jump to next instruction

%def op_iget_quick(load="ldr"):
    /* For: iget-quick, iget-boolean-quick, iget-byte-quick, iget-char-quick, iget-short-quick */
    /* op vA, vB, offset@CCCC */
    mov     r2, rINST, lsr #12          @ r2<- B
    FETCH r1, 1                         @ r1<- field byte offset
    GET_VREG r3, r2                     @ r3<- object we're operating on
    ubfx    r2, rINST, #8, #4           @ r2<- A
    cmp     r3, #0                      @ check object for null
    beq     common_errNullObject        @ object was null
    $load   r0, [r3, r1]                @ r0<- obj.field
    FETCH_ADVANCE_INST 2                @ advance rPC, load rINST
    SET_VREG r0, r2                     @ fp[A]<- r0
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    GOTO_OPCODE ip                      @ jump to next instruction

%def op_iget_short():
%  op_iget(load="ldrsh", helper="MterpIGetI16")

%def op_iget_short_quick():
%  op_iget_quick(load="ldrsh")

%def op_iget_wide():
%  op_iget(is_wide=True, helper="MterpIGetU64")

%def op_iget_wide_quick():
    /* iget-wide-quick vA, vB, offset@CCCC */
    mov     r2, rINST, lsr #12          @ r2<- B
    FETCH ip, 1                         @ ip<- field byte offset
    GET_VREG r3, r2                     @ r3<- object we're operating on
    ubfx    r2, rINST, #8, #4           @ r2<- A
    cmp     r3, #0                      @ check object for null
    beq     common_errNullObject        @ object was null
    ldrd    r0, [r3, ip]                @ r0<- obj.field (64 bits, aligned)
    FETCH_ADVANCE_INST 2                @ advance rPC, load rINST
    VREG_INDEX_TO_ADDR r3, r2           @ r3<- &fp[A]
    CLEAR_SHADOW_PAIR r2, ip, lr        @ Zero out the shadow regs
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    SET_VREG_WIDE_BY_ADDR r0, r1, r3    @ fp[A]<- r0/r1
    GOTO_OPCODE ip                      @ jump to next instruction

%def op_instance_of():
    /*
     * Check to see if an object reference is an instance of a class.
     *
     * Most common situation is a non-null object, being compared against
     * an already-resolved class.
     */
    /* instance-of vA, vB, class@CCCC */
    EXPORT_PC
    FETCH     r0, 1                     @ r0<- CCCC
    mov       r1, rINST, lsr #12        @ r1<- B
    VREG_INDEX_TO_ADDR r1, r1           @ r1<- &object
    ldr       r2, [rFP, #OFF_FP_METHOD] @ r2<- method
    mov       r3, rSELF                 @ r3<- self
    bl        MterpInstanceOf           @ (index, &obj, method, self)
    ldr       r1, [rSELF, #THREAD_EXCEPTION_OFFSET]
    ubfx      r9, rINST, #8, #4         @ r9<- A
    PREFETCH_INST 2
    cmp       r1, #0                    @ exception pending?
    bne       MterpException
    ADVANCE 2                           @ advance rPC
    SET_VREG r0, r9                     @ vA<- r0
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    GOTO_OPCODE ip                      @ jump to next instruction

%def op_iput(helper="MterpIPutU32"):
%  field(helper=helper)

%def op_iput_boolean():
%  op_iput(helper="MterpIPutU8")

%def op_iput_boolean_quick():
%  op_iput_quick(store="strb")

%def op_iput_byte():
%  op_iput(helper="MterpIPutI8")

%def op_iput_byte_quick():
%  op_iput_quick(store="strb")

%def op_iput_char():
%  op_iput(helper="MterpIPutU16")

%def op_iput_char_quick():
%  op_iput_quick(store="strh")

%def op_iput_object():
%  op_iput(helper="MterpIPutObj")

%def op_iput_object_quick():
    EXPORT_PC
    add     r0, rFP, #OFF_FP_SHADOWFRAME
    mov     r1, rPC
    mov     r2, rINST
    bl      MterpIputObjectQuick
    cmp     r0, #0
    beq     MterpException
    FETCH_ADVANCE_INST 2                @ advance rPC, load rINST
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    GOTO_OPCODE ip                      @ jump to next instruction

%def op_iput_quick(store="str"):
    /* For: iput-quick, iput-object-quick */
    /* op vA, vB, offset@CCCC */
    mov     r2, rINST, lsr #12          @ r2<- B
    FETCH r1, 1                         @ r1<- field byte offset
    GET_VREG r3, r2                     @ r3<- fp[B], the object pointer
    ubfx    r2, rINST, #8, #4           @ r2<- A
    cmp     r3, #0                      @ check object for null
    beq     common_errNullObject        @ object was null
    GET_VREG r0, r2                     @ r0<- fp[A]
    FETCH_ADVANCE_INST 2                @ advance rPC, load rINST
    $store     r0, [r3, r1]             @ obj.field<- r0
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    GOTO_OPCODE ip                      @ jump to next instruction

%def op_iput_short():
%  op_iput(helper="MterpIPutI16")

%def op_iput_short_quick():
%  op_iput_quick(store="strh")

%def op_iput_wide():
%  op_iput(helper="MterpIPutU64")

%def op_iput_wide_quick():
    /* iput-wide-quick vA, vB, offset@CCCC */
    mov     r2, rINST, lsr #12          @ r2<- B
    FETCH r3, 1                         @ r3<- field byte offset
    GET_VREG r2, r2                     @ r2<- fp[B], the object pointer
    ubfx    r0, rINST, #8, #4           @ r0<- A
    cmp     r2, #0                      @ check object for null
    beq     common_errNullObject        @ object was null
    VREG_INDEX_TO_ADDR r0, r0           @ r0<- &fp[A]
    GET_VREG_WIDE_BY_ADDR r0, r1, r0    @ r0/r1<- fp[A]/fp[A+1]
    FETCH_ADVANCE_INST 2                @ advance rPC, load rINST
    strd    r0, [r2, r3]                @ obj.field<- r0/r1
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    GOTO_OPCODE ip                      @ jump to next instruction

%def op_new_instance():
    /*
     * Create a new instance of a class.
     */
    /* new-instance vAA, class@BBBB */
    EXPORT_PC
    add     r0, rFP, #OFF_FP_SHADOWFRAME
    mov     r1, rSELF
    mov     r2, rINST
    bl      MterpNewInstance           @ (shadow_frame, self, inst_data)
    cmp     r0, #0
    beq     MterpPossibleException
    FETCH_ADVANCE_INST 2               @ advance rPC, load rINST
    GET_INST_OPCODE ip                 @ extract opcode from rINST
    GOTO_OPCODE ip                     @ jump to next instruction

%def op_sget(helper="MterpSGetU32"):
%  field(helper=helper)

%def op_sget_boolean():
%  op_sget(helper="MterpSGetU8")

%def op_sget_byte():
%  op_sget(helper="MterpSGetI8")

%def op_sget_char():
%  op_sget(helper="MterpSGetU16")

%def op_sget_object():
%  op_sget(helper="MterpSGetObj")

%def op_sget_short():
%  op_sget(helper="MterpSGetI16")

%def op_sget_wide():
%  op_sget(helper="MterpSGetU64")

%def op_sput(helper="MterpSPutU32"):
%  field(helper=helper)

%def op_sput_boolean():
%  op_sput(helper="MterpSPutU8")

%def op_sput_byte():
%  op_sput(helper="MterpSPutI8")

%def op_sput_char():
%  op_sput(helper="MterpSPutU16")

%def op_sput_object():
%  op_sput(helper="MterpSPutObj")

%def op_sput_short():
%  op_sput(helper="MterpSPutI16")

%def op_sput_wide():
%  op_sput(helper="MterpSPutU64")
