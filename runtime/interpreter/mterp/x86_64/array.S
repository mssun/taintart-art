%def op_aget(load="movl", shift="4", data_offset="MIRROR_INT_ARRAY_DATA_OFFSET", wide="0"):
/*
 * Array get, 32 bits or less.  vAA <- vBB[vCC].
 *
 * for: aget, aget-boolean, aget-byte, aget-char, aget-short, aget-wide
 *
 */
    /* op vAA, vBB, vCC */
    movzbq  2(rPC), %rax                    # eax <- BB
    movzbq  3(rPC), %rcx                    # ecx <- CC
    GET_VREG %eax, %rax                     # eax <- vBB (array object)
    GET_VREG %ecx, %rcx                     # ecx <- vCC (requested index)
    testl   %eax, %eax                      # null array object?
    je      common_errNullObject            # bail if so
    cmpl    MIRROR_ARRAY_LENGTH_OFFSET(%eax), %ecx
    jae     common_errArrayIndex            # index >= length, bail.
    .if $wide
    movq    $data_offset(%rax,%rcx,8), %rax
    SET_WIDE_VREG %rax, rINSTq
    .else
    $load   $data_offset(%rax,%rcx,$shift), %eax
    SET_VREG %eax, rINSTq
    .endif
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def op_aget_boolean():
%  op_aget(load="movzbl", shift="1", data_offset="MIRROR_BOOLEAN_ARRAY_DATA_OFFSET")

%def op_aget_byte():
%  op_aget(load="movsbl", shift="1", data_offset="MIRROR_BYTE_ARRAY_DATA_OFFSET")

%def op_aget_char():
%  op_aget(load="movzwl", shift="2", data_offset="MIRROR_CHAR_ARRAY_DATA_OFFSET")

%def op_aget_object():
/*
 * Array object get.  vAA <- vBB[vCC].
 *
 * for: aget-object
 */
    /* op vAA, vBB, vCC */
    movzbq  2(rPC), %rax                    # rax <- BB
    movzbq  3(rPC), %rcx                    # rcx <- CC
    GET_VREG OUT_32_ARG0, %rax              # eax <- vBB (array object)
    GET_VREG OUT_32_ARG1, %rcx              # ecx <- vCC (requested index)
    EXPORT_PC
    call    SYMBOL(artAGetObjectFromMterp)  # (array, index)
    movq    rSELF, %rcx
    cmpq    $$0, THREAD_EXCEPTION_OFFSET(%rcx)
    jnz     MterpException
    SET_VREG_OBJECT %eax, rINSTq
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def op_aget_short():
%  op_aget(load="movswl", shift="2", data_offset="MIRROR_SHORT_ARRAY_DATA_OFFSET")

%def op_aget_wide():
%  op_aget(load="movq", shift="8", data_offset="MIRROR_WIDE_ARRAY_DATA_OFFSET", wide="1")

%def op_aput(reg="rINST", store="movl", shift="4", data_offset="MIRROR_INT_ARRAY_DATA_OFFSET", wide="0"):
/*
 * Array put, 32 bits or less.  vBB[vCC] <- vAA.
 *
 * for: aput, aput-boolean, aput-byte, aput-char, aput-short, aput-wide
 *
 */
    /* op vAA, vBB, vCC */
    movzbq  2(rPC), %rax                    # rax <- BB
    movzbq  3(rPC), %rcx                    # rcx <- CC
    GET_VREG %eax, %rax                     # eax <- vBB (array object)
    GET_VREG %ecx, %rcx                     # ecx <- vCC (requested index)
    testl   %eax, %eax                      # null array object?
    je      common_errNullObject            # bail if so
    cmpl    MIRROR_ARRAY_LENGTH_OFFSET(%eax), %ecx
    jae     common_errArrayIndex            # index >= length, bail.
    .if $wide
    GET_WIDE_VREG rINSTq, rINSTq
    .else
    GET_VREG rINST, rINSTq
    .endif
    $store    $reg, $data_offset(%rax,%rcx,$shift)
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def op_aput_boolean():
%  op_aput(reg="rINSTbl", store="movb", shift="1", data_offset="MIRROR_BOOLEAN_ARRAY_DATA_OFFSET")

%def op_aput_byte():
%  op_aput(reg="rINSTbl", store="movb", shift="1", data_offset="MIRROR_BYTE_ARRAY_DATA_OFFSET")

%def op_aput_char():
%  op_aput(reg="rINSTw", store="movw", shift="2", data_offset="MIRROR_CHAR_ARRAY_DATA_OFFSET")

%def op_aput_object():
/*
 * Store an object into an array.  vBB[vCC] <- vAA.
 */
    /* op vAA, vBB, vCC */
    EXPORT_PC
    leaq    OFF_FP_SHADOWFRAME(rFP), OUT_ARG0
    movq    rPC, OUT_ARG1
    REFRESH_INST ${opnum}
    movq    rINSTq, OUT_ARG2
    call    SYMBOL(MterpAputObject)         # (array, index)
    testb   %al, %al
    jz      MterpPossibleException
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def op_aput_short():
%  op_aput(reg="rINSTw", store="movw", shift="2", data_offset="MIRROR_SHORT_ARRAY_DATA_OFFSET")

%def op_aput_wide():
%  op_aput(reg="rINSTq", store="movq", shift="8", data_offset="MIRROR_WIDE_ARRAY_DATA_OFFSET", wide="1")

%def op_array_length():
/*
 * Return the length of an array.
 */
    movl    rINST, %eax                     # eax <- BA
    sarl    $$4, rINST                      # rINST <- B
    GET_VREG %ecx, rINSTq                   # ecx <- vB (object ref)
    testl   %ecx, %ecx                      # is null?
    je      common_errNullObject
    andb    $$0xf, %al                      # eax <- A
    movl    MIRROR_ARRAY_LENGTH_OFFSET(%rcx), rINST
    SET_VREG rINST, %rax
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 1

%def op_fill_array_data():
    /* fill-array-data vAA, +BBBBBBBB */
    EXPORT_PC
    movslq  2(rPC), %rcx                    # rcx <- ssssssssBBBBbbbb
    leaq    (rPC,%rcx,2), OUT_ARG1          # OUT_ARG1 <- PC + ssssssssBBBBbbbb*2
    GET_VREG OUT_32_ARG0, rINSTq            # OUT_ARG0 <- vAA (array object)
    call    SYMBOL(MterpFillArrayData)      # (obj, payload)
    testb   %al, %al                        # 0 means an exception is thrown
    jz      MterpPossibleException
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 3

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
    leaq    OFF_FP_SHADOWFRAME(rFP), OUT_ARG0
    movq    rPC, OUT_ARG1
    movq    rSELF, OUT_ARG2
    call    SYMBOL($helper)
    testb   %al, %al                        # 0 means an exception is thrown
    jz      MterpPossibleException
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 3

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
    leaq    OFF_FP_SHADOWFRAME(rFP), OUT_ARG0
    movq    rPC, OUT_ARG1
    REFRESH_INST ${opnum}
    movq    rINSTq, OUT_ARG2
    movq    rSELF, OUT_ARG3
    call    SYMBOL(MterpNewArray)
    testb   %al, %al                        # 0 means an exception is thrown
    jz      MterpPossibleException
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2
