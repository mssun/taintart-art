%def op_aput_boolean():
%  op_aput(reg="rINSTbl", store="movb", shift="1", data_offset="MIRROR_BOOLEAN_ARRAY_DATA_OFFSET")
