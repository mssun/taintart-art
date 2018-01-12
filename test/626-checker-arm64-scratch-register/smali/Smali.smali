# Copyright (C) 2017 The Android Open Source Project
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

.class public LSmali;
.super Ljava/lang/Object;
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

.method public constructor <init>()V
    .registers 1
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V
    return-void
.end method

##  CHECK-START-ARM64: void Smali.test() register (after)
##  CHECK: begin_block
##  CHECK:   name "B0"
##  CHECK:       <<This:l\d+>>  ParameterValue
##  CHECK: end_block
##  CHECK: begin_block
##  CHECK:   successors "<<ThenBlock:B\d+>>" "<<ElseBlock:B\d+>>"
##  CHECK:       <<CondB:z\d+>>  InstanceFieldGet [<<This>>] field_name:Smali.conditionB
##  CHECK:                       If [<<CondB>>]
##  CHECK:  end_block
##  CHECK: begin_block
##  CHECK:   name "<<ElseBlock>>"
##  CHECK:                      ParallelMove moves:[40(sp)->d0,24(sp)->32(sp),28(sp)->36(sp),d0->d3,d3->d4,d2->d5,d4->d6,d5->d7,d6->d18,d7->d19,d18->d20,d19->d21,d20->d22,d21->d23,d22->d10,d23->d11,16(sp)->24(sp),20(sp)->28(sp),d10->d14,d11->d12,d12->d13,d13->d1,d14->d2,32(sp)->16(sp),36(sp)->20(sp)]
##  CHECK: end_block

