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

.class public LPeelUnroll;

.super Ljava/lang/Object;

## CHECK-START: void PeelUnroll.unrollingWhile(int[]) loop_optimization (before)
## CHECK-DAG: <<Array:l\d+>>    ParameterValue                            loop:none
## CHECK-DAG: <<Const0:i\d+>>   IntConstant 0                             loop:none
## CHECK-DAG: <<Const1:i\d+>>   IntConstant 1                             loop:none
## CHECK-DAG: <<Const2:i\d+>>   IntConstant 2                             loop:none
## CHECK-DAG: <<Const128:i\d+>> IntConstant 128                           loop:none
## CHECK-DAG: <<Limit:i\d+>>    IntConstant 4094                          loop:none
## CHECK-DAG: <<PhiI:i\d+>>     Phi [<<Const0>>,{{i\d+}}]                 loop:<<Loop:B\d+>> outer_loop:none
## CHECK-DAG: <<PhiS:i\d+>>     Phi [<<Const128>>,{{i\d+}}]               loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<AddI:i\d+>>     Add [<<PhiI>>,<<Const1>>]                 loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<Check:z\d+>>    GreaterThanOrEqual [<<PhiI>>,<<Limit>>]   loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<If:v\d+>>       If [<<Check>>]                            loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<Rem:i\d+>>      Rem [<<AddI>>,<<Const2>>]                 loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<NE:z\d+>>       NotEqual [<<Rem>>,<<Const0>>]             loop:<<Loop>>      outer_loop:none
## CHECK-DAG:                   If [<<NE>>]                               loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<AddS:i\d+>>     Add [<<PhiS>>,<<Const1>>]                 loop:<<Loop>>      outer_loop:none
## CHECK-DAG:                   ArraySet                                  loop:<<Loop>>      outer_loop:none
## CHECK-DAG:                   Phi [<<PhiS>>,<<AddS>>]                   loop:<<Loop>>      outer_loop:none

## CHECK-NOT:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
## CHECK-NOT:                   ArraySet                                  loop:<<Loop>>      outer_loop:none

## CHECK-START: void PeelUnroll.unrollingWhile(int[]) loop_optimization (after)
## CHECK-DAG: <<Array:l\d+>>    ParameterValue                            loop:none
## CHECK-DAG: <<Const0:i\d+>>   IntConstant 0                             loop:none
## CHECK-DAG: <<Const1:i\d+>>   IntConstant 1                             loop:none
## CHECK-DAG: <<Const2:i\d+>>   IntConstant 2                             loop:none
## CHECK-DAG: <<Const128:i\d+>> IntConstant 128                           loop:none
## CHECK-DAG: <<Limit:i\d+>>    IntConstant 4094                          loop:none
## CHECK-DAG: <<PhiI:i\d+>>     Phi [<<Const0>>,{{i\d+}}]                 loop:<<Loop:B\d+>> outer_loop:none
## CHECK-DAG: <<PhiS:i\d+>>     Phi [<<Const128>>,{{i\d+}}]               loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<AddI:i\d+>>     Add [<<PhiI>>,<<Const1>>]                 loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<Check:z\d+>>    GreaterThanOrEqual [<<PhiI>>,<<Limit>>]   loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<If:v\d+>>       If [<<Check>>]                            loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<Rem:i\d+>>      Rem [<<AddI>>,<<Const2>>]                 loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<NE:z\d+>>       NotEqual [<<Rem>>,<<Const0>>]             loop:<<Loop>>      outer_loop:none
## CHECK-DAG:                   If [<<NE>>]                               loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<AddS:i\d+>>     Add [<<PhiS>>,<<Const1>>]                 loop:<<Loop>>      outer_loop:none
## CHECK-DAG:                   ArraySet [{{l\d+}},{{i\d+}},<<PhiS>>]     loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<PhiSM:i\d+>>    Phi [<<PhiS>>,<<AddS>>]                   loop:<<Loop>>      outer_loop:none

