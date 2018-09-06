%def op_aput_byte():
%  op_aput(reg="rINSTbl", store="movb", shift="1", data_offset="MIRROR_BYTE_ARRAY_DATA_OFFSET")
