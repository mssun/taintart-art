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

.class public LSmaliTests2;
.super Ljava/lang/Object;

## CHECK-START: int SmaliTests2.$noinline$XorAllOnes(int) instruction_simplifier (before)
## CHECK-DAG:     <<Arg:i\d+>>      ParameterValue
## CHECK-DAG:     <<ConstF:i\d+>>   IntConstant -1
## CHECK-DAG:     <<Xor:i\d+>>      Xor [<<Arg>>,<<ConstF>>]
## CHECK-DAG:                       Return [<<Xor>>]

## CHECK-START: int SmaliTests2.$noinline$XorAllOnes(int) instruction_simplifier (after)
## CHECK-DAG:     <<Arg:i\d+>>      ParameterValue
## CHECK-DAG:     <<Not:i\d+>>      Not [<<Arg>>]
## CHECK-DAG:                       Return [<<Not>>]

## CHECK-START: int SmaliTests2.$noinline$XorAllOnes(int) instruction_simplifier (after)
## CHECK-NOT:                       Xor

# Original java source:
#
#     return arg ^ -1;
#
.method public static $noinline$XorAllOnes(I)I
    .registers 2
    .param p0, "arg"    # I

    .prologue
    .line 658
    xor-int/lit8 v0, p0, -0x1

    return v0
.end method

# Test simplification of the `~~var` pattern.
# The transformation tested is implemented in `InstructionSimplifierVisitor::VisitNot`.

## CHECK-START: long SmaliTests2.$noinline$NotNot1(long) instruction_simplifier (before)
## CHECK-DAG:     <<Arg:j\d+>>       ParameterValue
## CHECK-DAG:     <<ConstNeg1:j\d+>> LongConstant -1
## CHECK-DAG:     <<Not1:j\d+>>      Xor [<<Arg>>,<<ConstNeg1>>]
## CHECK-DAG:     <<Not2:j\d+>>      Xor [<<Not1>>,<<ConstNeg1>>]
## CHECK-DAG:                        Return [<<Not2>>]

## CHECK-START: long SmaliTests2.$noinline$NotNot1(long) instruction_simplifier (after)
## CHECK-DAG:     <<Arg:j\d+>>      ParameterValue
## CHECK-DAG:                       Return [<<Arg>>]

## CHECK-START: long SmaliTests2.$noinline$NotNot1(long) instruction_simplifier (after)
## CHECK-NOT:                       Xor

# Original java source:
#
#     return ~~arg;
.method public static $noinline$NotNot1(J)J
    .registers 6
    .param p0, "arg"    # J

    .prologue
    const-wide/16 v2, -0x1

    .line 1001
    xor-long v0, p0, v2

    xor-long/2addr v0, v2

    return-wide v0
.end method

## CHECK-START: int SmaliTests2.$noinline$NotNot2(int) instruction_simplifier (before)
## CHECK-DAG:     <<Arg:i\d+>>       ParameterValue
## CHECK-DAG:     <<ConstNeg1:i\d+>> IntConstant -1
## CHECK-DAG:     <<Not1:i\d+>>      Xor [<<Arg>>,<<ConstNeg1>>]
## CHECK-DAG:     <<Not2:i\d+>>      Xor [<<Not1>>,<<ConstNeg1>>]
## CHECK-DAG:     <<Add:i\d+>>       Add [<<Not2>>,<<Not1>>]
## CHECK-DAG:                        Return [<<Add>>]

## CHECK-START: int SmaliTests2.$noinline$NotNot2(int) instruction_simplifier (after)
## CHECK-DAG:     <<Arg:i\d+>>      ParameterValue
## CHECK-DAG:     <<Not:i\d+>>      Not [<<Arg>>]
## CHECK-DAG:     <<Add:i\d+>>      Add [<<Arg>>,<<Not>>]
## CHECK-DAG:                       Return [<<Add>>]

## CHECK-START: int SmaliTests2.$noinline$NotNot2(int) instruction_simplifier (after)
## CHECK:                           Not
## CHECK-NOT:                       Not

## CHECK-START: int SmaliTests2.$noinline$NotNot2(int) instruction_simplifier (after)
## CHECK-NOT:                       Xor

# Original java source:
#
#     int temp = ~arg;
#     return temp + ~temp;
#
.method public static $noinline$NotNot2(I)I
    .registers 3
    .param p0, "arg"    # I

    .prologue
    .line 1026
    xor-int/lit8 v0, p0, -0x1

    .line 1027
    .local v0, "temp":I
    xor-int/lit8 v1, v0, -0x1

    add-int/2addr v1, v0

    return v1
.end method

# Original java source:
#
#     return !arg;
#
.method public static NegateValue(Z)Z
    .registers 2
    .param p0, "arg"    # Z

    .prologue
    .line 1216
    if-nez p0, :cond_4

    const/4 v0, 0x1

    :goto_3
    return v0

    :cond_4
    const/4 v0, 0x0

    goto :goto_3
.end method

# Test simplification of double Boolean negation. Note that sometimes
# both negations can be removed but we only expect the simplifier to
# remove the second.

## CHECK-START: boolean SmaliTests2.$noinline$NotNotBool(boolean) instruction_simplifier (before)
## CHECK-DAG:     <<Arg:z\d+>>       ParameterValue
## CHECK-DAG:     <<Const1:i\d+>>    IntConstant 0
## CHECK-DAG:     <<Result:z\d+>>    InvokeStaticOrDirect method_name:SmaliTests2.NegateValue
## CHECK-DAG:     <<NotResult:z\d+>> NotEqual [<<Result>>,<<Const1>>]
## CHECK-DAG:                        If [<<NotResult>>]

