# Copyright (C) 2016 The Android Open Source Project
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

## CHECK-START: int Main2.signBoolean(boolean) builder (after)
## CHECK-DAG:     <<Zero:i\d+>>   IntConstant 0
## CHECK-DAG:     <<One:i\d+>>    IntConstant 1
## CHECK-DAG:     <<Phi:i\d+>>    Phi [<<One>>,<<Zero>>]
## CHECK-DAG:     <<Result:i\d+>> InvokeStaticOrDirect [<<Phi>>{{(,[ij]\d+)?}}] intrinsic:IntegerSignum
## CHECK-DAG:                     Return [<<Result>>]

## CHECK-START: int Main2.signBoolean(boolean) instruction_simplifier (after)
## CHECK-DAG:     <<Zero:i\d+>>   IntConstant 0
## CHECK-DAG:     <<One:i\d+>>    IntConstant 1
## CHECK-DAG:     <<Phi:i\d+>>    Phi [<<One>>,<<Zero>>]
## CHECK-DAG:     <<Result:i\d+>> Compare [<<Phi>>,<<Zero>>]
## CHECK-DAG:                     Return [<<Result>>]

## CHECK-START: int Main2.signBoolean(boolean) instruction_simplifier (after)
## CHECK-NOT:                     InvokeStaticOrDirect

## CHECK-START: int Main2.signBoolean(boolean) select_generator (after)
## CHECK-DAG:     <<Arg:z\d+>>    ParameterValue
## CHECK-DAG:     <<Zero:i\d+>>   IntConstant 0
## CHECK-DAG:     <<One:i\d+>>    IntConstant 1
## CHECK-DAG:     <<Sel:i\d+>>    Select [<<Zero>>,<<One>>,<<Arg>>]
## CHECK-DAG:     <<Result:i\d+>> Compare [<<Sel>>,<<Zero>>]
## CHECK-DAG:                     Return [<<Result>>]

## CHECK-START: int Main2.signBoolean(boolean) select_generator (after)
## CHECK-NOT:                     Phi

## CHECK-START: int Main2.signBoolean(boolean) instruction_simplifier$after_bce (after)
## CHECK-DAG:     <<Arg:z\d+>>    ParameterValue
## CHECK-DAG:     <<Zero:i\d+>>   IntConstant 0
## CHECK-DAG:     <<Result:i\d+>> Compare [<<Arg>>,<<Zero>>]
## CHECK-DAG:                     Return [<<Result>>]

## CHECK-START: int Main2.signBoolean(boolean) instruction_simplifier$after_bce (after)
## CHECK-NOT:                     Select

# Original java source:
#
#     private static int signBoolean(boolean x) {
#       return Integer.signum(x ? 1 : 0);
#     }

.method public static signBoolean(Z)I
    .registers 2
    .param p0, "x"    # Z

    .prologue
    .line 58
    if-eqz p0, :cond_8

    const/4 v0, 0x1

    :goto_3
    invoke-static {v0}, Ljava/lang/Integer;->signum(I)I

    move-result v0

    return v0

    :cond_8
    const/4 v0, 0x0

    goto :goto_3
.end method
