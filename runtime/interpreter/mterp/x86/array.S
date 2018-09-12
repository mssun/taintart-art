%def op_aget(load="movl", shift="4", data_offset="MIRROR_INT_ARRAY_DATA_OFFSET"):
/*
 * Array get, 32 bits or less.  vAA <- vBB[vCC].
 *
 * for: aget, aget-boolean, aget-byte, aget-char, aget-short
 *
 */
    /* op vAA, vBB, vCC */
    movzbl  2(rPC), %eax                    # eax <- BB
    movzbl  3(rPC), %ecx                    # ecx <- CC
    GET_VREG %eax, %eax                     # eax <- vBB (array object)
    GET_VREG %ecx, %ecx                     # ecx <- vCC (requested index)
    testl   %eax, %eax                      # null array object?
    je      common_errNullObject            # bail if so
    cmpl    MIRROR_ARRAY_LENGTH_OFFSET(%eax), %ecx
    jae     common_errArrayIndex            # index >= length, bail.
    $load   $data_offset(%eax,%ecx,$shift), %eax
    SET_VREG %eax, rINST
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
    movzbl  2(rPC), %eax                    # eax <- BB
    movzbl  3(rPC), %ecx                    # ecx <- CC
    GET_VREG %eax, %eax                     # eax <- vBB (array object)
    GET_VREG %ecx, %ecx                     # ecs <- vCC (requested index)
    EXPORT_PC
    movl    %eax, OUT_ARG0(%esp)
    movl    %ecx, OUT_ARG1(%esp)
    call    SYMBOL(artAGetObjectFromMterp)  # (array, index)
    movl    rSELF, %ecx
    RESTORE_IBASE_FROM_SELF %ecx
    cmpl    $$0, THREAD_EXCEPTION_OFFSET(%ecx)
    jnz     MterpException
    SET_VREG_OBJECT %eax, rINST
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def op_aget_short():
%  op_aget(load="movswl", shift="2", data_offset="MIRROR_SHORT_ARRAY_DATA_OFFSET")

%def op_aget_wide():
/*
 * Array get, 64 bits.  vAA <- vBB[vCC].
 */
    /* aget-wide vAA, vBB, vCC */
    movzbl  2(rPC), %eax                    # eax <- BB
    movzbl  3(rPC), %ecx                    # ecx <- CC
    GET_VREG %eax, %eax                     # eax <- vBB (array object)
    GET_VREG %ecx, %ecx                     # ecx <- vCC (requested index)
    testl   %eax, %eax                      # null array object?
    je      common_errNullObject            # bail if so
    cmpl    MIRROR_ARRAY_LENGTH_OFFSET(%eax), %ecx
    jae     common_errArrayIndex            # index >= length, bail.
    leal    MIRROR_WIDE_ARRAY_DATA_OFFSET(%eax,%ecx,8), %eax
    movq    (%eax), %xmm0                   # xmm0 <- vBB[vCC]
    SET_WIDE_FP_VREG %xmm0, rINST           # vAA <- xmm0
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def op_aput(reg="rINST", store="movl", shift="4", data_offset="MIRROR_INT_ARRAY_DATA_OFFSET"):
/*
 * Array put, 32 bits or less.  vBB[vCC] <- vAA.
 *
 * for: aput, aput-boolean, aput-byte, aput-char, aput-short
 *
 */
    /* op vAA, vBB, vCC */
    movzbl  2(rPC), %eax                    # eax <- BB
    movzbl  3(rPC), %ecx                    # ecx <- CC
    GET_VREG %eax, %eax                     # eax <- vBB (array object)
    GET_VREG %ecx, %ecx                     # ecx <- vCC (requested index)
    testl   %eax, %eax                      # null array object?
    je      common_errNullObject            # bail if so
    cmpl    MIRROR_ARRAY_LENGTH_OFFSET(%eax), %ecx
    jae     common_errArrayIndex            # index >= length, bail.
    leal    $data_offset(%eax,%ecx,$shift), %eax
    GET_VREG rINST, rINST
    $store  $reg, (%eax)
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
    leal    OFF_FP_SHADOWFRAME(rFP), %eax
    movl    %eax, OUT_ARG0(%esp)
    movl    rPC, OUT_ARG1(%esp)
    REFRESH_INST ${opnum}
    movl    rINST, OUT_ARG2(%esp)
    call    SYMBOL(MterpAputObject)         # (array, index)
    RESTORE_IBASE
    testb   %al, %al
    jz      MterpPossibleException
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def op_aput_short():
%  op_aput(reg="rINSTw", store="movw", shift="2", data_offset="MIRROR_SHORT_ARRAY_DATA_OFFSET")

%def op_aput_wide():
/*
 * Array put, 64 bits.  vBB[vCC] <- vAA.
 *
 */
    /* aput-wide vAA, vBB, vCC */
    movzbl  2(rPC), %eax                    # eax <- BB
    movzbl  3(rPC), %ecx                    # ecx <- CC
    GET_VREG %eax, %eax                     # eax <- vBB (array object)
    GET_VREG %ecx, %ecx                     # ecx <- vCC (requested index)
    testl   %eax, %eax                      # null array object?
    je      common_errNullObject            # bail if so
    cmpl    MIRROR_ARRAY_LENGTH_OFFSET(%eax), %ecx
    jae     common_errArrayIndex            # index >= length, bail.
    leal    MIRROR_WIDE_ARRAY_DATA_OFFSET(%eax,%ecx,8), %eax
    GET_WIDE_FP_VREG %xmm0, rINST           # xmm0 <- vAA
    movq    %xmm0, (%eax)                   # vBB[vCC] <- xmm0
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2

%def op_array_length():
/*
 * Return the length of an array.
 */
    mov     rINST, %eax                     # eax <- BA
    sarl    $$4, rINST                      # rINST <- B
    GET_VREG %ecx, rINST                    # ecx <- vB (object ref)
    testl   %ecx, %ecx                      # is null?
    je      common_errNullObject
    andb    $$0xf, %al                      # eax <- A
    movl    MIRROR_ARRAY_LENGTH_OFFSET(%ecx), rINST
    SET_VREG rINST, %eax
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 1

%def op_fill_array_data():
    /* fill-array-data vAA, +BBBBBBBB */
    EXPORT_PC
    movl    2(rPC), %ecx                    # ecx <- BBBBbbbb
    leal    (rPC,%ecx,2), %ecx              # ecx <- PC + BBBBbbbb*2
    GET_VREG %eax, rINST                    # eax <- vAA (array object)
    movl    %eax, OUT_ARG0(%esp)
    movl    %ecx, OUT_ARG1(%esp)
    call    SYMBOL(MterpFillArrayData)      # (obj, payload)
    REFRESH_IBASE
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
    leal    OFF_FP_SHADOWFRAME(rFP), %eax
    movl    %eax, OUT_ARG0(%esp)
    movl    rPC, OUT_ARG1(%esp)
    movl    rSELF, %ecx
    movl    %ecx, OUT_ARG2(%esp)
    call    SYMBOL($helper)
    REFRESH_IBASE
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
    leal    OFF_FP_SHADOWFRAME(rFP), %eax
    movl    %eax, OUT_ARG0(%esp)
    movl    rPC, OUT_ARG1(%esp)
    REFRESH_INST ${opnum}
    movl    rINST, OUT_ARG2(%esp)
    movl    rSELF, %ecx
    movl    %ecx, OUT_ARG3(%esp)
    call    SYMBOL(MterpNewArray)
    RESTORE_IBASE
    testb   %al, %al                        # 0 means an exception is thrown
    jz      MterpPossibleException
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2
