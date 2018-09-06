%def op_aget_byte():
%  op_aget(load="movsbl", shift="1", data_offset="MIRROR_BYTE_ARRAY_DATA_OFFSET")