## CHECK-START: boolean SmaliTests2.$noinline$NotNotBool(boolean) instruction_simplifier (after)
## CHECK-NOT:                        NotEqual

## CHECK-START: boolean SmaliTests2.$noinline$NotNotBool(boolean) instruction_simplifier (after)
## CHECK-DAG:     <<Arg:z\d+>>       ParameterValue
## CHECK-DAG:     <<Result:z\d+>>    InvokeStaticOrDirect method_name:SmaliTests2.NegateValue
## CHECK-DAG:     <<Const0:i\d+>>    IntConstant 0
## CHECK-DAG:     <<Const1:i\d+>>    IntConstant 1
## CHECK-DAG:     <<Phi:i\d+>>       Phi [<<Const1>>,<<Const0>>]
## CHECK-DAG:                        Return [<<Phi>>]

## CHECK-START: boolean SmaliTests2.$noinline$NotNotBool(boolean) instruction_simplifier$after_inlining (before)
## CHECK-DAG:     <<Arg:z\d+>>       ParameterValue
## CHECK-DAG:     <<Const0:i\d+>>    IntConstant 0
## CHECK-DAG:     <<Const1:i\d+>>    IntConstant 1
## CHECK-DAG:                        If [<<Arg>>]
## CHECK-DAG:     <<Phi:i\d+>>       Phi [<<Const1>>,<<Const0>>]
## CHECK-DAG:                        Return [<<Phi>>]

## CHECK-START: boolean SmaliTests2.$noinline$NotNotBool(boolean) instruction_simplifier$after_gvn (after)
## CHECK-DAG:     <<Arg:z\d+>>       ParameterValue
## CHECK-DAG:                        Return [<<Arg>>]

# Original java source:
#
#     return !(NegateValue(arg));
#
.method public static $noinline$NotNotBool(Z)Z
    .registers 2
    .param p0, "arg"    # Z

    .prologue
    .line 1220
    invoke-static {p0}, LSmaliTests2;->NegateValue(Z)Z

    move-result v0

    if-nez v0, :cond_8

    const/4 v0, 0x1

    :goto_7
    return v0

    :cond_8
    const/4 v0, 0x0

    goto :goto_7
.end method

## CHECK-START: int SmaliTests2.$noinline$bug68142795Short(short) instruction_simplifier (before)
## CHECK-DAG:      <<Arg:s\d+>>      ParameterValue
## CHECK-DAG:      <<Const:i\d+>>    IntConstant 65535
## CHECK-DAG:      <<And1:i\d+>>     And [<<Arg>>,<<Const>>]
## CHECK-DAG:      <<And2:i\d+>>     And [<<And1>>,<<Const>>]
## CHECK-DAG:      <<Conv:s\d+>>     TypeConversion [<<And2>>]
## CHECK-DAG:                        Return [<<Conv>>]

## CHECK-START: int SmaliTests2.$noinline$bug68142795Short(short) instruction_simplifier (after)
## CHECK-DAG:      <<Arg:s\d+>>      ParameterValue
## CHECK-DAG:                        Return [<<Arg>>]

# Original java source
#
#     return (short)(0xffff & (s & 0xffff));
#
.method public static $noinline$bug68142795Short(S)I
    .registers 3
    .param p0, "s"    # S

    .prologue
    const v1, 0xffff

    .line 2562
    and-int v0, p0, v1

    and-int/2addr v0, v1

    int-to-short v0, v0

    return v0
.end method

# Original java source
#
#     return 255;
#
.method private static $inline$get255()I
    .registers 1

    .prologue
    .line 2849
    const/16 v0, 0xff

    return v0
.end method

## CHECK-START: int SmaliTests2.$noinline$bug68142795Boolean(boolean) instruction_simplifier$after_inlining (before)
## CHECK-DAG:      <<Arg:z\d+>>      ParameterValue
## CHECK-DAG:      <<Const0:i\d+>>   IntConstant 0
## CHECK-DAG:      <<Const1:i\d+>>   IntConstant 1
## CHECK-DAG:      <<Const255:i\d+>> IntConstant 255
## CHECK-DAG:                        If [<<Arg>>]
## CHECK-DAG:      <<Phi:i\d+>>      Phi [<<Const1>>,<<Const0>>]
## CHECK-DAG:      <<And:i\d+>>      And [<<Const255>>,<<Phi>>]
## CHECK-DAG:      <<Conv:b\d+>>     TypeConversion [<<And>>]
## CHECK-DAG:                        Return [<<Conv>>]

## CHECK-START: int SmaliTests2.$noinline$bug68142795Boolean(boolean) instruction_simplifier$after_gvn (after)
## CHECK-DAG:      <<Arg:z\d+>>      ParameterValue
## CHECK-DAG:                        Return [<<Arg>>]

# Original java source
#
#     int v = b ? 1 : 0;  // Should be simplified to "b" after inlining.
#     return (byte)($inline$get255() & v);
#
.method public static $noinline$bug68142795Boolean(Z)I
    .registers 3
    .param p0, "b"    # Z

    .prologue
    .line 2580
    if-eqz p0, :cond_a

    const/4 v0, 0x1

    .line 2581
    .local v0, "v":I
    :goto_3
    invoke-static {}, LSmaliTests2;->$inline$get255()I

    move-result v1

    and-int/2addr v1, v0

    int-to-byte v1, v1

    return v1

    .line 2580
    .end local v0    # "v":I
    :cond_a
    const/4 v0, 0x0

    goto :goto_3
.end method
