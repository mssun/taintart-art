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

## CHECK-START: int Main2.rotateLeftBoolean(boolean, int) builder (after)
## CHECK:         <<ArgVal:z\d+>>  ParameterValue
## CHECK:         <<ArgDist:i\d+>> ParameterValue
## CHECK-DAG:     <<Zero:i\d+>>    IntConstant 0
## CHECK-DAG:     <<One:i\d+>>     IntConstant 1
## CHECK-DAG:     <<Val:i\d+>>     Phi [<<One>>,<<Zero>>]
## CHECK-DAG:     <<Result:i\d+>>  InvokeStaticOrDirect [<<Val>>,<<ArgDist>>{{(,[ij]\d+)?}}] intrinsic:IntegerRotateLeft
## CHECK-DAG:                      Return [<<Result>>]

## CHECK-START: int Main2.rotateLeftBoolean(boolean, int) instruction_simplifier (after)
## CHECK:         <<ArgVal:z\d+>>  ParameterValue
## CHECK:         <<ArgDist:i\d+>> ParameterValue
## CHECK-DAG:     <<Zero:i\d+>>    IntConstant 0
## CHECK-DAG:     <<One:i\d+>>     IntConstant 1
## CHECK-DAG:     <<Val:i\d+>>     Phi [<<One>>,<<Zero>>]
## CHECK-DAG:     <<NegDist:i\d+>> Neg [<<ArgDist>>]
## CHECK-DAG:     <<Result:i\d+>>  Ror [<<Val>>,<<NegDist>>]
## CHECK-DAG:                      Return [<<Result>>]

## CHECK-START: int Main2.rotateLeftBoolean(boolean, int) instruction_simplifier (after)
## CHECK-NOT:                      InvokeStaticOrDirect

## CHECK-START: int Main2.rotateLeftBoolean(boolean, int) select_generator (after)
## CHECK:         <<ArgVal:z\d+>>  ParameterValue
## CHECK:         <<ArgDist:i\d+>> ParameterValue
## CHECK-DAG:     <<Zero:i\d+>>    IntConstant 0
## CHECK-DAG:     <<One:i\d+>>     IntConstant 1
## CHECK-DAG:     <<SelVal:i\d+>>  Select [<<Zero>>,<<One>>,<<ArgVal>>]
## CHECK-DAG:     <<NegDist:i\d+>> Neg [<<ArgDist>>]
## CHECK-DAG:     <<Result:i\d+>>  Ror [<<SelVal>>,<<NegDist>>]
## CHECK-DAG:                      Return [<<Result>>]

## CHECK-START: int Main2.rotateLeftBoolean(boolean, int) select_generator (after)
## CHECK-NOT:                      Phi

## CHECK-START: int Main2.rotateLeftBoolean(boolean, int) instruction_simplifier$after_bce (after)
## CHECK:         <<ArgVal:z\d+>>  ParameterValue
## CHECK:         <<ArgDist:i\d+>> ParameterValue
## CHECK-DAG:     <<NegDist:i\d+>> Neg [<<ArgDist>>]
## CHECK-DAG:     <<Result:i\d+>>  Ror [<<ArgVal>>,<<NegDist>>]
## CHECK-DAG:                      Return [<<Result>>]

## CHECK-START: int Main2.rotateLeftBoolean(boolean, int) instruction_simplifier$after_bce (after)
## CHECK-NOT:                      Select

# Original java source
#
#     private static int rotateLeftBoolean(boolean value, int distance) {
#       return Integer.rotateLeft(value ? 1 : 0, distance);
#     }

.method public static rotateLeftBoolean(ZI)I
    .registers 3
    .param p0, "value"    # Z
    .param p1, "distance"    # I

    .prologue
    .line 66
    if-eqz p0, :cond_8

    const/4 v0, 0x1

    :goto_3
    invoke-static {v0, p1}, Ljava/lang/Integer;->rotateLeft(II)I

    move-result v0

    return v0

    :cond_8
    const/4 v0, 0x0

    goto :goto_3
.end method

## CHECK-START: int Main2.rotateRightBoolean(boolean, int) builder (after)
## CHECK:         <<ArgVal:z\d+>>  ParameterValue
## CHECK:         <<ArgDist:i\d+>> ParameterValue
## CHECK-DAG:     <<Zero:i\d+>>    IntConstant 0
## CHECK-DAG:     <<One:i\d+>>     IntConstant 1
## CHECK-DAG:     <<Val:i\d+>>     Phi [<<One>>,<<Zero>>]
## CHECK-DAG:     <<Result:i\d+>>  InvokeStaticOrDirect [<<Val>>,<<ArgDist>>{{(,[ij]\d+)?}}] intrinsic:IntegerRotateRight
## CHECK-DAG:                      Return [<<Result>>]

## CHECK-START: int Main2.rotateRightBoolean(boolean, int) instruction_simplifier (after)
## CHECK:         <<ArgVal:z\d+>>  ParameterValue
## CHECK:         <<ArgDist:i\d+>> ParameterValue
## CHECK-DAG:     <<Zero:i\d+>>    IntConstant 0
## CHECK-DAG:     <<One:i\d+>>     IntConstant 1
## CHECK-DAG:     <<Val:i\d+>>     Phi [<<One>>,<<Zero>>]
## CHECK-DAG:     <<Result:i\d+>>  Ror [<<Val>>,<<ArgDist>>]
## CHECK-DAG:                      Return [<<Result>>]

## CHECK-START: int Main2.rotateRightBoolean(boolean, int) instruction_simplifier (after)
## CHECK-NOT:                      InvokeStaticOrDirect

## CHECK-START: int Main2.rotateRightBoolean(boolean, int) select_generator (after)
## CHECK:         <<ArgVal:z\d+>>  ParameterValue
## CHECK:         <<ArgDist:i\d+>> ParameterValue
## CHECK-DAG:     <<Zero:i\d+>>    IntConstant 0
## CHECK-DAG:     <<One:i\d+>>     IntConstant 1
## CHECK-DAG:     <<SelVal:i\d+>>  Select [<<Zero>>,<<One>>,<<ArgVal>>]
## CHECK-DAG:     <<Result:i\d+>>  Ror [<<SelVal>>,<<ArgDist>>]
## CHECK-DAG:                      Return [<<Result>>]

## CHECK-START: int Main2.rotateRightBoolean(boolean, int) select_generator (after)
## CHECK-NOT:                     Phi

## CHECK-START: int Main2.rotateRightBoolean(boolean, int) instruction_simplifier$after_bce (after)
## CHECK:         <<ArgVal:z\d+>>  ParameterValue
## CHECK:         <<ArgDist:i\d+>> ParameterValue
## CHECK-DAG:     <<Result:i\d+>>  Ror [<<ArgVal>>,<<ArgDist>>]
## CHECK-DAG:                      Return [<<Result>>]

## CHECK-START: int Main2.rotateRightBoolean(boolean, int) instruction_simplifier$after_bce (after)
## CHECK-NOT:                     Select

# Original java source:
#
#     private static int rotateRightBoolean(boolean value, int distance) {
#       return Integer.rotateRight(value ? 1 : 0, distance);
#     }

.method public static rotateRightBoolean(ZI)I
    .registers 3
    .param p0, "value"    # Z
    .param p1, "distance"    # I

    .prologue
    .line 219
    if-eqz p0, :cond_8

    const/4 v0, 0x1

    :goto_3
    invoke-static {v0, p1}, Ljava/lang/Integer;->rotateRight(II)I

    move-result v0

    return v0

    :cond_8
    const/4 v0, 0x0

    goto :goto_3
.end method