##  CHECK-START-ARM64: void Smali.test() disassembly (after)
##  CHECK: begin_block
##  CHECK:   name "B0"
##  CHECK:       <<This:l\d+>>  ParameterValue
##  CHECK: end_block
##  CHECK: begin_block
##  CHECK:   successors "<<ThenBlock:B\d+>>" "<<ElseBlock:B\d+>>"
##  CHECK:       <<CondB:z\d+>>  InstanceFieldGet [<<This>>] field_name:Smali.conditionB
##  CHECK:                       If [<<CondB>>]
##  CHECK:  end_block
##  CHECK: begin_block
##  CHECK:   name "<<ElseBlock>>"
##  CHECK:                      ParallelMove moves:[invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid]
##  CHECK:                        fmov d31, d2
##  CHECK:                        ldr s2, [sp, #36]
##  CHECK:                        ldr w16, [sp, #16]
##  CHECK:                        str w16, [sp, #36]
##  CHECK:                        str s14, [sp, #16]
##  CHECK:                        ldr s14, [sp, #28]
##  CHECK:                        str s1, [sp, #28]
##  CHECK:                        ldr s1, [sp, #32]
##  CHECK:                        str s31, [sp, #32]
##  CHECK:                        ldr s31, [sp, #20]
##  CHECK:                        str s31, [sp, #40]
##  CHECK:                        str s12, [sp, #20]
##  CHECK:                        fmov d12, d11
##  CHECK:                        fmov d11, d10
##  CHECK:                        fmov d10, d23
##  CHECK:                        fmov d23, d22
##  CHECK:                        fmov d22, d21
##  CHECK:                        fmov d21, d20
##  CHECK:                        fmov d20, d19
##  CHECK:                        fmov d19, d18
##  CHECK:                        fmov d18, d7
##  CHECK:                        fmov d7, d6
##  CHECK:                        fmov d6, d5
##  CHECK:                        fmov d5, d4
##  CHECK:                        fmov d4, d3
##  CHECK:                        fmov d3, d13
##  CHECK:                        ldr s13, [sp, #24]
##  CHECK:                        str s3, [sp, #24]
##  CHECK:                        ldr s3, pc+{{\d+}} (addr {{0x[0-9a-f]+}}) (100)
##  CHECK: end_block
.method public test()V
    .registers 45

    const-string v39, ""

    move-object/from16 v0, p0

    iget-boolean v0, v0, LSmali;->b17:Z

    move/from16 v42, v0

    if-eqz v42, :cond_367

    const/16 v19, 0x0

    :goto_c
    move-object/from16 v0, p0

    iget-boolean v0, v0, LSmali;->b16:Z

    move/from16 v42, v0

    if-eqz v42, :cond_36b

    const/16 v18, 0x0

    :goto_16
    move-object/from16 v0, p0

    iget-boolean v0, v0, LSmali;->b18:Z

    move/from16 v42, v0

    if-eqz v42, :cond_36f

    const/16 v20, 0x0

    :goto_20
    move-object/from16 v0, p0

    iget-boolean v0, v0, LSmali;->b19:Z

    move/from16 v42, v0

    if-eqz v42, :cond_373

    const/16 v21, 0x0

    :goto_2a
    move-object/from16 v0, p0

    iget-boolean v0, v0, LSmali;->b20:Z

    move/from16 v42, v0

    if-eqz v42, :cond_377

    const/16 v22, 0x0

    :goto_34
    move-object/from16 v0, p0

    iget-boolean v0, v0, LSmali;->b21:Z

    move/from16 v42, v0

    if-eqz v42, :cond_37b

    const/16 v23, 0x0

    :goto_3e
    move-object/from16 v0, p0

    iget-boolean v0, v0, LSmali;->b15:Z

    move/from16 v42, v0

    if-eqz v42, :cond_37f

    const/16 v17, 0x0

    :goto_48
    move-object/from16 v0, p0

    iget-boolean v0, v0, LSmali;->b00:Z

    move/from16 v42, v0

    if-eqz v42, :cond_383

    const/4 v2, 0x0

    :goto_51
    move-object/from16 v0, p0

    iget-boolean v0, v0, LSmali;->b22:Z

    move/from16 v42, v0

    if-eqz v42, :cond_387

    const/16 v24, 0x0

    :goto_5b
    move-object/from16 v0, p0

    iget-boolean v0, v0, LSmali;->b23:Z

    move/from16 v42, v0

    if-eqz v42, :cond_38b

    const/16 v25, 0x0

    :goto_65
    move-object/from16 v0, p0

    iget-boolean v0, v0, LSmali;->b24:Z

    move/from16 v42, v0

    if-eqz v42, :cond_38f

    const/16 v26, 0x0

    :goto_6f
    move-object/from16 v0, p0

    iget-boolean v0, v0, LSmali;->b25:Z

    move/from16 v42, v0

    if-eqz v42, :cond_393

    const/16 v27, 0x0

    :goto_79
    move-object/from16 v0, p0

    iget-boolean v0, v0, LSmali;->b26:Z

    move/from16 v42, v0

    if-eqz v42, :cond_397

    const/16 v28, 0x0

    :goto_83
    move-object/from16 v0, p0

    iget-boolean v0, v0, LSmali;->b27:Z

    move/from16 v42, v0

    if-eqz v42, :cond_39b

    const/16 v29, 0x0

    :goto_8d
    move-object/from16 v0, p0

    iget-boolean v0, v0, LSmali;->b29:Z

    move/from16 v42, v0

    if-eqz v42, :cond_39f

    const/16 v31, 0x0

    :goto_97
    move-object/from16 v0, p0

    iget-boolean v0, v0, LSmali;->b28:Z

    move/from16 v42, v0

    if-eqz v42, :cond_3a3

    const/16 v30, 0x0

    :goto_a1
    move-object/from16 v0, p0

    iget-boolean v0, v0, LSmali;->b01:Z

    move/from16 v42, v0

    if-eqz v42, :cond_3a7

    const/4 v3, 0x0

    :goto_aa
    move-object/from16 v0, p0

    iget-boolean v0, v0, LSmali;->b02:Z

    move/from16 v42, v0

    if-eqz v42, :cond_3ab

    const/4 v4, 0x0

    :goto_b3
    move-object/from16 v0, p0

    iget-boolean v0, v0, LSmali;->b03:Z

    move/from16 v42, v0

    if-eqz v42, :cond_3af

    const/4 v5, 0x0

    :goto_bc
    move-object/from16 v0, p0

    iget-boolean v0, v0, LSmali;->b04:Z

    move/from16 v42, v0

    if-eqz v42, :cond_3b3

    const/4 v6, 0x0

    :goto_c5
    move-object/from16 v0, p0

    iget-boolean v0, v0, LSmali;->b05:Z

    move/from16 v42, v0

    if-eqz v42, :cond_3b7

    const/4 v7, 0x0

    :goto_ce
    move-object/from16 v0, p0

    iget-boolean v0, v0, LSmali;->b07:Z

    move/from16 v42, v0

    if-eqz v42, :cond_3bb

    const/4 v9, 0x0

    :goto_d7
    move-object/from16 v0, p0

    iget-boolean v0, v0, LSmali;->b06:Z

    move/from16 v42, v0

    if-eqz v42, :cond_3bf

    const/4 v8, 0x0

    :goto_e0
    move-object/from16 v0, p0

    iget-boolean v0, v0, LSmali;->b30:Z

    move/from16 v42, v0

    if-eqz v42, :cond_3c3

    const/16 v32, 0x0

    :goto_ea
    move-object/from16 v0, p0

    iget-boolean v0, v0, LSmali;->b31:Z

    move/from16 v42, v0

    if-eqz v42, :cond_3c7

    const/16 v33, 0x0

    :goto_f4
    move-object/from16 v0, p0

    iget-boolean v0, v0, LSmali;->b32:Z

    move/from16 v42, v0

    if-eqz v42, :cond_3cb

    const/16 v34, 0x0

    :goto_fe
    move-object/from16 v0, p0

    iget-boolean v0, v0, LSmali;->b33:Z

    move/from16 v42, v0

    if-eqz v42, :cond_3cf

    const/16 v35, 0x0

    :goto_108
    move-object/from16 v0, p0

    iget-boolean v0, v0, LSmali;->b34:Z

    move/from16 v42, v0

    if-eqz v42, :cond_3d3

    const/16 v36, 0x0

    :goto_112
    move-object/from16 v0, p0

    iget-boolean v0, v0, LSmali;->b36:Z

    move/from16 v42, v0

    if-eqz v42, :cond_3d7

    const/16 v38, 0x0

    :goto_11c
    move-object/from16 v0, p0

    iget-boolean v0, v0, LSmali;->b35:Z

    move/from16 v42, v0

    if-eqz v42, :cond_3db

    const/16 v37, 0x0

    :goto_126
    move-object/from16 v0, p0

    iget-boolean v0, v0, LSmali;->b08:Z

    move/from16 v42, v0

    if-eqz v42, :cond_3df

    const/4 v10, 0x0

    :goto_12f
    move-object/from16 v0, p0

    iget-boolean v0, v0, LSmali;->b09:Z

    move/from16 v42, v0

    if-eqz v42, :cond_3e3

    const/4 v11, 0x0

    :goto_138
    move-object/from16 v0, p0

    iget-boolean v0, v0, LSmali;->b10:Z

    move/from16 v42, v0

    if-eqz v42, :cond_3e7

    const/4 v12, 0x0

    :goto_141
    move-object/from16 v0, p0

    iget-boolean v0, v0, LSmali;->b11:Z

    move/from16 v42, v0

    if-eqz v42, :cond_3eb

    const/4 v13, 0x0

    :goto_14a
    move-object/from16 v0, p0

    iget-boolean v0, v0, LSmali;->b12:Z

    move/from16 v42, v0

    if-eqz v42, :cond_3ef

    const/4 v14, 0x0

    :goto_153
    move-object/from16 v0, p0

    iget-boolean v0, v0, LSmali;->b14:Z

    move/from16 v42, v0

    if-eqz v42, :cond_3f3

    const/16 v16, 0x0

    :goto_15d
    move-object/from16 v0, p0

    iget-boolean v0, v0, LSmali;->b13:Z

    move/from16 v42, v0

    if-eqz v42, :cond_3f7

    const/4 v15, 0x0

    :goto_166
    move-object/from16 v0, p0

    iget-boolean v0, v0, LSmali;->conditionA:Z

    move/from16 v42, v0

    if-eqz v42, :cond_202

    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v18, v18, v42

    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v19, v19, v42

    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v20, v20, v42

    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v21, v21, v42

    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v22, v22, v42

    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v23, v23, v42

    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v17, v17, v42

    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v10, v10, v42

    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v11, v11, v42

    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v12, v12, v42

    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v13, v13, v42

    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v14, v14, v42

    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v32, v32, v42

    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v33, v33, v42

    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v34, v34, v42

    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v35, v35, v42

    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v36, v36, v42

    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v3, v3, v42

    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v4, v4, v42

    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v5, v5, v42

    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v6, v6, v42

    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v7, v7, v42

    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v25, v25, v42

    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v26, v26, v42

    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v27, v27, v42

    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v28, v28, v42

    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v29, v29, v42

    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v24, v24, v42

    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v2, v2, v42

    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v16, v16, v42

    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v15, v15, v42

    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v38, v38, v42

    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v37, v37, v42

    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v9, v9, v42

    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v8, v8, v42

    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v31, v31, v42

    const/high16 v42, 0x447a0000    # 1000.0f

    div-float v30, v30, v42

    :cond_202
    move-object/from16 v0, p0

    iget-boolean v0, v0, LSmali;->conditionB:Z

    move/from16 v42, v0

    if-eqz v42, :cond_29e

    const/high16 v42, 0x42c80000    # 100.0f

    div-float v18, v18, v42

    const/high16 v42, 0x42c80000    # 100.0f

    div-float v19, v19, v42

    const/high16 v42, 0x42c80000    # 100.0f

    div-float v20, v20, v42

    const/high16 v42, 0x42c80000    # 100.0f

    div-float v21, v21, v42

    const/high16 v42, 0x42c80000    # 100.0f

    div-float v22, v22, v42

    const/high16 v42, 0x42c80000    # 100.0f

    div-float v23, v23, v42

    const/high16 v42, 0x42c80000    # 100.0f

    div-float v17, v17, v42

    const/high16 v42, 0x42c80000    # 100.0f

    div-float v10, v10, v42

    const/high16 v42, 0x42c80000    # 100.0f

    div-float v11, v11, v42

    const/high16 v42, 0x42c80000    # 100.0f

    div-float v12, v12, v42

    const/high16 v42, 0x42c80000    # 100.0f

    div-float v13, v13, v42

    const/high16 v42, 0x42c80000    # 100.0f

    div-float v14, v14, v42

    const/high16 v42, 0x42c80000    # 100.0f

    div-float v32, v32, v42

    const/high16 v42, 0x42c80000    # 100.0f

    div-float v33, v33, v42

    const/high16 v42, 0x42c80000    # 100.0f

    div-float v34, v34, v42

    const/high16 v42, 0x42c80000    # 100.0f

    div-float v35, v35, v42

    const/high16 v42, 0x42c80000    # 100.0f

    div-float v36, v36, v42

    const/high16 v42, 0x42c80000    # 100.0f

    div-float v3, v3, v42

    const/high16 v42, 0x42c80000    # 100.0f

    div-float v4, v4, v42

    const/high16 v42, 0x42c80000    # 100.0f

    div-float v5, v5, v42

    const/high16 v42, 0x42c80000    # 100.0f

    div-float v6, v6, v42

    const/high16 v42, 0x42c80000    # 100.0f

    div-float v7, v7, v42

    const/high16 v42, 0x42c80000    # 100.0f

    div-float v25, v25, v42

    const/high16 v42, 0x42c80000    # 100.0f

    div-float v26, v26, v42

    const/high16 v42, 0x42c80000    # 100.0f

    div-float v27, v27, v42

    const/high16 v42, 0x42c80000    # 100.0f

    div-float v28, v28, v42

    const/high16 v42, 0x42c80000    # 100.0f

    div-float v29, v29, v42

    const/high16 v42, 0x42c80000    # 100.0f

    div-float v24, v24, v42

    const/high16 v42, 0x42c80000    # 100.0f

    div-float v2, v2, v42

    const/high16 v42, 0x42c80000    # 100.0f

    div-float v16, v16, v42

    const/high16 v42, 0x42c80000    # 100.0f

    div-float v15, v15, v42

    const/high16 v42, 0x42c80000    # 100.0f

    div-float v38, v38, v42

    const/high16 v42, 0x42c80000    # 100.0f

    div-float v37, v37, v42

    const/high16 v42, 0x42c80000    # 100.0f

    div-float v9, v9, v42

    const/high16 v42, 0x42c80000    # 100.0f

    div-float v8, v8, v42

    const/high16 v42, 0x42c80000    # 100.0f

    div-float v31, v31, v42

    const/high16 v42, 0x42c80000    # 100.0f

    div-float v30, v30, v42

    :cond_29e
    move-object/from16 v0, p0

    iget-boolean v0, v0, LSmali;->conditionC:Z

    move/from16 v42, v0

    if-eqz v42, :cond_33a

    const/high16 v42, 0x41400000    # 12.0f

    div-float v18, v18, v42

    const/high16 v42, 0x41400000    # 12.0f

    div-float v19, v19, v42

    const/high16 v42, 0x41400000    # 12.0f

    div-float v20, v20, v42

    const/high16 v42, 0x41400000    # 12.0f

    div-float v21, v21, v42

    const/high16 v42, 0x41400000    # 12.0f

    div-float v22, v22, v42

    const/high16 v42, 0x41400000    # 12.0f

    div-float v23, v23, v42

    const/high16 v42, 0x41400000    # 12.0f

    div-float v17, v17, v42

    const/high16 v42, 0x41400000    # 12.0f

    div-float v10, v10, v42

    const/high16 v42, 0x41400000    # 12.0f

    div-float v11, v11, v42

    const/high16 v42, 0x41400000    # 12.0f

    div-float v12, v12, v42

    const/high16 v42, 0x41400000    # 12.0f

    div-float v13, v13, v42

    const/high16 v42, 0x41400000    # 12.0f

    div-float v14, v14, v42

    const/high16 v42, 0x41400000    # 12.0f

    div-float v32, v32, v42

    const/high16 v42, 0x41400000    # 12.0f

    div-float v33, v33, v42

    const/high16 v42, 0x41400000    # 12.0f

    div-float v34, v34, v42

    const/high16 v42, 0x41400000    # 12.0f

    div-float v35, v35, v42

    const/high16 v42, 0x41400000    # 12.0f

    div-float v36, v36, v42

    const/high16 v42, 0x41400000    # 12.0f

    div-float v3, v3, v42

    const/high16 v42, 0x41400000    # 12.0f

    div-float v4, v4, v42

    const/high16 v42, 0x41400000    # 12.0f

    div-float v5, v5, v42

    const/high16 v42, 0x41400000    # 12.0f

    div-float v6, v6, v42

    const/high16 v42, 0x41400000    # 12.0f

    div-float v7, v7, v42

    const/high16 v42, 0x41400000    # 12.0f

    div-float v25, v25, v42

    const/high16 v42, 0x41400000    # 12.0f

    div-float v26, v26, v42

    const/high16 v42, 0x41400000    # 12.0f

    div-float v27, v27, v42

    const/high16 v42, 0x41400000    # 12.0f

    div-float v28, v28, v42

    const/high16 v42, 0x41400000    # 12.0f

    div-float v29, v29, v42

    const/high16 v42, 0x41400000    # 12.0f

    div-float v24, v24, v42

    const/high16 v42, 0x41400000    # 12.0f

    div-float v2, v2, v42

    const/high16 v42, 0x41400000    # 12.0f

    div-float v16, v16, v42

    const/high16 v42, 0x41400000    # 12.0f

    div-float v15, v15, v42

    const/high16 v42, 0x41400000    # 12.0f

    div-float v38, v38, v42

    const/high16 v42, 0x41400000    # 12.0f

    div-float v37, v37, v42

    const/high16 v42, 0x41400000    # 12.0f

    div-float v9, v9, v42

    const/high16 v42, 0x41400000    # 12.0f

    div-float v8, v8, v42

    const/high16 v42, 0x41400000    # 12.0f

    div-float v31, v31, v42

    const/high16 v42, 0x41400000    # 12.0f

    div-float v30, v30, v42

    :cond_33a
    const/16 v41, 0x0

    const/high16 v42, 0x42c80000    # 100.0f

    mul-float v42, v42, v41

    invoke-static/range {v42 .. v42}, Ljava/lang/Math;->round(F)I

    move-result v42

    move/from16 v0, v42

    int-to-float v0, v0

    move/from16 v42, v0

    const/high16 v43, 0x42c80000    # 100.0f

    div-float v41, v42, v43

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

    return-void

    :cond_367
    const/high16 v19, 0x3f800000    # 1.0f

    goto/16 :goto_c

    :cond_36b
    const/high16 v18, 0x3f800000    # 1.0f

    goto/16 :goto_16

    :cond_36f
    const/high16 v20, 0x3f800000    # 1.0f

    goto/16 :goto_20

    :cond_373
    const/high16 v21, 0x3f800000    # 1.0f

    goto/16 :goto_2a

    :cond_377
    const/high16 v22, 0x3f800000    # 1.0f

    goto/16 :goto_34

    :cond_37b
    const/high16 v23, 0x3f800000    # 1.0f

    goto/16 :goto_3e

    :cond_37f
    const/high16 v17, 0x3f800000    # 1.0f

    goto/16 :goto_48

    :cond_383
    const/high16 v2, 0x3f800000    # 1.0f

    goto/16 :goto_51

    :cond_387
    const/high16 v24, 0x3f800000    # 1.0f

    goto/16 :goto_5b

    :cond_38b
    const/high16 v25, 0x3f800000    # 1.0f

    goto/16 :goto_65

    :cond_38f
    const/high16 v26, 0x3f800000    # 1.0f

    goto/16 :goto_6f

    :cond_393
    const/high16 v27, 0x3f800000    # 1.0f

    goto/16 :goto_79

    :cond_397
    const/high16 v28, 0x3f800000    # 1.0f

    goto/16 :goto_83

    :cond_39b
    const/high16 v29, 0x3f800000    # 1.0f

    goto/16 :goto_8d

    :cond_39f
    const/high16 v31, 0x3f800000    # 1.0f

    goto/16 :goto_97

    :cond_3a3
    const/high16 v30, 0x3f800000    # 1.0f

    goto/16 :goto_a1

    :cond_3a7
    const/high16 v3, 0x3f800000    # 1.0f

    goto/16 :goto_aa

    :cond_3ab
    const/high16 v4, 0x3f800000    # 1.0f

    goto/16 :goto_b3

    :cond_3af
    const/high16 v5, 0x3f800000    # 1.0f

    goto/16 :goto_bc

    :cond_3b3
    const/high16 v6, 0x3f800000    # 1.0f

    goto/16 :goto_c5

    :cond_3b7
    const/high16 v7, 0x3f800000    # 1.0f

    goto/16 :goto_ce

    :cond_3bb
    const/high16 v9, 0x3f800000    # 1.0f

    goto/16 :goto_d7

    :cond_3bf
    const/high16 v8, 0x3f800000    # 1.0f

    goto/16 :goto_e0

    :cond_3c3
    const/high16 v32, 0x3f800000    # 1.0f

    goto/16 :goto_ea

    :cond_3c7
    const/high16 v33, 0x3f800000    # 1.0f

    goto/16 :goto_f4

    :cond_3cb
    const/high16 v34, 0x3f800000    # 1.0f

    goto/16 :goto_fe

    :cond_3cf
    const/high16 v35, 0x3f800000    # 1.0f

    goto/16 :goto_108

    :cond_3d3
    const/high16 v36, 0x3f800000    # 1.0f

    goto/16 :goto_112

    :cond_3d7
    const/high16 v38, 0x3f800000    # 1.0f

    goto/16 :goto_11c

    :cond_3db
    const/high16 v37, 0x3f800000    # 1.0f

    goto/16 :goto_126

    :cond_3df
    const/high16 v10, 0x3f800000    # 1.0f

    goto/16 :goto_12f

    :cond_3e3
    const/high16 v11, 0x3f800000    # 1.0f

    goto/16 :goto_138

    :cond_3e7
    const/high16 v12, 0x3f800000    # 1.0f

    goto/16 :goto_141

    :cond_3eb
    const/high16 v13, 0x3f800000    # 1.0f

    goto/16 :goto_14a

    :cond_3ef
    const/high16 v14, 0x3f800000    # 1.0f

    goto/16 :goto_153

    :cond_3f3
    const/high16 v16, 0x3f800000    # 1.0f

    goto/16 :goto_15d

    :cond_3f7
    const/high16 v15, 0x3f800000    # 1.0f

    goto/16 :goto_166