## CHECK-DAG: <<AddIA:i\d+>>    Add [<<AddI>>,<<Const1>>]                 loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<CheckA:z\d+>>   GreaterThanOrEqual [<<AddI>>,<<Limit>>]   loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<IfA:v\d+>>      If [<<Const0>>]                           loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<RemA:i\d+>>     Rem [<<AddIA>>,<<Const2>>]                loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<NEA:z\d+>>      NotEqual [<<RemA>>,<<Const0>>]            loop:<<Loop>>      outer_loop:none
## CHECK-DAG:                   If [<<NEA>>]                              loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<AddSA:i\d+>>    Add [<<PhiSM>>,<<Const1>>]                loop:<<Loop>>      outer_loop:none
## CHECK-DAG:                   ArraySet [{{l\d+}},{{i\d+}},<<PhiSM>>]    loop:<<Loop>>      outer_loop:none
## CHECK-DAG:                   Phi [<<AddSA>>,<<PhiSM>>]                 loop:<<Loop>>      outer_loop:none

## CHECK-NOT:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
## CHECK-NOT:                   ArraySet                                  loop:<<Loop>>      outer_loop:none
.method public static final unrollingWhile([I)V
    .registers 5
    .param p0, "a"    # [I

    .line 167
    const/4 v0, 0x0

    .line 168
    .local v0, "i":I
    const/16 v1, 0x80

    .line 169
    .local v1, "s":I
    :goto_3
    add-int/lit8 v2, v0, 0x1

    .end local v0    # "i":I
    .local v2, "i":I
    const/16 v3, 0xffe

    if-ge v0, v3, :cond_14

    .line 170
    rem-int/lit8 v0, v2, 0x2

    if-nez v0, :cond_12

    .line 171
    add-int/lit8 v0, v1, 0x1

    .end local v1    # "s":I
    .local v0, "s":I
    aput v1, p0, v2

    .line 169
    move v1, v0

    .end local v2    # "i":I
    .local v0, "i":I
    .restart local v1    # "s":I
    :cond_12
    move v0, v2

    goto :goto_3

    .line 174
    .end local v0    # "i":I
    .restart local v2    # "i":I
    :cond_14
    return-void
.end method


## CHECK-START: int PeelUnroll.unrollingWhileLiveOuts(int[]) loop_optimization (before)
## CHECK-DAG: <<Array:l\d+>>    ParameterValue                            loop:none
## CHECK-DAG: <<Const0:i\d+>>   IntConstant 0                             loop:none
## CHECK-DAG: <<Const1:i\d+>>   IntConstant 1                             loop:none
## CHECK-DAG: <<Const2:i\d+>>   IntConstant 2                             loop:none
## CHECK-DAG: <<Const128:i\d+>> IntConstant 128                           loop:none
## CHECK-DAG: <<Limit:i\d+>>    IntConstant 4094                          loop:none
## CHECK-DAG: <<PhiI:i\d+>>     Phi [<<Const0>>,{{i\d+}}]                 loop:<<Loop:B\d+>> outer_loop:none
## CHECK-DAG: <<PhiS:i\d+>>     Phi [<<Const128>>,{{i\d+}}]               loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<AddI:i\d+>>     Add [<<PhiI>>,<<Const1>>]                 loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<Check:z\d+>>    GreaterThanOrEqual [<<PhiI>>,<<Limit>>]   loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<If:v\d+>>       If [<<Check>>]                            loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<Rem:i\d+>>      Rem [<<AddI>>,<<Const2>>]                 loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<NE:z\d+>>       NotEqual [<<Rem>>,<<Const0>>]             loop:<<Loop>>      outer_loop:none
## CHECK-DAG:                   If [<<NE>>]                               loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<AddS:i\d+>>     Add [<<PhiS>>,<<Const1>>]                 loop:<<Loop>>      outer_loop:none
## CHECK-DAG:                   ArraySet                                  loop:<<Loop>>      outer_loop:none
## CHECK-DAG:                   Phi [<<PhiS>>,<<AddS>>]                   loop:<<Loop>>      outer_loop:none

## CHECK-NOT:                   ArrayGet
## CHECK-NOT:                   ArraySet

## CHECK-START: int PeelUnroll.unrollingWhileLiveOuts(int[]) loop_optimization (after)
## CHECK-DAG: <<Array:l\d+>>    ParameterValue                            loop:none
## CHECK-DAG: <<Const0:i\d+>>   IntConstant 0                             loop:none
## CHECK-DAG: <<Const1:i\d+>>   IntConstant 1                             loop:none
## CHECK-DAG: <<Const2:i\d+>>   IntConstant 2                             loop:none
## CHECK-DAG: <<Const128:i\d+>> IntConstant 128                           loop:none
## CHECK-DAG: <<Limit:i\d+>>    IntConstant 4094                          loop:none
## CHECK-DAG: <<PhiI:i\d+>>     Phi [<<Const0>>,{{i\d+}}]                 loop:<<Loop:B\d+>> outer_loop:none
## CHECK-DAG: <<PhiS:i\d+>>     Phi [<<Const128>>,{{i\d+}}]               loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<AddI:i\d+>>     Add [<<PhiI>>,<<Const1>>]                 loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<Check:z\d+>>    GreaterThanOrEqual [<<PhiI>>,<<Limit>>]   loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<If:v\d+>>       If [<<Check>>]                            loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<Rem:i\d+>>      Rem [<<AddI>>,<<Const2>>]                 loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<NE:z\d+>>       NotEqual [<<Rem>>,<<Const0>>]             loop:<<Loop>>      outer_loop:none
## CHECK-DAG:                   If [<<NE>>]                               loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<AddS:i\d+>>     Add [<<PhiS>>,<<Const1>>]                 loop:<<Loop>>      outer_loop:none
## CHECK-DAG:                   ArraySet [{{l\d+}},{{i\d+}},<<PhiS>>]     loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<PhiSM:i\d+>>    Phi [<<PhiS>>,<<AddS>>]                   loop:<<Loop>>      outer_loop:none

## CHECK-DAG: <<AddIA:i\d+>>    Add [<<AddI>>,<<Const1>>]                 loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<CheckA:z\d+>>   GreaterThanOrEqual [<<AddI>>,<<Limit>>]   loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<IfA:v\d+>>      If [<<Const0>>]                           loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<RemA:i\d+>>     Rem [<<AddIA>>,<<Const2>>]                loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<NEA:z\d+>>      NotEqual [<<RemA>>,<<Const0>>]            loop:<<Loop>>      outer_loop:none
## CHECK-DAG:                   If [<<NEA>>]                              loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<AddSA:i\d+>>    Add [<<PhiSM>>,<<Const1>>]                loop:<<Loop>>      outer_loop:none
## CHECK-DAG:                   ArraySet [{{l\d+}},{{i\d+}},<<PhiSM>>]    loop:<<Loop>>      outer_loop:none
## CHECK-DAG:                   Phi [<<AddSA>>,<<PhiSM>>]                 loop:<<Loop>>      outer_loop:none

## CHECK-DAG: <<RetPhi:i\d+>>   Phi [<<PhiS>>,<<PhiSM>>]                  loop:none
## CHECK-DAG:                   Return [<<RetPhi>>]                       loop:none

## CHECK-NOT:                   ArrayGet
## CHECK-NOT:                   ArraySet
.method public static final unrollingWhileLiveOuts([I)I
    .registers 5
    .param p0, "a"    # [I

    .line 598
    const/4 v0, 0x0

    .line 599
    .local v0, "i":I
    const/16 v1, 0x80

    .line 600
    .local v1, "s":I
    :goto_3
    add-int/lit8 v2, v0, 0x1

    .end local v0    # "i":I
    .local v2, "i":I
    const/16 v3, 0xffe

    if-ge v0, v3, :cond_14

    .line 601
    rem-int/lit8 v0, v2, 0x2

    if-nez v0, :cond_12

    .line 602
    add-int/lit8 v0, v1, 0x1

    .end local v1    # "s":I
    .local v0, "s":I
    aput v1, p0, v2

    .line 600
    move v1, v0

    .end local v2    # "i":I
    .local v0, "i":I
    .restart local v1    # "s":I
    :cond_12
    move v0, v2

    goto :goto_3

    .line 605
    .end local v0    # "i":I
    .restart local v2    # "i":I
    :cond_14
    return v1
.end method

