%def op_aget_char():
%  op_aget(load="movzwl", shift="2", data_offset="MIRROR_CHAR_ARRAY_DATA_OFFSET")