.end method

##  CHECK-START-ARM64: void Smali.testD8() register (after)
##  CHECK: begin_block
##  CHECK:   name "B0"
##  CHECK:       <<This:l\d+>>  ParameterValue
##  CHECK: end_block

##  CHECK: begin_block
##  CHECK:  successors "<<ThenBlockA:B\d+>>" "<<ElseBlockA:B\d+>>"
##  CHECK:       <<CondA:z\d+>>  InstanceFieldGet [<<This>>] field_name:Smali.conditionA
##  CHECK:                       If [<<CondA>>]
##  CHECK: end_block

##  CHECK: begin_block
##  CHECK:   name "<<ThenBlockA>>"
##  CHECK: end_block
##  CHECK: begin_block
##  CHECK:   name "<<ElseBlockA>>"
##  CHECK:                       ParallelMove moves:[d2->d0,40(sp)->d17,d20->d26,d19->d27,d27->d1,d26->d2,d14->d20,d13->d19,d17->d14,d0->d13]
##  CHECK: end_block

##  CHECK: begin_block
##  CHECK:  successors "<<ThenBlockB:B\d+>>" "<<ElseBlockB:B\d+>>"
##  CHECK:       <<CondB:z\d+>>  InstanceFieldGet [<<This>>] field_name:Smali.conditionB
##  CHECK:                       If [<<CondB>>]
##  CHECK: end_block

