# Copyright (C) 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

.class public LMain2;
.super Ljava/lang/Object;


# instance fields
.field b00:Z

.field b01:Z

.field b02:Z

.field b03:Z

.field b04:Z

.field b05:Z

.field b06:Z

.field b07:Z

.field b08:Z

.field b09:Z

.field b10:Z

.field b11:Z

.field b12:Z

.field b13:Z

.field b14:Z

.field b15:Z

.field b16:Z

.field b17:Z

.field b18:Z

.field b19:Z

.field b20:Z

.field b21:Z

.field b22:Z

.field b23:Z

.field b24:Z

.field b25:Z

.field b26:Z

.field b27:Z

.field b28:Z

.field b29:Z

.field b30:Z

.field b31:Z

.field b32:Z

.field b33:Z

.field b34:Z

.field b35:Z

.field b36:Z

.field conditionA:Z

.field conditionB:Z

.field conditionC:Z


# direct methods
.method public constructor <init>()V
    .registers 1

    .prologue
    .line 17
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    return-void
.end method

## CHECK-START-ARM64: void Main2.test() register (after)
## CHECK: begin_block
## CHECK:   name "B0"
## CHECK:       <<This:l\d+>>  ParameterValue
## CHECK: end_block
## CHECK: begin_block
## CHECK:   successors "<<ThenBlock:B\d+>>" "<<ElseBlock:B\d+>>"
## CHECK:       <<CondB:z\d+>>  InstanceFieldGet [<<This>>] field_name:Main2.conditionB
## CHECK:                       If [<<CondB>>]
## CHECK:  end_block
## CHECK: begin_block
## CHECK:   name "<<ElseBlock>>"
## CHECK:                      ParallelMove moves:[40(sp)->d0,24(sp)->32(sp),28(sp)->36(sp),d0->d3,d3->d4,d2->d5,d4->d6,d5->d7,d6->d18,d7->d19,d18->d20,d19->d21,d20->d22,d21->d23,d22->d10,d23->d11,16(sp)->24(sp),20(sp)->28(sp),d10->d14,d11->d12,d12->d13,d13->d1,d14->d2,32(sp)->16(sp),36(sp)->20(sp)]
## CHECK: end_block

## CHECK-START-ARM64: void Main2.test() disassembly (after)
## CHECK: begin_block
## CHECK:   name "B0"
## CHECK:       <<This:l\d+>>  ParameterValue
## CHECK: end_block
## CHECK: begin_block
## CHECK:   successors "<<ThenBlock:B\d+>>" "<<ElseBlock:B\d+>>"
## CHECK:       <<CondB:z\d+>>  InstanceFieldGet [<<This>>] field_name:Main2.conditionB
## CHECK:                       If [<<CondB>>]
## CHECK:  end_block
## CHECK: begin_block
## CHECK:   name "<<ElseBlock>>"
## CHECK:                      ParallelMove moves:[invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid]
## CHECK:                        fmov d31, d2
## CHECK:                        ldr s2, [sp, #36]
## CHECK:                        ldr w16, [sp, #16]
## CHECK:                        str w16, [sp, #36]
## CHECK:                        str s14, [sp, #16]
## CHECK:                        ldr s14, [sp, #28]
## CHECK:                        str s1, [sp, #28]
## CHECK:                        ldr s1, [sp, #32]
## CHECK:                        str s31, [sp, #32]
## CHECK:                        ldr s31, [sp, #20]
## CHECK:                        str s31, [sp, #40]
## CHECK:                        str s12, [sp, #20]
## CHECK:                        fmov d12, d11
## CHECK:                        fmov d11, d10
## CHECK:                        fmov d10, d23
## CHECK:                        fmov d23, d22
## CHECK:                        fmov d22, d21
## CHECK:                        fmov d21, d20
## CHECK:                        fmov d20, d19
## CHECK:                        fmov d19, d18
## CHECK:                        fmov d18, d7
## CHECK:                        fmov d7, d6
## CHECK:                        fmov d6, d5
## CHECK:                        fmov d5, d4
## CHECK:                        fmov d4, d3
## CHECK:                        fmov d3, d13
## CHECK:                        ldr s13, [sp, #24]
## CHECK:                        str s3, [sp, #24]
## CHECK:                        ldr s3, pc+{{\d+}} (addr {{0x[0-9a-f]+}}) (100)
## CHECK: end_block

