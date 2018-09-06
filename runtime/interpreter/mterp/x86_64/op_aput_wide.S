%def op_aput_wide():
%  op_aput(reg="rINSTq", store="movq", shift="8", data_offset="MIRROR_WIDE_ARRAY_DATA_OFFSET", wide="1")