##  CHECK: begin_block
##  CHECK:   name "<<ThenBlockB>>"
##  CHECK: end_block
##  CHECK: begin_block
##  CHECK:   name "<<ElseBlockB>>"
##  CHECK:                       ParallelMove moves:[#100->d13,16(sp)->d1,20(sp)->d2,28(sp)->d19,24(sp)->d20,36(sp)->d14,32(sp)->16(sp),d1->20(sp),d2->24(sp),d20->28(sp),d19->32(sp),d14->36(sp),d13->40(sp)]
##  CHECK: end_block

##  CHECK-START-ARM64: void Smali.testD8() disassembly (after)
##  CHECK: begin_block
##  CHECK:   name "B0"
##  CHECK:       <<This:l\d+>>  ParameterValue
##  CHECK: end_block

##  CHECK: begin_block
##  CHECK:  successors "<<ThenBlockA:B\d+>>" "<<ElseBlockA:B\d+>>"
##  CHECK:       <<CondA:z\d+>>  InstanceFieldGet [<<This>>] field_name:Smali.conditionA
##  CHECK:                       If [<<CondA>>]
##  CHECK: end_block

##  CHECK: begin_block
##  CHECK:   name "<<ThenBlockA>>"
##  CHECK: end_block
##  CHECK: begin_block
##  CHECK:   name "<<ElseBlockA>>"
##  CHECK: end_block

