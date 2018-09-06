%def op_aput_char():
%  op_aput(reg="rINSTw", store="movw", shift="2", data_offset="MIRROR_CHAR_ARRAY_DATA_OFFSET")
