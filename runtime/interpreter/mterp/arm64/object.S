%def field(helper=""):
    /*
     * General field read / write (iget-* iput-* sget-* sput-*).
     */
    .extern $helper
    mov      x0, xPC                       // arg0: Instruction* inst
    mov      x1, xINST                     // arg1: uint16_t inst_data
    add      x2, xFP, #OFF_FP_SHADOWFRAME  // arg2: ShadowFrame* sf
    mov      x3, xSELF                     // arg3: Thread* self
    PREFETCH_INST 2                        // prefetch next opcode
    bl       $helper
    cbz      x0, MterpPossibleException
    ADVANCE 2
    GET_INST_OPCODE ip                     // extract opcode from rINST
    GOTO_OPCODE ip                         // jump to next instruction

%def op_check_cast():
    /*
     * Check to see if a cast from one class to another is allowed.
     */
    /* check-cast vAA, class//BBBB */
    EXPORT_PC
    FETCH    w0, 1                      // w0<- BBBB
    lsr      w1, wINST, #8              // w1<- AA
    VREG_INDEX_TO_ADDR x1, w1           // w1<- &object
    ldr      x2, [xFP, #OFF_FP_METHOD]  // w2<- method
    mov      x3, xSELF                  // w3<- self
    bl       MterpCheckCast             // (index, &obj, method, self)
    PREFETCH_INST 2
    cbnz     w0, MterpPossibleException
    ADVANCE  2
    GET_INST_OPCODE ip                  // extract opcode from rINST
    GOTO_OPCODE ip                      // jump to next instruction

%def op_iget(is_object=False, is_wide=False, load="ldr", helper="MterpIGetU32"):
   // Fast-path which gets the field offset from thread-local cache.
   add      x0, xSELF, #THREAD_INTERPRETER_CACHE_OFFSET       // cache address
   ubfx     x1, xPC, #2, #THREAD_INTERPRETER_CACHE_SIZE_LOG2  // entry index
   add      x0, x0, x1, lsl #4            // entry address within the cache
   ldp      x0, x1, [x0]                  // entry key (pc) and value (offset)
   lsr      w2, wINST, #12                // B
   GET_VREG w2, w2                        // object we're operating on
   cmp      x0, xPC
%  slow_path_label = add_helper(lambda: field(helper))
   b.ne     ${slow_path_label}            // cache miss
   cbz      w2, common_errNullObject      // null object
%  if is_wide:
     ldr      x0, [x2, x1]                // x0<- obj.field
%  else:
     ${load}  w0, [x2, x1]                // w0<- obj.field
%  #endif
%  if is_object:
     UNPOISON_HEAP_REF w0
#if defined(USE_READ_BARRIER)
# if defined(USE_BAKER_READ_BARRIER)
     ldr    w1, [xSELF, #THREAD_IS_GC_MARKING_OFFSET]
     cbnz   w1, .L_${opcode}_mark         // GC is active.
.L_${opcode}_marked:
# else
     bl artReadBarrierMark                // x0 <- artReadBarrierMark(x0)
# endif
#endif
%  #endif
   ubfx     w2, wINST, #8, #4             // w2<- A
   FETCH_ADVANCE_INST 2                   // advance rPC, load rINST
%  if is_object:
     SET_VREG_OBJECT w0, w2               // fp[A]<- w0
%  elif is_wide:
     SET_VREG_WIDE x0, w2                 // fp[A]<- x0
%  else:
     SET_VREG w0, w2                      // fp[A]<- w0
%  #endif
   GET_INST_OPCODE ip                     // extract opcode from rINST
   GOTO_OPCODE ip                         // jump to next instruction
%  if is_object:
#if defined(USE_READ_BARRIER) && defined(USE_BAKER_READ_BARRIER)
.L_${opcode}_mark:
     bl artReadBarrierMark                // x0 <- artReadBarrierMark(x0)
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
    /* op vA, vB, offset//CCCC */
    lsr     w2, wINST, #12              // w2<- B
    FETCH w1, 1                         // w1<- field byte offset
    EXPORT_PC
    GET_VREG w0, w2                     // w0<- object we're operating on
    bl      artIGetObjectFromMterp      // (obj, offset)
    ldr     x3, [xSELF, #THREAD_EXCEPTION_OFFSET]
    ubfx    w2, wINST, #8, #4           // w2<- A
    PREFETCH_INST 2
    cbnz    w3, MterpPossibleException      // bail out
    SET_VREG_OBJECT w0, w2              // fp[A]<- w0
    ADVANCE 2                           // advance rPC
    GET_INST_OPCODE ip                  // extract opcode from wINST
    GOTO_OPCODE ip                      // jump to next instruction

%def op_iget_quick(load="ldr", extend=""):
    /* For: iget-quick, iget-boolean-quick, iget-byte-quick, iget-char-quick, iget-short-quick */
    /* op vA, vB, offset//CCCC */
    lsr     w2, wINST, #12              // w2<- B
    FETCH w1, 1                         // w1<- field byte offset
    GET_VREG w3, w2                     // w3<- object we're operating on
    ubfx    w2, wINST, #8, #4           // w2<- A
    cbz     w3, common_errNullObject    // object was null
    $load   w0, [x3, x1]                // w0<- obj.field
    FETCH_ADVANCE_INST 2                // advance rPC, load rINST
    $extend
    SET_VREG w0, w2                     // fp[A]<- w0
    GET_INST_OPCODE ip                  // extract opcode from rINST
    GOTO_OPCODE ip                      // jump to next instruction

%def op_iget_short():
%  op_iget(load="ldrsh", helper="MterpIGetI16")

%def op_iget_short_quick():
%  op_iget_quick(load="ldrsh")

%def op_iget_wide():
%  op_iget(is_wide=True, helper="MterpIGetU64")

%def op_iget_wide_quick():
    /* iget-wide-quick vA, vB, offset//CCCC */
    lsr     w2, wINST, #12              // w2<- B
    FETCH w4, 1                         // w4<- field byte offset
    GET_VREG w3, w2                     // w3<- object we're operating on
    ubfx    w2, wINST, #8, #4           // w2<- A
    cbz     w3, common_errNullObject    // object was null
    ldr     x0, [x3, x4]                // x0<- obj.field
    FETCH_ADVANCE_INST 2                // advance rPC, load wINST
    SET_VREG_WIDE x0, w2
    GET_INST_OPCODE ip                  // extract opcode from wINST
    GOTO_OPCODE ip                      // jump to next instruction

%def op_instance_of():
    /*
     * Check to see if an object reference is an instance of a class.
     *
     * Most common situation is a non-null object, being compared against
     * an already-resolved class.
     */
    /* instance-of vA, vB, class//CCCC */
    EXPORT_PC
    FETCH     w0, 1                     // w0<- CCCC
    lsr       w1, wINST, #12            // w1<- B
    VREG_INDEX_TO_ADDR x1, w1           // w1<- &object
    ldr       x2, [xFP, #OFF_FP_METHOD] // w2<- method
    mov       x3, xSELF                 // w3<- self
    bl        MterpInstanceOf           // (index, &obj, method, self)
    ldr       x1, [xSELF, #THREAD_EXCEPTION_OFFSET]
    ubfx      w2, wINST, #8, #4         // w2<- A
    PREFETCH_INST 2
    cbnz      x1, MterpException
    ADVANCE 2                           // advance rPC
    SET_VREG w0, w2                     // vA<- w0
    GET_INST_OPCODE ip                  // extract opcode from rINST
    GOTO_OPCODE ip                      // jump to next instruction

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
    add     x0, xFP, #OFF_FP_SHADOWFRAME
    mov     x1, xPC
    mov     w2, wINST
    bl      MterpIputObjectQuick
    cbz     w0, MterpException
    FETCH_ADVANCE_INST 2                // advance rPC, load rINST
    GET_INST_OPCODE ip                  // extract opcode from rINST
    GOTO_OPCODE ip                      // jump to next instruction

%def op_iput_quick(store="str"):
    /* For: iput-quick, iput-object-quick */
    /* op vA, vB, offset//CCCC */
    lsr     w2, wINST, #12              // w2<- B
    FETCH w1, 1                         // w1<- field byte offset
    GET_VREG w3, w2                     // w3<- fp[B], the object pointer
    ubfx    w2, wINST, #8, #4           // w2<- A
    cbz     w3, common_errNullObject    // object was null
    GET_VREG w0, w2                     // w0<- fp[A]
    FETCH_ADVANCE_INST 2                // advance rPC, load rINST
    $store     w0, [x3, x1]             // obj.field<- w0
    GET_INST_OPCODE ip                  // extract opcode from rINST
    GOTO_OPCODE ip                      // jump to next instruction

%def op_iput_short():
%  op_iput(helper="MterpIPutI16")

%def op_iput_short_quick():
%  op_iput_quick(store="strh")

%def op_iput_wide():
%  op_iput(helper="MterpIPutU64")

%def op_iput_wide_quick():
    /* iput-wide-quick vA, vB, offset//CCCC */
    lsr     w2, wINST, #12              // w2<- B
    FETCH w3, 1                         // w3<- field byte offset
    GET_VREG w2, w2                     // w2<- fp[B], the object pointer
    ubfx    w0, wINST, #8, #4           // w0<- A
    cbz     w2, common_errNullObject    // object was null
    GET_VREG_WIDE x0, w0                // x0<- fp[A]
    FETCH_ADVANCE_INST 2                // advance rPC, load wINST
    str     x0, [x2, x3]                // obj.field<- x0
    GET_INST_OPCODE ip                  // extract opcode from wINST
    GOTO_OPCODE ip                      // jump to next instruction

%def op_new_instance():
    /*
     * Create a new instance of a class.
     */
    /* new-instance vAA, class//BBBB */
    EXPORT_PC
    add     x0, xFP, #OFF_FP_SHADOWFRAME
    mov     x1, xSELF
    mov     w2, wINST
    bl      MterpNewInstance           // (shadow_frame, self, inst_data)
    cbz     w0, MterpPossibleException
    FETCH_ADVANCE_INST 2               // advance rPC, load rINST
    GET_INST_OPCODE ip                 // extract opcode from rINST
    GOTO_OPCODE ip                     // jump to next instruction

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