# Original java source:
#
#     public void test() {
#       String r = "";
#
#       // For the purpose of this regression test, the order of
#       // definition of these float variable matters.  Likewise with the
#       // order of the instructions where these variables are used below.
#       // Reordering these lines make make the original (b/32545705)
#       // issue vanish.
#       float f17 = b17 ? 0.0f : 1.0f;
#       float f16 = b16 ? 0.0f : 1.0f;
#       float f18 = b18 ? 0.0f : 1.0f;
#       float f19 = b19 ? 0.0f : 1.0f;
#       float f20 = b20 ? 0.0f : 1.0f;
#       float f21 = b21 ? 0.0f : 1.0f;
#       float f15 = b15 ? 0.0f : 1.0f;
#       float f00 = b00 ? 0.0f : 1.0f;
#       float f22 = b22 ? 0.0f : 1.0f;
#       float f23 = b23 ? 0.0f : 1.0f;
#       float f24 = b24 ? 0.0f : 1.0f;
#       float f25 = b25 ? 0.0f : 1.0f;
#       float f26 = b26 ? 0.0f : 1.0f;
#       float f27 = b27 ? 0.0f : 1.0f;
#       float f29 = b29 ? 0.0f : 1.0f;
#       float f28 = b28 ? 0.0f : 1.0f;
#       float f01 = b01 ? 0.0f : 1.0f;
#       float f02 = b02 ? 0.0f : 1.0f;
#       float f03 = b03 ? 0.0f : 1.0f;
#       float f04 = b04 ? 0.0f : 1.0f;
#       float f05 = b05 ? 0.0f : 1.0f;
#       float f07 = b07 ? 0.0f : 1.0f;
#       float f06 = b06 ? 0.0f : 1.0f;
#       float f30 = b30 ? 0.0f : 1.0f;
#       float f31 = b31 ? 0.0f : 1.0f;
#       float f32 = b32 ? 0.0f : 1.0f;
#       float f33 = b33 ? 0.0f : 1.0f;
#       float f34 = b34 ? 0.0f : 1.0f;
#       float f36 = b36 ? 0.0f : 1.0f;
#       float f35 = b35 ? 0.0f : 1.0f;
#       float f08 = b08 ? 0.0f : 1.0f;
#       float f09 = b09 ? 0.0f : 1.0f;
#       float f10 = b10 ? 0.0f : 1.0f;
#       float f11 = b11 ? 0.0f : 1.0f;
#       float f12 = b12 ? 0.0f : 1.0f;
#       float f14 = b14 ? 0.0f : 1.0f;
#       float f13 = b13 ? 0.0f : 1.0f;
#
#       if (conditionA) {
#         f16 /= 1000.0f;
#         f17 /= 1000.0f;
#         f18 /= 1000.0f;
#         f19 /= 1000.0f;
#         f20 /= 1000.0f;
#         f21 /= 1000.0f;
#         f15 /= 1000.0f;
#         f08 /= 1000.0f;
#         f09 /= 1000.0f;
#         f10 /= 1000.0f;
#         f11 /= 1000.0f;
#         f12 /= 1000.0f;
#         f30 /= 1000.0f;
#         f31 /= 1000.0f;
#         f32 /= 1000.0f;
#         f33 /= 1000.0f;
#         f34 /= 1000.0f;
#         f01 /= 1000.0f;
#         f02 /= 1000.0f;
#         f03 /= 1000.0f;
#         f04 /= 1000.0f;
#         f05 /= 1000.0f;
#         f23 /= 1000.0f;
#         f24 /= 1000.0f;
#         f25 /= 1000.0f;
#         f26 /= 1000.0f;
#         f27 /= 1000.0f;
#         f22 /= 1000.0f;
#         f00 /= 1000.0f;
#         f14 /= 1000.0f;
#         f13 /= 1000.0f;
#         f36 /= 1000.0f;
#         f35 /= 1000.0f;
#         f07 /= 1000.0f;
#         f06 /= 1000.0f;
#         f29 /= 1000.0f;
#         f28 /= 1000.0f;
#       }
#       // The parallel move that used to exhaust the ARM64 parallel move
#       // resolver's scratch register pool (provided by VIXL) was in the
#       // "else" branch of the following condition generated by ART's
#       // compiler.
#       if (conditionB) {
#         f16 /= 100.0f;
#         f17 /= 100.0f;
#         f18 /= 100.0f;
#         f19 /= 100.0f;
#         f20 /= 100.0f;
#         f21 /= 100.0f;
#         f15 /= 100.0f;
#         f08 /= 100.0f;
#         f09 /= 100.0f;
#         f10 /= 100.0f;
#         f11 /= 100.0f;
#         f12 /= 100.0f;
#         f30 /= 100.0f;
#         f31 /= 100.0f;
#         f32 /= 100.0f;
#         f33 /= 100.0f;
#         f34 /= 100.0f;
#         f01 /= 100.0f;
#         f02 /= 100.0f;
#         f03 /= 100.0f;
#         f04 /= 100.0f;
#         f05 /= 100.0f;
#         f23 /= 100.0f;
#         f24 /= 100.0f;
#         f25 /= 100.0f;
#         f26 /= 100.0f;
#         f27 /= 100.0f;
#         f22 /= 100.0f;
#         f00 /= 100.0f;
#         f14 /= 100.0f;
#         f13 /= 100.0f;
#         f36 /= 100.0f;
#         f35 /= 100.0f;
#         f07 /= 100.0f;
#         f06 /= 100.0f;
#         f29 /= 100.0f;
#         f28 /= 100.0f;
#       }
#       if (conditionC) {
#         f16 /= 12.0f;
#         f17 /= 12.0f;
#         f18 /= 12.0f;
#         f19 /= 12.0f;
#         f20 /= 12.0f;
#         f21 /= 12.0f;
#         f15 /= 12.0f;
#         f08 /= 12.0f;
#         f09 /= 12.0f;
#         f10 /= 12.0f;
#         f11 /= 12.0f;
#         f12 /= 12.0f;
#         f30 /= 12.0f;
#         f31 /= 12.0f;
#         f32 /= 12.0f;
#         f33 /= 12.0f;
#         f34 /= 12.0f;
#         f01 /= 12.0f;
#         f02 /= 12.0f;
#         f03 /= 12.0f;
#         f04 /= 12.0f;
#         f05 /= 12.0f;
#         f23 /= 12.0f;
#         f24 /= 12.0f;
#         f25 /= 12.0f;
#         f26 /= 12.0f;
#         f27 /= 12.0f;
#         f22 /= 12.0f;
#         f00 /= 12.0f;
#         f14 /= 12.0f;
#         f13 /= 12.0f;
#         f36 /= 12.0f;
#         f35 /= 12.0f;
#         f07 /= 12.0f;
#         f06 /= 12.0f;
#         f29 /= 12.0f;
#         f28 /= 12.0f;
#       }
#       float s = 0.0f;
#       s = ((float) Math.round(100.0f * s)) / 100.0f;
#       String res = s + r;
#     }

