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
.source "Main2.java"


# direct methods
.method constructor <init>()V
    .registers 1

    .prologue
    .line 17
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    return-void
.end method

# Elementary test negating a boolean. Verifies that blocks are merged and
# empty branches removed.

## CHECK-START: boolean Main2.BooleanNot(boolean) select_generator (before)
## CHECK-DAG:     <<Param:z\d+>>    ParameterValue
## CHECK-DAG:     <<Const0:i\d+>>   IntConstant 0
## CHECK-DAG:     <<Const1:i\d+>>   IntConstant 1
## CHECK-DAG:                       If [<<Param>>]
## CHECK-DAG:     <<Phi:i\d+>>      Phi [<<Const1>>,<<Const0>>]
## CHECK-DAG:                       Return [<<Phi>>]

## CHECK-START: boolean Main2.BooleanNot(boolean) select_generator (before)
## CHECK:                           Goto
## CHECK:                           Goto
## CHECK:                           Goto
## CHECK-NOT:                       Goto

## CHECK-START: boolean Main2.BooleanNot(boolean) select_generator (after)
## CHECK-DAG:     <<Param:z\d+>>    ParameterValue
## CHECK-DAG:     <<Const0:i\d+>>   IntConstant 0
## CHECK-DAG:     <<Const1:i\d+>>   IntConstant 1
## CHECK-DAG:     <<NotParam:i\d+>> Select [<<Const1>>,<<Const0>>,<<Param>>]
## CHECK-DAG:                       Return [<<NotParam>>]

## CHECK-START: boolean Main2.BooleanNot(boolean) select_generator (after)
## CHECK-NOT:                       If
## CHECK-NOT:                       Phi

## CHECK-START: boolean Main2.BooleanNot(boolean) select_generator (after)
## CHECK:                           Goto
## CHECK-NOT:                       Goto

# The original java source of this method:
#
#     return !x;
#
.method public static BooleanNot(Z)Z
    .registers 2
    .param p0, "x"    # Z

    .prologue
    .line 70
    if-nez p0, :cond_4

    const/4 v0, 0x1

    :goto_3
    return v0

    :cond_4
    const/4 v0, 0x0

    goto :goto_3
.end method

# Program which further uses negated conditions.
# Note that Phis are discovered retrospectively.

## CHECK-START: boolean Main2.ValuesOrdered(int, int, int) select_generator (before)
## CHECK-DAG:     <<ParamX:i\d+>>   ParameterValue
## CHECK-DAG:     <<ParamY:i\d+>>   ParameterValue
## CHECK-DAG:     <<ParamZ:i\d+>>   ParameterValue
## CHECK-DAG:     <<Const0:i\d+>>   IntConstant 0
## CHECK-DAG:     <<Const1:i\d+>>   IntConstant 1
## CHECK-DAG:     <<CondXY:z\d+>>   GreaterThan [<<ParamX>>,<<ParamY>>]
## CHECK-DAG:                       If [<<CondXY>>]
## CHECK-DAG:     <<CondYZ:z\d+>>   GreaterThan [<<ParamY>>,<<ParamZ>>]
## CHECK-DAG:                       If [<<CondYZ>>]
## CHECK-DAG:     <<CondXYZ:z\d+>>  NotEqual [<<PhiXY:i\d+>>,<<PhiYZ:i\d+>>]
## CHECK-DAG:                       If [<<CondXYZ>>]
## CHECK-DAG:                       Return [<<PhiXYZ:i\d+>>]
## CHECK-DAG:     <<PhiXY>>         Phi [<<Const1>>,<<Const0>>]
## CHECK-DAG:     <<PhiYZ>>         Phi [<<Const1>>,<<Const0>>]
## CHECK-DAG:     <<PhiXYZ>>        Phi [<<Const1>>,<<Const0>>]

## CHECK-START: boolean Main2.ValuesOrdered(int, int, int) select_generator (after)
## CHECK-DAG:     <<ParamX:i\d+>>   ParameterValue
## CHECK-DAG:     <<ParamY:i\d+>>   ParameterValue
## CHECK-DAG:     <<ParamZ:i\d+>>   ParameterValue
## CHECK-DAG:     <<Const0:i\d+>>   IntConstant 0
## CHECK-DAG:     <<Const1:i\d+>>   IntConstant 1
## CHECK-DAG:     <<CmpXY:z\d+>>    GreaterThan [<<ParamX>>,<<ParamY>>]
## CHECK-DAG:     <<SelXY:i\d+>>    Select [<<Const1>>,<<Const0>>,<<CmpXY>>]
## CHECK-DAG:     <<CmpYZ:z\d+>>    GreaterThan [<<ParamY>>,<<ParamZ>>]
## CHECK-DAG:     <<SelYZ:i\d+>>    Select [<<Const1>>,<<Const0>>,<<CmpYZ>>]
## CHECK-DAG:     <<CmpXYZ:z\d+>>   NotEqual [<<SelXY>>,<<SelYZ>>]
## CHECK-DAG:     <<SelXYZ:i\d+>>   Select [<<Const1>>,<<Const0>>,<<CmpXYZ>>]
## CHECK-DAG:                       Return [<<SelXYZ>>]