##  CHECK: begin_block
##  CHECK:  successors "<<ThenBlockB:B\d+>>" "<<ElseBlockB:B\d+>>"
##  CHECK:       <<CondB:z\d+>>  InstanceFieldGet [<<This>>] field_name:Smali.conditionB
##  CHECK:                       If [<<CondB>>]
##  CHECK: end_block

##  CHECK: begin_block
##  CHECK:   name "<<ThenBlockB>>"
##  CHECK: end_block
##  CHECK: begin_block
##  CHECK:   name "<<ElseBlockB>>"
##  CHECK:                       ParallelMove moves:[invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid]
##  CHECK:                         ldr w16, [sp, #32]
##  CHECK:                         str s19, [sp, #32]
##  CHECK:                         ldr s19, [sp, #28]
##  CHECK:                         str s20, [sp, #28]
##  CHECK:                         ldr s20, [sp, #24]
##  CHECK:                         str s2, [sp, #24]
##  CHECK:                         ldr s2, [sp, #20]
##  CHECK:                         str s1, [sp, #20]
##  CHECK:                         ldr s1, [sp, #16]
##  CHECK:                         str w16, [sp, #16]
##  CHECK:                         fmov d31, d14
##  CHECK:                         ldr s14, [sp, #36]
##  CHECK:                         str s31, [sp, #36]
##  CHECK:                         str s13, [sp, #40]
##  CHECK:                         ldr s13, pc+580 (addr 0x61c) (100)
##  CHECK: end_block
.method public testD8()V
    .registers 47

    move-object/from16 v0, p0

    const-string v1, ""

    iget-boolean v2, v0, LSmali;->b17:Z

    if-eqz v2, :cond_a

    const/4 v2, 0x0

    goto :goto_c

    :cond_a
    const/high16 v2, 0x3f800000    # 1.0f

    :goto_c
    iget-boolean v5, v0, LSmali;->b16:Z

    if-eqz v5, :cond_12

    const/4 v5, 0x0

    goto :goto_14

    :cond_12
    const/high16 v5, 0x3f800000    # 1.0f

    :goto_14
    iget-boolean v6, v0, LSmali;->b18:Z

    if-eqz v6, :cond_1a

    const/4 v6, 0x0

    goto :goto_1c

    :cond_1a
    const/high16 v6, 0x3f800000    # 1.0f

    :goto_1c
    iget-boolean v7, v0, LSmali;->b19:Z

    if-eqz v7, :cond_22

    const/4 v7, 0x0

    goto :goto_24

    :cond_22
    const/high16 v7, 0x3f800000    # 1.0f

    :goto_24
    iget-boolean v8, v0, LSmali;->b20:Z

    if-eqz v8, :cond_2a

    const/4 v8, 0x0

    goto :goto_2c

    :cond_2a
    const/high16 v8, 0x3f800000    # 1.0f

    :goto_2c
    iget-boolean v9, v0, LSmali;->b21:Z

    if-eqz v9, :cond_32

    const/4 v9, 0x0

    goto :goto_34

    :cond_32
    const/high16 v9, 0x3f800000    # 1.0f

    :goto_34
    iget-boolean v10, v0, LSmali;->b15:Z

    if-eqz v10, :cond_3a

    const/4 v10, 0x0

    goto :goto_3c

    :cond_3a
    const/high16 v10, 0x3f800000    # 1.0f

    :goto_3c
    iget-boolean v11, v0, LSmali;->b00:Z

    if-eqz v11, :cond_42

    const/4 v11, 0x0

    goto :goto_44

    :cond_42
    const/high16 v11, 0x3f800000    # 1.0f

    :goto_44
    iget-boolean v12, v0, LSmali;->b22:Z

    if-eqz v12, :cond_4a

    const/4 v12, 0x0

    goto :goto_4c

    :cond_4a
    const/high16 v12, 0x3f800000    # 1.0f

    :goto_4c
    iget-boolean v13, v0, LSmali;->b23:Z

    if-eqz v13, :cond_52

    const/4 v13, 0x0

    goto :goto_54

    :cond_52
    const/high16 v13, 0x3f800000    # 1.0f

    :goto_54
    iget-boolean v14, v0, LSmali;->b24:Z

    if-eqz v14, :cond_5a

    const/4 v14, 0x0

    goto :goto_5c

    :cond_5a
    const/high16 v14, 0x3f800000    # 1.0f

    :goto_5c
    iget-boolean v15, v0, LSmali;->b25:Z

    if-eqz v15, :cond_62

    const/4 v15, 0x0

    goto :goto_64

    :cond_62
    const/high16 v15, 0x3f800000    # 1.0f

    :goto_64
    iget-boolean v3, v0, LSmali;->b26:Z

    if-eqz v3, :cond_6a

    const/4 v3, 0x0

    goto :goto_6c

    :cond_6a
    const/high16 v3, 0x3f800000    # 1.0f

    :goto_6c
    iget-boolean v4, v0, LSmali;->b27:Z

    if-eqz v4, :cond_72

    const/4 v4, 0x0

    goto :goto_74

    :cond_72
    const/high16 v4, 0x3f800000    # 1.0f

    :goto_74
    move-object/from16 v18, v1

    iget-boolean v1, v0, LSmali;->b29:Z

    if-eqz v1, :cond_7c

    const/4 v1, 0x0

    goto :goto_7e

    :cond_7c
    const/high16 v1, 0x3f800000    # 1.0f

    :goto_7e
    move/from16 v19, v1

    iget-boolean v1, v0, LSmali;->b28:Z

    if-eqz v1, :cond_86

    const/4 v1, 0x0

    goto :goto_88

    :cond_86
    const/high16 v1, 0x3f800000    # 1.0f

    :goto_88
    move/from16 v20, v1

    iget-boolean v1, v0, LSmali;->b01:Z

    if-eqz v1, :cond_90

    const/4 v1, 0x0

    goto :goto_92

    :cond_90
    const/high16 v1, 0x3f800000    # 1.0f

    :goto_92
    move/from16 v21, v11

    iget-boolean v11, v0, LSmali;->b02:Z

    if-eqz v11, :cond_9a

    const/4 v11, 0x0

    goto :goto_9c

    :cond_9a
    const/high16 v11, 0x3f800000    # 1.0f

    :goto_9c
    move/from16 v22, v12

    iget-boolean v12, v0, LSmali;->b03:Z

    if-eqz v12, :cond_a4

    const/4 v12, 0x0

    goto :goto_a6

    :cond_a4
    const/high16 v12, 0x3f800000    # 1.0f

    :goto_a6
    move/from16 v23, v4

    iget-boolean v4, v0, LSmali;->b04:Z

    if-eqz v4, :cond_ae

    const/4 v4, 0x0

    goto :goto_b0

    :cond_ae
    const/high16 v4, 0x3f800000    # 1.0f

    :goto_b0
    move/from16 v24, v3

    iget-boolean v3, v0, LSmali;->b05:Z

    if-eqz v3, :cond_b8

    const/4 v3, 0x0

    goto :goto_ba

    :cond_b8
    const/high16 v3, 0x3f800000    # 1.0f

    :goto_ba
    move/from16 v25, v15

    iget-boolean v15, v0, LSmali;->b07:Z

    if-eqz v15, :cond_c2

    const/4 v15, 0x0

    goto :goto_c4

    :cond_c2
    const/high16 v15, 0x3f800000    # 1.0f

    :goto_c4
    move/from16 v26, v15

    iget-boolean v15, v0, LSmali;->b06:Z

    if-eqz v15, :cond_cc

    const/4 v15, 0x0

    goto :goto_ce

    :cond_cc
    const/high16 v15, 0x3f800000    # 1.0f

    :goto_ce
    move/from16 v27, v15

    iget-boolean v15, v0, LSmali;->b30:Z

    if-eqz v15, :cond_d6

    const/4 v15, 0x0

    goto :goto_d8

    :cond_d6
    const/high16 v15, 0x3f800000    # 1.0f

    :goto_d8
    move/from16 v28, v14

    iget-boolean v14, v0, LSmali;->b31:Z

    if-eqz v14, :cond_e0

    const/4 v14, 0x0

    goto :goto_e2

    :cond_e0
    const/high16 v14, 0x3f800000    # 1.0f

    :goto_e2
    move/from16 v29, v13

    iget-boolean v13, v0, LSmali;->b32:Z

    if-eqz v13, :cond_ea

    const/4 v13, 0x0

    goto :goto_ec

    :cond_ea
    const/high16 v13, 0x3f800000    # 1.0f

    :goto_ec
    move/from16 v30, v3

    iget-boolean v3, v0, LSmali;->b33:Z

    if-eqz v3, :cond_f4

    const/4 v3, 0x0

    goto :goto_f6

    :cond_f4
    const/high16 v3, 0x3f800000    # 1.0f

    :goto_f6
    move/from16 v31, v4

    iget-boolean v4, v0, LSmali;->b34:Z

    if-eqz v4, :cond_fe

    const/4 v4, 0x0

    goto :goto_100

    :cond_fe
    const/high16 v4, 0x3f800000    # 1.0f

    :goto_100
    move/from16 v32, v12

    iget-boolean v12, v0, LSmali;->b36:Z

    if-eqz v12, :cond_108

    const/4 v12, 0x0

    goto :goto_10a

    :cond_108
    const/high16 v12, 0x3f800000    # 1.0f

    :goto_10a
    move/from16 v33, v12

    iget-boolean v12, v0, LSmali;->b35:Z

    if-eqz v12, :cond_112

    const/4 v12, 0x0

    goto :goto_114

    :cond_112
    const/high16 v12, 0x3f800000    # 1.0f

    :goto_114
    move/from16 v34, v12

    iget-boolean v12, v0, LSmali;->b08:Z

    if-eqz v12, :cond_11c

    const/4 v12, 0x0

    goto :goto_11e

    :cond_11c
    const/high16 v12, 0x3f800000    # 1.0f

    :goto_11e
    move/from16 v35, v11

    iget-boolean v11, v0, LSmali;->b09:Z

    if-eqz v11, :cond_126

    const/4 v11, 0x0

    goto :goto_128

    :cond_126
    const/high16 v11, 0x3f800000    # 1.0f

    :goto_128
    move/from16 v36, v1

    iget-boolean v1, v0, LSmali;->b10:Z

    if-eqz v1, :cond_130

    const/4 v1, 0x0

    goto :goto_132

    :cond_130
    const/high16 v1, 0x3f800000    # 1.0f

    :goto_132
    move/from16 v37, v4

    iget-boolean v4, v0, LSmali;->b11:Z

    if-eqz v4, :cond_13a

    const/4 v4, 0x0

    goto :goto_13c

    :cond_13a
    const/high16 v4, 0x3f800000    # 1.0f

    :goto_13c
    move/from16 v38, v3

    iget-boolean v3, v0, LSmali;->b12:Z

    if-eqz v3, :cond_144

    const/4 v3, 0x0

    goto :goto_146

    :cond_144
    const/high16 v3, 0x3f800000    # 1.0f

    :goto_146
    move/from16 v39, v13

    iget-boolean v13, v0, LSmali;->b14:Z

    if-eqz v13, :cond_14e

    const/4 v13, 0x0

    goto :goto_150

    :cond_14e
    const/high16 v13, 0x3f800000    # 1.0f

    :goto_150
    move/from16 v40, v13

    iget-boolean v13, v0, LSmali;->b13:Z

    if-eqz v13, :cond_159

    const/16 v16, 0x0

    goto :goto_15b

    :cond_159
    const/high16 v16, 0x3f800000    # 1.0f

    :goto_15b
    move/from16 v13, v16

    move/from16 v41, v13

    iget-boolean v13, v0, LSmali;->conditionA:Z

    if-eqz v13, :cond_1a2

    const/high16 v13, 0x447a0000    # 1000.0f

    div-float/2addr v5, v13

    div-float/2addr v2, v13

    div-float/2addr v6, v13

    div-float/2addr v7, v13

    div-float/2addr v8, v13

    div-float/2addr v9, v13

    div-float/2addr v10, v13

    div-float/2addr v12, v13

    div-float/2addr v11, v13

    div-float/2addr v1, v13

    div-float/2addr v4, v13

    div-float/2addr v3, v13

    div-float/2addr v15, v13

    div-float/2addr v14, v13

    div-float v16, v39, v13

    div-float v38, v38, v13

    div-float v37, v37, v13

    div-float v36, v36, v13

    div-float v35, v35, v13

    div-float v32, v32, v13

    div-float v31, v31, v13

    div-float v30, v30, v13

    div-float v29, v29, v13

    div-float v28, v28, v13

    div-float v25, v25, v13

    div-float v24, v24, v13

    div-float v23, v23, v13

    div-float v22, v22, v13

    div-float v21, v21, v13

    div-float v39, v40, v13

    div-float v40, v41, v13

    div-float v33, v33, v13

    div-float v34, v34, v13

    div-float v26, v26, v13

    div-float v27, v27, v13

    div-float v19, v19, v13

    div-float v13, v20, v13

    goto :goto_1aa

    :cond_1a2
    move/from16 v13, v20

    move/from16 v16, v39

    move/from16 v39, v40

    move/from16 v40, v41

    :goto_1aa
    move/from16 v42, v13

    iget-boolean v13, v0, LSmali;->conditionB:Z

    const/high16 v20, 0x42c80000    # 100.0f

    if-eqz v13, :cond_1fd

    div-float v5, v5, v20

    div-float v2, v2, v20

    div-float v6, v6, v20

    div-float v7, v7, v20

    div-float v8, v8, v20

    div-float v9, v9, v20

    div-float v10, v10, v20

    div-float v12, v12, v20

    div-float v11, v11, v20

    div-float v1, v1, v20

    div-float v4, v4, v20

    div-float v3, v3, v20

    div-float v15, v15, v20

    div-float v14, v14, v20

    div-float v16, v16, v20

    div-float v38, v38, v20

    div-float v37, v37, v20

    div-float v36, v36, v20

    div-float v35, v35, v20

    div-float v32, v32, v20

    div-float v31, v31, v20

    div-float v30, v30, v20

    div-float v29, v29, v20

    div-float v28, v28, v20

    div-float v25, v25, v20

    div-float v24, v24, v20

    div-float v23, v23, v20

    div-float v22, v22, v20

    div-float v21, v21, v20

    div-float v39, v39, v20

    div-float v40, v40, v20

    div-float v33, v33, v20

    div-float v34, v34, v20

    div-float v26, v26, v20

    div-float v27, v27, v20

    div-float v19, v19, v20

    div-float v13, v42, v20

    goto :goto_1ff

    :cond_1fd
    move/from16 v13, v42

    :goto_1ff
    move/from16 v43, v13

    iget-boolean v13, v0, LSmali;->conditionC:Z

    if-eqz v13, :cond_244

    const/high16 v13, 0x41400000    # 12.0f

    div-float/2addr v5, v13

    div-float/2addr v2, v13

    div-float/2addr v6, v13

    div-float/2addr v7, v13

    div-float/2addr v8, v13

    div-float/2addr v9, v13

    div-float/2addr v10, v13

    div-float/2addr v12, v13

    div-float/2addr v11, v13

    div-float/2addr v1, v13

    div-float/2addr v4, v13

    div-float/2addr v3, v13

    div-float/2addr v15, v13

    div-float/2addr v14, v13

    div-float v16, v16, v13

    div-float v38, v38, v13

    div-float v37, v37, v13

    div-float v36, v36, v13

    div-float v35, v35, v13

    div-float v32, v32, v13

    div-float v31, v31, v13

    div-float v30, v30, v13

    div-float v29, v29, v13

    div-float v28, v28, v13

    div-float v25, v25, v13

    div-float v24, v24, v13

    div-float v23, v23, v13

    div-float v22, v22, v13

    div-float v21, v21, v13

    div-float v39, v39, v13

    div-float v40, v40, v13

    div-float v33, v33, v13

    div-float v34, v34, v13

    div-float v26, v26, v13

    div-float v27, v27, v13

    div-float v19, v19, v13

    div-float v13, v43, v13

    goto :goto_246

    :cond_244
    move/from16 v13, v43

    :goto_246
    const/16 v17, 0x0

    mul-float v0, v20, v17

    invoke-static {v0}, Ljava/lang/Math;->round(F)I

    move-result v0

    int-to-float v0, v0

    div-float v0, v0, v20

    move/from16 v44, v1

    new-instance v1, Ljava/lang/StringBuilder;

    invoke-direct {v1}, Ljava/lang/StringBuilder;-><init>()V

    invoke-virtual {v1, v0}, Ljava/lang/StringBuilder;->append(F)Ljava/lang/StringBuilder;

    move/from16 v45, v0

    move-object/from16 v0, v18

    invoke-virtual {v1, v0}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    invoke-virtual {v1}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    move-result-object v1

    return-void
.end method