# virtual methods
.method public test()V
    .registers 45

    .prologue
    .line 121
    const-string v39, ""

    .line 128
    .local v39, "r":Ljava/lang/String;
    move-object/from16 v0, p0

    iget-boolean v0, v0, LMain2;->b17:Z

    move/from16 v42, v0

    if-eqz v42, :cond_367

    const/16 v19, 0x0

    .line 129
    .local v19, "f17":F
    :goto_c
    move-object/from16 v0, p0

    iget-boolean v0, v0, LMain2;->b16:Z

    move/from16 v42, v0

    if-eqz v42, :cond_36b

    const/16 v18, 0x0

    .line 130
    .local v18, "f16":F
    :goto_16
    move-object/from16 v0, p0

    iget-boolean v0, v0, LMain2;->b18:Z

    move/from16 v42, v0

    if-eqz v42, :cond_36f

    const/16 v20, 0x0

    .line 131
    .local v20, "f18":F
    :goto_20
    move-object/from16 v0, p0

    iget-boolean v0, v0, LMain2;->b19:Z

    move/from16 v42, v0

    if-eqz v42, :cond_373

    const/16 v21, 0x0

    .line 132
    .local v21, "f19":F
    :goto_2a
    move-object/from16 v0, p0

    iget-boolean v0, v0, LMain2;->b20:Z

    move/from16 v42, v0

    if-eqz v42, :cond_377

    const/16 v22, 0x0

    .line 133
    .local v22, "f20":F
    :goto_34
    move-object/from16 v0, p0

    iget-boolean v0, v0, LMain2;->b21:Z

    move/from16 v42, v0

    if-eqz v42, :cond_37b

    const/16 v23, 0x0

    .line 134
    .local v23, "f21":F
    :goto_3e
    move-object/from16 v0, p0

    iget-boolean v0, v0, LMain2;->b15:Z

    move/from16 v42, v0

    if-eqz v42, :cond_37f

    const/16 v17, 0x0

    .line 135
    .local v17, "f15":F
    :goto_48
    move-object/from16 v0, p0

    iget-boolean v0, v0, LMain2;->b00:Z

    move/from16 v42, v0

    if-eqz v42, :cond_383

    const/4 v2, 0x0

    .line 136
    .local v2, "f00":F
    :goto_51
    move-object/from16 v0, p0

    iget-boolean v0, v0, LMain2;->b22:Z

    move/from16 v42, v0

    if-eqz v42, :cond_387

    const/16 v24, 0x0

    .line 137
    .local v24, "f22":F
    :goto_5b
    move-object/from16 v0, p0

    iget-boolean v0, v0, LMain2;->b23:Z

    move/from16 v42, v0

    if-eqz v42, :cond_38b

    const/16 v25, 0x0

    .line 138
    .local v25, "f23":F
    :goto_65
    move-object/from16 v0, p0

    iget-boolean v0, v0, LMain2;->b24:Z

    move/from16 v42, v0

    if-eqz v42, :cond_38f

    const/16 v26, 0x0

    .line 139
    .local v26, "f24":F
    :goto_6f
    move-object/from16 v0, p0

    iget-boolean v0, v0, LMain2;->b25:Z

    move/from16 v42, v0

    if-eqz v42, :cond_393

    const/16 v27, 0x0

    .line 140
    .local v27, "f25":F
    :goto_79
    move-object/from16 v0, p0

    iget-boolean v0, v0, LMain2;->b26:Z

    move/from16 v42, v0

    if-eqz v42, :cond_397

    const/16 v28, 0x0

    .line 141
    .local v28, "f26":F
    :goto_83
    move-object/from16 v0, p0

    iget-boolean v0, v0, LMain2;->b27:Z

    move/from16 v42, v0

    if-eqz v42, :cond_39b

    const/16 v29, 0x0

    .line 142
    .local v29, "f27":F
    :goto_8d
    move-object/from16 v0, p0

    iget-boolean v0, v0, LMain2;->b29:Z

    move/from16 v42, v0

    if-eqz v42, :cond_39f

    const/16 v31, 0x0

    .line 143
    .local v31, "f29":F
    :goto_97
    move-object/from16 v0, p0

    iget-boolean v0, v0, LMain2;->b28:Z

    move/from16 v42, v0

    if-eqz v42, :cond_3a3

    const/16 v30, 0x0

    .line 144
    .local v30, "f28":F
    :goto_a1
    move-object/from16 v0, p0

    iget-boolean v0, v0, LMain2;->b01:Z

    move/from16 v42, v0

    if-eqz v42, :cond_3a7

    const/4 v3, 0x0

    .line 145
    .local v3, "f01":F
    :goto_aa
    move-object/from16 v0, p0

    iget-boolean v0, v0, LMain2;->b02:Z

    move/from16 v42, v0

    if-eqz v42, :cond_3ab

    const/4 v4, 0x0

    .line 146
    .local v4, "f02":F
    :goto_b3
    move-object/from16 v0, p0

    iget-boolean v0, v0, LMain2;->b03:Z

    move/from16 v42, v0

    if-eqz v42, :cond_3af

    const/4 v5, 0x0

    .line 147
    .local v5, "f03":F
    :goto_bc
    move-object/from16 v0, p0

    iget-boolean v0, v0, LMain2;->b04:Z

    move/from16 v42, v0

    if-eqz v42, :cond_3b3

    const/4 v6, 0x0

    .line 148
    .local v6, "f04":F
    :goto_c5
    move-object/from16 v0, p0

    iget-boolean v0, v0, LMain2;->b05:Z

    move/from16 v42, v0

    if-eqz v42, :cond_3b7

    const/4 v7, 0x0

    .line 149
    .local v7, "f05":F
    :goto_ce
    move-object/from16 v0, p0

    iget-boolean v0, v0, LMain2;->b07:Z

    move/from16 v42, v0

    if-eqz v42, :cond_3bb

    const/4 v9, 0x0

    .line 150
    .local v9, "f07":F
    :goto_d7
    move-object/from16 v0, p0

    iget-boolean v0, v0, LMain2;->b06:Z

    move/from16 v42, v0

    if-eqz v42, :cond_3bf

    const/4 v8, 0x0

    .line 151
    .local v8, "f06":F
    :goto_e0
    move-object/from16 v0, p0

    iget-boolean v0, v0, LMain2;->b30:Z

    move/from16 v42, v0

    if-eqz v42, :cond_3c3

    const/16 v32, 0x0

    .line 152
    .local v32, "f30":F
    :goto_ea
    move-object/from16 v0, p0

    iget-boolean v0, v0, LMain2;->b31:Z

    move/from16 v42, v0

    if-eqz v42, :cond_3c7

    const/16 v33, 0x0

    .line 153
    .local v33, "f31":F
    :goto_f4
    move-object/from16 v0, p0

    iget-boolean v0, v0, LMain2;->b32:Z

    move/from16 v42, v0

    if-eqz v42, :cond_3cb

    const/16 v34, 0x0

    .line 154
    .local v34, "f32":F
    :goto_fe
    move-object/from16 v0, p0

    iget-boolean v0, v0, LMain2;->b33:Z

    move/from16 v42, v0

    if-eqz v42, :cond_3cf

    const/16 v35, 0x0

    .line 155
    .local v35, "f33":F
    :goto_108
    move-object/from16 v0, p0

    iget-boolean v0, v0, LMain2;->b34:Z

    move/from16 v42, v0

    if-eqz v42, :cond_3d3

    const/16 v36, 0x0

    .line 156
    .local v36, "f34":F
    :goto_112
    move-object/from16 v0, p0

    iget-boolean v0, v0, LMain2;->b36:Z

    move/from16 v42, v0

    if-eqz v42, :cond_3d7

    const/16 v38, 0x0

    .line 157
    .local v38, "f36":F
    :goto_11c
    move-object/from16 v0, p0

    iget-boolean v0, v0, LMain2;->b35:Z

    move/from16 v42, v0

    if-eqz v42, :cond_3db

    const/16 v37, 0x0

    .line 158
    .local v37, "f35":F
    :goto_126
    move-object/from16 v0, p0

    iget-boolean v0, v0, LMain2;->b08:Z

    move/from16 v42, v0

    if-eqz v42, :cond_3df

    const/4 v10, 0x0

    .line 159
    .local v10, "f08":F
    :goto_12f
    move-object/from16 v0, p0

    iget-boolean v0, v0, LMain2;->b09:Z

    move/from16 v42, v0

    if-eqz v42, :cond_3e3

    const/4 v11, 0x0

    .line 160
    .local v11, "f09":F
    :goto_138
    move-object/from16 v0, p0

    iget-boolean v0, v0, LMain2;->b10:Z

    move/from16 v42, v0

    if-eqz v42, :cond_3e7

    const/4 v12, 0x0

    .line 161
    .local v12, "f10":F
    :goto_141
    move-object/from16 v0, p0

    iget-boolean v0, v0, LMain2;->b11:Z

    move/from16 v42, v0

    if-eqz v42, :cond_3eb

    const/4 v13, 0x0

    .line 162
    .local v13, "f11":F
    :goto_14a
    move-object/from16 v0, p0

    iget-boolean v0, v0, LMain2;->b12:Z

    move/from16 v42, v0

    if-eqz v42, :cond_3ef

    const/4 v14, 0x0

    .line 163
    .local v14, "f12":F
    :goto_153
    move-object/from16 v0, p0

    iget-boolean v0, v0, LMain2;->b14:Z

    move/from16 v42, v0

    if-eqz v42, :cond_3f3

    const/16 v16, 0x0

    .line 164
    .local v16, "f14":F
    :goto_15d
    move-object/from16 v0, p0

    iget-boolean v0, v0, LMain2;->b13:Z

    move/from16 v42, v0

    if-eqz v42, :cond_3f7

    const/4 v15, 0x0

    .line 166
    .local v15, "f13":F
    :goto_166
    move-object/from16 v0, p0

    iget-boolean v0, v0, LMain2;->conditionA:Z

    move/from16 v42, v0

    if-eqz v42, :cond_202

    .line 167
    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v18, v18, v42

    .line 168
    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v19, v19, v42

    .line 169
    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v20, v20, v42

    .line 170
    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v21, v21, v42

    .line 171
    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v22, v22, v42

    .line 172
    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v23, v23, v42

    .line 173
    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v17, v17, v42

    .line 174
    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v10, v10, v42

    .line 175
    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v11, v11, v42

    .line 176
    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v12, v12, v42

    .line 177
    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v13, v13, v42

    .line 178
    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v14, v14, v42

    .line 179
    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v32, v32, v42

    .line 180
    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v33, v33, v42

    .line 181
    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v34, v34, v42

    .line 182
    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v35, v35, v42

    .line 183
    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v36, v36, v42

    .line 184
    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v3, v3, v42

    .line 185
    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v4, v4, v42

    .line 186
    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v5, v5, v42

    .line 187
    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v6, v6, v42

    .line 188
    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v7, v7, v42

    .line 189
    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v25, v25, v42

    .line 190
    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v26, v26, v42

    .line 191
    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v27, v27, v42

    .line 192
    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v28, v28, v42

    .line 193
    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v29, v29, v42

    .line 194
    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v24, v24, v42

    .line 195
    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v2, v2, v42

    .line 196
    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v16, v16, v42

    .line 197
    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v15, v15, v42

    .line 198
    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v38, v38, v42

    .line 199
    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v37, v37, v42

    .line 200
    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v9, v9, v42

    .line 201
    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v8, v8, v42

    .line 202
    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v31, v31, v42

    .line 203
    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v30, v30, v42

    .line 209
    :cond_202
    move-object/from16 v0, p0

    iget-boolean v0, v0, LMain2;->conditionB:Z

    move/from16 v42, v0

    if-eqz v42, :cond_29e

    .line 210
    const/high16 v42, 0x42c80000    # 100.0f

    div-float v18, v18, v42

    .line 211
    const/high16 v42, 0x42c80000    # 100.0f

    div-float v19, v19, v42

    .line 212
    const/high16 v42, 0x42c80000    # 100.0f

    div-float v20, v20, v42

    .line 213
    const/high16 v42, 0x42c80000    # 100.0f

    div-float v21, v21, v42

    .line 214
    const/high16 v42, 0x42c80000    # 100.0f

    div-float v22, v22, v42

    .line 215
    const/high16 v42, 0x42c80000    # 100.0f

    div-float v23, v23, v42

    .line 216
    const/high16 v42, 0x42c80000    # 100.0f

    div-float v17, v17, v42

    .line 217
    const/high16 v42, 0x42c80000    # 100.0f

    div-float v10, v10, v42

    .line 218
    const/high16 v42, 0x42c80000    # 100.0f

    div-float v11, v11, v42

    .line 219
    const/high16 v42, 0x42c80000    # 100.0f

    div-float v12, v12, v42

    .line 220
    const/high16 v42, 0x42c80000    # 100.0f

    div-float v13, v13, v42

    .line 221
    const/high16 v42, 0x42c80000    # 100.0f

    div-float v14, v14, v42

    .line 222
    const/high16 v42, 0x42c80000    # 100.0f

    div-float v32, v32, v42

    .line 223
    const/high16 v42, 0x42c80000    # 100.0f

    div-float v33, v33, v42

    .line 224
    const/high16 v42, 0x42c80000    # 100.0f

    div-float v34, v34, v42

    .line 225
    const/high16 v42, 0x42c80000    # 100.0f

    div-float v35, v35, v42

    .line 226
    const/high16 v42, 0x42c80000    # 100.0f

    div-float v36, v36, v42

    .line 227
    const/high16 v42, 0x42c80000    # 100.0f

    div-float v3, v3, v42

    .line 228
    const/high16 v42, 0x42c80000    # 100.0f

    div-float v4, v4, v42

    .line 229
    const/high16 v42, 0x42c80000    # 100.0f

    div-float v5, v5, v42

    .line 230
    const/high16 v42, 0x42c80000    # 100.0f

    div-float v6, v6, v42

    .line 231
    const/high16 v42, 0x42c80000    # 100.0f

    div-float v7, v7, v42

    .line 232
    const/high16 v42, 0x42c80000    # 100.0f

    div-float v25, v25, v42

    .line 233
    const/high16 v42, 0x42c80000    # 100.0f

    div-float v26, v26, v42

    .line 234
    const/high16 v42, 0x42c80000    # 100.0f

    div-float v27, v27, v42

    .line 235
    const/high16 v42, 0x42c80000    # 100.0f

    div-float v28, v28, v42

    .line 236
    const/high16 v42, 0x42c80000    # 100.0f

    div-float v29, v29, v42

    .line 237
    const/high16 v42, 0x42c80000    # 100.0f

    div-float v24, v24, v42

    .line 238
    const/high16 v42, 0x42c80000    # 100.0f

    div-float v2, v2, v42

    .line 239
    const/high16 v42, 0x42c80000    # 100.0f

    div-float v16, v16, v42

    .line 240
    const/high16 v42, 0x42c80000    # 100.0f

    div-float v15, v15, v42

    .line 241
    const/high16 v42, 0x42c80000    # 100.0f

    div-float v38, v38, v42

    .line 242
    const/high16 v42, 0x42c80000    # 100.0f

    div-float v37, v37, v42

    .line 243
    const/high16 v42, 0x42c80000    # 100.0f

    div-float v9, v9, v42

    .line 244
    const/high16 v42, 0x42c80000    # 100.0f

    div-float v8, v8, v42

    .line 245
    const/high16 v42, 0x42c80000    # 100.0f

    div-float v31, v31, v42

    .line 246
    const/high16 v42, 0x42c80000    # 100.0f

    div-float v30, v30, v42

    .line 248
    :cond_29e
    move-object/from16 v0, p0

    iget-boolean v0, v0, LMain2;->conditionC:Z

    move/from16 v42, v0

    if-eqz v42, :cond_33a

    .line 249
    const/high16 v42, 0x41400000    # 12.0f

    div-float v18, v18, v42

    .line 250
    const/high16 v42, 0x41400000    # 12.0f

    div-float v19, v19, v42

    .line 251
    const/high16 v42, 0x41400000    # 12.0f

    div-float v20, v20, v42

    .line 252
    const/high16 v42, 0x41400000    # 12.0f

    div-float v21, v21, v42

    .line 253
    const/high16 v42, 0x41400000    # 12.0f

    div-float v22, v22, v42

    .line 254
    const/high16 v42, 0x41400000    # 12.0f

    div-float v23, v23, v42

    .line 255
    const/high16 v42, 0x41400000    # 12.0f

    div-float v17, v17, v42

    .line 256
    const/high16 v42, 0x41400000    # 12.0f

    div-float v10, v10, v42

    .line 257
    const/high16 v42, 0x41400000    # 12.0f

    div-float v11, v11, v42

    .line 258
    const/high16 v42, 0x41400000    # 12.0f

    div-float v12, v12, v42

    .line 259
    const/high16 v42, 0x41400000    # 12.0f

    div-float v13, v13, v42

    .line 260
    const/high16 v42, 0x41400000    # 12.0f

    div-float v14, v14, v42

    .line 261
    const/high16 v42, 0x41400000    # 12.0f

    div-float v32, v32, v42

    .line 262
    const/high16 v42, 0x41400000    # 12.0f

    div-float v33, v33, v42

    .line 263
    const/high16 v42, 0x41400000    # 12.0f

    div-float v34, v34, v42

    .line 264
    const/high16 v42, 0x41400000    # 12.0f

    div-float v35, v35, v42

    .line 265
    const/high16 v42, 0x41400000    # 12.0f

    div-float v36, v36, v42

    .line 266
    const/high16 v42, 0x41400000    # 12.0f

    div-float v3, v3, v42

    .line 267
    const/high16 v42, 0x41400000    # 12.0f

    div-float v4, v4, v42

    .line 268
    const/high16 v42, 0x41400000    # 12.0f

    div-float v5, v5, v42

    .line 269
    const/high16 v42, 0x41400000    # 12.0f

    div-float v6, v6, v42

    .line 270
    const/high16 v42, 0x41400000    # 12.0f

    div-float v7, v7, v42

    .line 271
    const/high16 v42, 0x41400000    # 12.0f

    div-float v25, v25, v42

    .line 272
    const/high16 v42, 0x41400000    # 12.0f

    div-float v26, v26, v42

    .line 273
    const/high16 v42, 0x41400000    # 12.0f

    div-float v27, v27, v42

    .line 274
    const/high16 v42, 0x41400000    # 12.0f

    div-float v28, v28, v42

    .line 275
    const/high16 v42, 0x41400000    # 12.0f

    div-float v29, v29, v42

    .line 276
    const/high16 v42, 0x41400000    # 12.0f

    div-float v24, v24, v42

    .line 277
    const/high16 v42, 0x41400000    # 12.0f

    div-float v2, v2, v42

    .line 278
    const/high16 v42, 0x41400000    # 12.0f

    div-float v16, v16, v42

    .line 279
    const/high16 v42, 0x41400000    # 12.0f

    div-float v15, v15, v42

    .line 280
    const/high16 v42, 0x41400000    # 12.0f

    div-float v38, v38, v42

    .line 281
    const/high16 v42, 0x41400000    # 12.0f

    div-float v37, v37, v42

    .line 282
    const/high16 v42, 0x41400000    # 12.0f

    div-float v9, v9, v42

    .line 283
    const/high16 v42, 0x41400000    # 12.0f

    div-float v8, v8, v42

    .line 284
    const/high16 v42, 0x41400000    # 12.0f

    div-float v31, v31, v42

    .line 285
    const/high16 v42, 0x41400000    # 12.0f

    div-float v30, v30, v42

    .line 287
    :cond_33a
    const/16 v41, 0x0

    .line 288
    .local v41, "s":F
    const/high16 v42, 0x42c80000    # 100.0f

    mul-float v42, v42, v41

    invoke-static/range {v42 .. v42}, Ljava/lang/Math;->round(F)I

    move-result v42

    move/from16 v0, v42

    int-to-float v0, v0

    move/from16 v42, v0

    const/high16 v43, 0x42c80000    # 100.0f

    div-float v41, v42, v43

    .line 289
    new-instance v42, Ljava/lang/StringBuilder;

    invoke-direct/range {v42 .. v42}, Ljava/lang/StringBuilder;-><init>()V

    move-object/from16 v0, v42

    move/from16 v1, v41

    invoke-virtual {v0, v1}, Ljava/lang/StringBuilder;->append(F)Ljava/lang/StringBuilder;

    move-result-object v42

    move-object/from16 v0, v42

    move-object/from16 v1, v39

    invoke-virtual {v0, v1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v42

    invoke-virtual/range {v42 .. v42}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    move-result-object v40

    .line 290
    .local v40, "res":Ljava/lang/String;
    return-void

    .line 128
    .end local v2    # "f00":F
    .end local v3    # "f01":F
    .end local v4    # "f02":F
    .end local v5    # "f03":F
    .end local v6    # "f04":F
    .end local v7    # "f05":F
    .end local v8    # "f06":F
    .end local v9    # "f07":F
    .end local v10    # "f08":F
    .end local v11    # "f09":F
    .end local v12    # "f10":F
    .end local v13    # "f11":F
    .end local v14    # "f12":F
    .end local v15    # "f13":F
    .end local v16    # "f14":F
    .end local v17    # "f15":F
    .end local v18    # "f16":F
    .end local v19    # "f17":F
    .end local v20    # "f18":F
    .end local v21    # "f19":F
    .end local v22    # "f20":F
    .end local v23    # "f21":F
    .end local v24    # "f22":F
    .end local v25    # "f23":F
    .end local v26    # "f24":F
    .end local v27    # "f25":F
    .end local v28    # "f26":F
    .end local v29    # "f27":F
    .end local v30    # "f28":F
    .end local v31    # "f29":F
    .end local v32    # "f30":F
    .end local v33    # "f31":F
    .end local v34    # "f32":F
    .end local v35    # "f33":F
    .end local v36    # "f34":F
    .end local v37    # "f35":F
    .end local v38    # "f36":F
    .end local v40    # "res":Ljava/lang/String;
    .end local v41    # "s":F
    :cond_367
    const/high16 v19, 0x3f800000    # 1.0f

    goto/16 :goto_c

    .line 129
    .restart local v19    # "f17":F
    :cond_36b
    const/high16 v18, 0x3f800000    # 1.0f

    goto/16 :goto_16

    .line 130
    .restart local v18    # "f16":F
    :cond_36f
    const/high16 v20, 0x3f800000    # 1.0f

    goto/16 :goto_20

    .line 131
    .restart local v20    # "f18":F
    :cond_373
    const/high16 v21, 0x3f800000    # 1.0f

    goto/16 :goto_2a

    .line 132
    .restart local v21    # "f19":F
    :cond_377
    const/high16 v22, 0x3f800000    # 1.0f

    goto/16 :goto_34

    .line 133
    .restart local v22    # "f20":F
    :cond_37b
    const/high16 v23, 0x3f800000    # 1.0f

    goto/16 :goto_3e

    .line 134
    .restart local v23    # "f21":F
    :cond_37f
    const/high16 v17, 0x3f800000    # 1.0f

    goto/16 :goto_48

    .line 135
    .restart local v17    # "f15":F
    :cond_383
    const/high16 v2, 0x3f800000    # 1.0f

    goto/16 :goto_51

    .line 136
    .restart local v2    # "f00":F
    :cond_387
    const/high16 v24, 0x3f800000    # 1.0f

    goto/16 :goto_5b

    .line 137
    .restart local v24    # "f22":F
    :cond_38b
    const/high16 v25, 0x3f800000    # 1.0f

    goto/16 :goto_65

    .line 138
    .restart local v25    # "f23":F
    :cond_38f
    const/high16 v26, 0x3f800000    # 1.0f

    goto/16 :goto_6f

    .line 139
    .restart local v26    # "f24":F
    :cond_393
    const/high16 v27, 0x3f800000    # 1.0f

    goto/16 :goto_79

    .line 140
    .restart local v27    # "f25":F
    :cond_397
    const/high16 v28, 0x3f800000    # 1.0f

    goto/16 :goto_83

    .line 141
    .restart local v28    # "f26":F
    :cond_39b
    const/high16 v29, 0x3f800000    # 1.0f

    goto/16 :goto_8d

    .line 142
    .restart local v29    # "f27":F
    :cond_39f
    const/high16 v31, 0x3f800000    # 1.0f

    goto/16 :goto_97

    .line 143
    .restart local v31    # "f29":F
    :cond_3a3
    const/high16 v30, 0x3f800000    # 1.0f

    goto/16 :goto_a1

    .line 144
    .restart local v30    # "f28":F
    :cond_3a7
    const/high16 v3, 0x3f800000    # 1.0f

    goto/16 :goto_aa

    .line 145
    .restart local v3    # "f01":F
    :cond_3ab
    const/high16 v4, 0x3f800000    # 1.0f

    goto/16 :goto_b3

    .line 146
    .restart local v4    # "f02":F
    :cond_3af
    const/high16 v5, 0x3f800000    # 1.0f

    goto/16 :goto_bc

    .line 147
    .restart local v5    # "f03":F
    :cond_3b3
    const/high16 v6, 0x3f800000    # 1.0f

    goto/16 :goto_c5

    .line 148
    .restart local v6    # "f04":F
    :cond_3b7
    const/high16 v7, 0x3f800000    # 1.0f

    goto/16 :goto_ce

    .line 149
    .restart local v7    # "f05":F
    :cond_3bb
    const/high16 v9, 0x3f800000    # 1.0f

    goto/16 :goto_d7

    .line 150
    .restart local v9    # "f07":F
    :cond_3bf
    const/high16 v8, 0x3f800000    # 1.0f

    goto/16 :goto_e0

    .line 151
    .restart local v8    # "f06":F
    :cond_3c3
    const/high16 v32, 0x3f800000    # 1.0f

    goto/16 :goto_ea

    .line 152
    .restart local v32    # "f30":F
    :cond_3c7
    const/high16 v33, 0x3f800000    # 1.0f

    goto/16 :goto_f4

    .line 153
    .restart local v33    # "f31":F
    :cond_3cb
    const/high16 v34, 0x3f800000    # 1.0f

    goto/16 :goto_fe

    .line 154
    .restart local v34    # "f32":F
    :cond_3cf
    const/high16 v35, 0x3f800000    # 1.0f

    goto/16 :goto_108

    .line 155
    .restart local v35    # "f33":F
    :cond_3d3
    const/high16 v36, 0x3f800000    # 1.0f

    goto/16 :goto_112

    .line 156
    .restart local v36    # "f34":F
    :cond_3d7
    const/high16 v38, 0x3f800000    # 1.0f

    goto/16 :goto_11c

    .line 157
    .restart local v38    # "f36":F
    :cond_3db
    const/high16 v37, 0x3f800000    # 1.0f

    goto/16 :goto_126

    .line 158
    .restart local v37    # "f35":F
    :cond_3df
    const/high16 v10, 0x3f800000    # 1.0f

    goto/16 :goto_12f

    .line 159
    .restart local v10    # "f08":F
    :cond_3e3
    const/high16 v11, 0x3f800000    # 1.0f

    goto/16 :goto_138

    .line 160
    .restart local v11    # "f09":F
    :cond_3e7
    const/high16 v12, 0x3f800000    # 1.0f

    goto/16 :goto_141

    .line 161
    .restart local v12    # "f10":F
    :cond_3eb
    const/high16 v13, 0x3f800000    # 1.0f

    goto/16 :goto_14a

    .line 162
    .restart local v13    # "f11":F
    :cond_3ef
    const/high16 v14, 0x3f800000    # 1.0f

    goto/16 :goto_153

    .line 163
    .restart local v14    # "f12":F
    :cond_3f3
    const/high16 v16, 0x3f800000    # 1.0f

    goto/16 :goto_15d

    .line 164
    .restart local v16    # "f14":F
    :cond_3f7
    const/high16 v15, 0x3f800000    # 1.0f

    goto/16 :goto_166
.end method
