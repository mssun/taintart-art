%def op_aget_boolean():
%  op_aget(load="movzbl", shift="1", data_offset="MIRROR_BOOLEAN_ARRAY_DATA_OFFSET")