# The original java source of this method:
#
#     return (x <= y) == (y <= z);
#
.method public static ValuesOrdered(III)Z
    .registers 7
    .param p0, "x"    # I
    .param p1, "y"    # I
    .param p2, "z"    # I

    .prologue
    const/4 v0, 0x1

    const/4 v1, 0x0

    .line 166
    if-gt p0, p1, :cond_b

    move v3, v0

    :goto_5
    if-gt p1, p2, :cond_d

    move v2, v0

    :goto_8
    if-ne v3, v2, :cond_f

    :goto_a
    return v0

    :cond_b
    move v3, v1

    goto :goto_5

    :cond_d
    move v2, v1

    goto :goto_8

    :cond_f
    move v0, v1

    goto :goto_a
.end method

## CHECK-START: int Main2.NegatedCondition(boolean) select_generator (before)
## CHECK-DAG:     <<Param:z\d+>>    ParameterValue
## CHECK-DAG:     <<Const42:i\d+>>  IntConstant 42
## CHECK-DAG:     <<Const43:i\d+>>  IntConstant 43
## CHECK-DAG:                       If [<<Param>>]
## CHECK-DAG:     <<Phi:i\d+>>      Phi [<<Const42>>,<<Const43>>]
## CHECK-DAG:                       Return [<<Phi>>]

## CHECK-START: int Main2.NegatedCondition(boolean) select_generator (after)
## CHECK-DAG:     <<Param:z\d+>>    ParameterValue
## CHECK-DAG:     <<Const42:i\d+>>  IntConstant 42
## CHECK-DAG:     <<Const43:i\d+>>  IntConstant 43
## CHECK-DAG:     <<Select:i\d+>>   Select [<<Const43>>,<<Const42>>,<<Param>>]
## CHECK-DAG:                       Return [<<Select>>]

## CHECK-START: int Main2.NegatedCondition(boolean) select_generator (after)
## CHECK-NOT:                       BooleanNot

# The original java source of this method:
#
#     if (x != false) {
#       return 42;
#     } else {
#       return 43;
#     }
#
.method public static NegatedCondition(Z)I
    .registers 2
    .param p0, "x"    # Z

    .prologue
    .line 188
    if-eqz p0, :cond_5

    .line 189
    const/16 v0, 0x2a

    .line 191
    :goto_4
    return v0

    :cond_5
    const/16 v0, 0x2b

    goto :goto_4
.end method

## CHECK-START: int Main2.MultiplePhis() select_generator (before)
## CHECK-DAG:     <<Const0:i\d+>>   IntConstant 0
## CHECK-DAG:     <<Const1:i\d+>>   IntConstant 1
## CHECK-DAG:     <<Const13:i\d+>>  IntConstant 13
## CHECK-DAG:     <<Const42:i\d+>>  IntConstant 42
## CHECK-DAG:     <<PhiX:i\d+>>     Phi [<<Const0>>,<<Const13>>,<<Const42>>]
## CHECK-DAG:     <<PhiY:i\d+>>     Phi [<<Const1>>,<<Add:i\d+>>,<<Add>>]
## CHECK-DAG:     <<Add>>           Add [<<PhiY>>,<<Const1>>]
## CHECK-DAG:     <<Cond:z\d+>>     LessThanOrEqual [<<Add>>,<<Const1>>]
## CHECK-DAG:                       If [<<Cond>>]
## CHECK-DAG:                       Return [<<PhiX>>]

## CHECK-START: int Main2.MultiplePhis() select_generator (after)
## CHECK-DAG:     <<Const0:i\d+>>   IntConstant 0
## CHECK-DAG:     <<Const1:i\d+>>   IntConstant 1
## CHECK-DAG:     <<Const13:i\d+>>  IntConstant 13
## CHECK-DAG:     <<Const42:i\d+>>  IntConstant 42
## CHECK-DAG:     <<PhiX:i\d+>>     Phi [<<Const0>>,<<Select:i\d+>>]
## CHECK-DAG:     <<PhiY:i\d+>>     Phi [<<Const1>>,<<Add:i\d+>>]
## CHECK-DAG:     <<Add>>           Add [<<PhiY>>,<<Const1>>]
## CHECK-DAG:     <<Cond:z\d+>>     LessThanOrEqual [<<Add>>,<<Const1>>]
## CHECK-DAG:     <<Select>>        Select [<<Const13>>,<<Const42>>,<<Cond>>]
## CHECK-DAG:                       Return [<<PhiX>>]

# The original java source of this method:
#
#     int x = 0;
#     int y = 1;
#     while (y++ < 10) {
#       if (y > 1) {
#         x = 13;
#       } else {
#         x = 42;
#       }
#     }
#     return x;
#
.method public static MultiplePhis()I
    .registers 4

    .prologue
    .line 290
    const/4 v0, 0x0

    .line 291
    .local v0, "x":I
    const/4 v1, 0x1

    .local v1, "y":I
    move v2, v1

    .line 292
    .end local v1    # "y":I
    .local v2, "y":I
    :goto_3
    add-int/lit8 v1, v2, 0x1

    .end local v2    # "y":I
    .restart local v1    # "y":I
    const/16 v3, 0xa

    if-ge v2, v3, :cond_14

    .line 293
    const/4 v3, 0x1

    if-le v1, v3, :cond_10

    .line 294
    const/16 v0, 0xd

    move v2, v1

    .end local v1    # "y":I
    .restart local v2    # "y":I
    goto :goto_3

    .line 296
    .end local v2    # "y":I
    .restart local v1    # "y":I
    :cond_10
    const/16 v0, 0x2a

    move v2, v1

    .end local v1    # "y":I
    .restart local v2    # "y":I
    goto :goto_3

    .line 299
    .end local v2    # "y":I
    .restart local v1    # "y":I
    :cond_14
    return v0
.end method
