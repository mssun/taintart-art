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

.class public LSmaliTests;
.super Ljava/lang/Object;

#
# Test transformation of Not/Not/And into Or/Not.
#

## CHECK-START: int SmaliTests.$opt$noinline$andToOr(int, int) instruction_simplifier (before)
## CHECK-DAG:       <<P1:i\d+>>          ParameterValue
## CHECK-DAG:       <<P2:i\d+>>          ParameterValue
## CHECK-DAG:       <<Not1:i\d+>>        Not [<<P1>>]
## CHECK-DAG:       <<Not2:i\d+>>        Not [<<P2>>]
## CHECK-DAG:       <<And:i\d+>>         And [<<Not1>>,<<Not2>>]
## CHECK-DAG:                            Return [<<And>>]

## CHECK-START: int SmaliTests.$opt$noinline$andToOr(int, int) instruction_simplifier (after)
## CHECK-DAG:       <<P1:i\d+>>          ParameterValue
## CHECK-DAG:       <<P2:i\d+>>          ParameterValue
## CHECK-DAG:       <<Or:i\d+>>          Or [<<P1>>,<<P2>>]
## CHECK-DAG:       <<Not:i\d+>>         Not [<<Or>>]
## CHECK-DAG:                            Return [<<Not>>]

## CHECK-START: int SmaliTests.$opt$noinline$andToOr(int, int) instruction_simplifier (after)
## CHECK-DAG:                            Not
## CHECK-NOT:                            Not

## CHECK-START: int SmaliTests.$opt$noinline$andToOr(int, int) instruction_simplifier (after)
## CHECK-NOT:                            And
.method public static $opt$noinline$andToOr(II)I
    .registers 4
    .param p0, "a"    # I
    .param p1, "b"    # I

    .prologue
    sget-boolean v0, LSmaliTests;->doThrow:Z
    if-eqz v0, :cond_a
    new-instance v0, Ljava/lang/Error;
    invoke-direct {v0}, Ljava/lang/Error;-><init>()V
    throw v0

  :cond_a
    # return ~a & ~b;
    not-int v0, p0
    not-int v1, p1
    and-int/2addr v0, v1

    return v0
.end method

# Test transformation of Not/Not/And into Or/Not for boolean negations.
# Note that the graph before this instruction simplification pass does not
# contain `HBooleanNot` instructions. This is because this transformation
# follows the optimization of `HSelect` to `HBooleanNot` occurring in the
# same pass.

## CHECK-START: boolean SmaliTests.$opt$noinline$booleanAndToOr(boolean, boolean) instruction_simplifier (before)
## CHECK-DAG:       <<P1:z\d+>>          ParameterValue
## CHECK-DAG:       <<P2:z\d+>>          ParameterValue
## CHECK-DAG:       <<Const1:i\d+>>      IntConstant 1
## CHECK-DAG:       <<NotP1:i\d+>>       Xor [<<P1>>,<<Const1>>]
## CHECK-DAG:       <<NotP2:i\d+>>       Xor [<<P2>>,<<Const1>>]
## CHECK-DAG:       <<And:i\d+>>         And [<<NotP1>>,<<NotP2>>]
## CHECK-DAG:                            Return [<<And>>]

## CHECK-START: boolean SmaliTests.$opt$noinline$booleanAndToOr(boolean, boolean) instruction_simplifier (after)
## CHECK-DAG:       <<Cond1:z\d+>>       ParameterValue
## CHECK-DAG:       <<Cond2:z\d+>>       ParameterValue
## CHECK-DAG:       <<Or:i\d+>>          Or [<<Cond1>>,<<Cond2>>]
## CHECK-DAG:       <<BooleanNot:z\d+>>  BooleanNot [<<Or>>]
## CHECK-DAG:                            Return [<<BooleanNot>>]

## CHECK-START: boolean SmaliTests.$opt$noinline$booleanAndToOr(boolean, boolean) instruction_simplifier$after_bce (after)
## CHECK-DAG:                            BooleanNot
## CHECK-NOT:                            BooleanNot

## CHECK-START: boolean SmaliTests.$opt$noinline$booleanAndToOr(boolean, boolean) instruction_simplifier$after_bce (after)
## CHECK-NOT:                            And
.method public static $opt$noinline$booleanAndToOr(ZZ)Z
    .registers 4
    .param p0, "a"    # Z
    .param p1, "b"    # Z

    .prologue
    sget-boolean v0, LSmaliTests;->doThrow:Z
    if-eqz v0, :cond_a
    new-instance v0, Ljava/lang/Error;
    invoke-direct {v0}, Ljava/lang/Error;-><init>()V
    throw v0

  :cond_a
    # return !a & !b;
    xor-int/lit8 v0, p0, 0x1
    xor-int/lit8 v1, p1, 0x1
    and-int/2addr v0, v1
    return v0
.end method

# Test transformation of Not/Not/Or into And/Not.

## CHECK-START: long SmaliTests.$opt$noinline$orToAnd(long, long) instruction_simplifier (before)
## CHECK-DAG:       <<P1:j\d+>>          ParameterValue
## CHECK-DAG:       <<P2:j\d+>>          ParameterValue
## CHECK-DAG:       <<Not1:j\d+>>        Not [<<P1>>]
## CHECK-DAG:       <<Not2:j\d+>>        Not [<<P2>>]
## CHECK-DAG:       <<Or:j\d+>>          Or [<<Not1>>,<<Not2>>]
## CHECK-DAG:                            Return [<<Or>>]

## CHECK-START: long SmaliTests.$opt$noinline$orToAnd(long, long) instruction_simplifier (after)
## CHECK-DAG:       <<P1:j\d+>>          ParameterValue
## CHECK-DAG:       <<P2:j\d+>>          ParameterValue
## CHECK-DAG:       <<And:j\d+>>         And [<<P1>>,<<P2>>]
## CHECK-DAG:       <<Not:j\d+>>         Not [<<And>>]
## CHECK-DAG:                            Return [<<Not>>]

## CHECK-START: long SmaliTests.$opt$noinline$orToAnd(long, long) instruction_simplifier (after)
## CHECK-DAG:                            Not
## CHECK-NOT:                            Not

## CHECK-START: long SmaliTests.$opt$noinline$orToAnd(long, long) instruction_simplifier (after)
## CHECK-NOT:                            Or
.method public static $opt$noinline$orToAnd(JJ)J
    .registers 8
    .param p0, "a"    # J
    .param p2, "b"    # J

    .prologue
    sget-boolean v0, LSmaliTests;->doThrow:Z
    if-eqz v0, :cond_a
    new-instance v0, Ljava/lang/Error;
    invoke-direct {v0}, Ljava/lang/Error;-><init>()V
    throw v0

  :cond_a
    # return ~a | ~b;
    not-long v0, p0
    not-long v2, p2
    or-long/2addr v0, v2
    return-wide v0
.end method

# Test transformation of Not/Not/Or into Or/And for boolean negations.
# Note that the graph before this instruction simplification pass does not
# contain `HBooleanNot` instructions. This is because this transformation
# follows the optimization of `HSelect` to `HBooleanNot` occurring in the
# same pass.

## CHECK-START: boolean SmaliTests.$opt$noinline$booleanOrToAnd(boolean, boolean) instruction_simplifier (before)
## CHECK-DAG:       <<P1:z\d+>>          ParameterValue
## CHECK-DAG:       <<P2:z\d+>>          ParameterValue
## CHECK-DAG:       <<Const1:i\d+>>      IntConstant 1
## CHECK-DAG:       <<NotP1:i\d+>>       Xor [<<P1>>,<<Const1>>]
## CHECK-DAG:       <<NotP2:i\d+>>       Xor [<<P2>>,<<Const1>>]
## CHECK-DAG:       <<Or:i\d+>>          Or [<<NotP1>>,<<NotP2>>]
## CHECK-DAG:                            Return [<<Or>>]

## CHECK-START: boolean SmaliTests.$opt$noinline$booleanOrToAnd(boolean, boolean) instruction_simplifier (after)
## CHECK-DAG:       <<Cond1:z\d+>>       ParameterValue
## CHECK-DAG:       <<Cond2:z\d+>>       ParameterValue
## CHECK-DAG:       <<And:i\d+>>         And [<<Cond1>>,<<Cond2>>]
## CHECK-DAG:       <<BooleanNot:z\d+>>  BooleanNot [<<And>>]
## CHECK-DAG:                            Return [<<BooleanNot>>]

## CHECK-START: boolean SmaliTests.$opt$noinline$booleanOrToAnd(boolean, boolean) instruction_simplifier$after_bce (after)
## CHECK-DAG:                            BooleanNot
## CHECK-NOT:                            BooleanNot

## CHECK-START: boolean SmaliTests.$opt$noinline$booleanOrToAnd(boolean, boolean) instruction_simplifier$after_bce (after)
## CHECK-NOT:                            Or
.method public static $opt$noinline$booleanOrToAnd(ZZ)Z
    .registers 4
    .param p0, "a"    # Z
    .param p1, "b"    # Z

    .prologue
    sget-boolean v0, LSmaliTests;->doThrow:Z
    if-eqz v0, :cond_a
    new-instance v0, Ljava/lang/Error;
    invoke-direct {v0}, Ljava/lang/Error;-><init>()V
    throw v0

  :cond_a
    # return !a | !b;
    xor-int/lit8 v0, p0, 0x1
    xor-int/lit8 v1, p1, 0x1
    or-int/2addr v0, v1
    return v0
.end method

# Test that the transformation copes with inputs being separated from the
# bitwise operations.
# This is a regression test. The initial logic was inserting the new bitwise
# operation incorrectly.

## CHECK-START: int SmaliTests.$opt$noinline$regressInputsAway(int, int) instruction_simplifier (before)
## CHECK-DAG:       <<P1:i\d+>>          ParameterValue
## CHECK-DAG:       <<P2:i\d+>>          ParameterValue
## CHECK-DAG:       <<Cst1:i\d+>>        IntConstant 1
## CHECK-DAG:       <<AddP1:i\d+>>       Add [<<P1>>,<<Cst1>>]
## CHECK-DAG:       <<Not1:i\d+>>        Not [<<AddP1>>]
## CHECK-DAG:       <<AddP2:i\d+>>       Add [<<P2>>,<<Cst1>>]
## CHECK-DAG:       <<Not2:i\d+>>        Not [<<AddP2>>]
## CHECK-DAG:       <<Or:i\d+>>          Or [<<Not1>>,<<Not2>>]
## CHECK-DAG:                            Return [<<Or>>]

## CHECK-START: int SmaliTests.$opt$noinline$regressInputsAway(int, int) instruction_simplifier (after)
## CHECK-DAG:       <<P1:i\d+>>          ParameterValue
## CHECK-DAG:       <<P2:i\d+>>          ParameterValue
## CHECK-DAG:       <<Cst1:i\d+>>        IntConstant 1
## CHECK-DAG:       <<AddP1:i\d+>>       Add [<<P1>>,<<Cst1>>]
## CHECK-DAG:       <<AddP2:i\d+>>       Add [<<P2>>,<<Cst1>>]
## CHECK-DAG:       <<And:i\d+>>         And [<<AddP1>>,<<AddP2>>]
## CHECK-DAG:       <<Not:i\d+>>         Not [<<And>>]
## CHECK-DAG:                            Return [<<Not>>]

## CHECK-START: int SmaliTests.$opt$noinline$regressInputsAway(int, int) instruction_simplifier (after)
## CHECK-DAG:                            Not
## CHECK-NOT:                            Not

## CHECK-START: int SmaliTests.$opt$noinline$regressInputsAway(int, int) instruction_simplifier (after)
## CHECK-NOT:                            Or
.method public static $opt$noinline$regressInputsAway(II)I
    .registers 7
    .param p0, "a"    # I
    .param p1, "b"    # I

    .prologue
    sget-boolean v4, LSmaliTests;->doThrow:Z
    if-eqz v4, :cond_a
    new-instance v4, Ljava/lang/Error;
    invoke-direct {v4}, Ljava/lang/Error;-><init>()V
    throw v4

  :cond_a
    # int a1 = a + 1;
    add-int/lit8 v0, p0, 0x1
    # int not_a1 = ~a1;
    not-int v2, v0
    # int b1 = b + 1;
    add-int/lit8 v1, p1, 0x1
    # int not_b1 = ~b1;
    not-int v3, v1

    # return not_a1 | not_b1
    or-int v4, v2, v3
    return v4
.end method

# Test transformation of Not/Not/Xor into Xor.

# See first note above.
## CHECK-START: int SmaliTests.$opt$noinline$notXorToXor(int, int) instruction_simplifier (before)
## CHECK-DAG:       <<P1:i\d+>>          ParameterValue
## CHECK-DAG:       <<P2:i\d+>>          ParameterValue
## CHECK-DAG:       <<Not1:i\d+>>        Not [<<P1>>]
## CHECK-DAG:       <<Not2:i\d+>>        Not [<<P2>>]
## CHECK-DAG:       <<Xor:i\d+>>         Xor [<<Not1>>,<<Not2>>]
## CHECK-DAG:                            Return [<<Xor>>]

## CHECK-START: int SmaliTests.$opt$noinline$notXorToXor(int, int) instruction_simplifier (after)
## CHECK-DAG:       <<P1:i\d+>>          ParameterValue
## CHECK-DAG:       <<P2:i\d+>>          ParameterValue
## CHECK-DAG:       <<Xor:i\d+>>         Xor [<<P1>>,<<P2>>]
## CHECK-DAG:                            Return [<<Xor>>]

## CHECK-START: int SmaliTests.$opt$noinline$notXorToXor(int, int) instruction_simplifier (after)
## CHECK-NOT:                            Not
.method public static $opt$noinline$notXorToXor(II)I
    .registers 4
    .param p0, "a"    # I
    .param p1, "b"    # I

    .prologue
    sget-boolean v0, LSmaliTests;->doThrow:Z
    if-eqz v0, :cond_a
    new-instance v0, Ljava/lang/Error;
    invoke-direct {v0}, Ljava/lang/Error;-><init>()V
    throw v0

  :cond_a
    # return ~a ^ ~b;
    not-int v0, p0
    not-int v1, p1
    xor-int/2addr v0, v1
    return v0
.end method

# Test transformation of Not/Not/Xor into Xor for boolean negations.
# Note that the graph before this instruction simplification pass does not
# contain `HBooleanNot` instructions. This is because this transformation
# follows the optimization of `HSelect` to `HBooleanNot` occurring in the
# same pass.

## CHECK-START: boolean SmaliTests.$opt$noinline$booleanNotXorToXor(boolean, boolean) instruction_simplifier (before)
## CHECK-DAG:       <<P1:z\d+>>          ParameterValue
## CHECK-DAG:       <<P2:z\d+>>          ParameterValue
## CHECK-DAG:       <<Const1:i\d+>>      IntConstant 1
## CHECK-DAG:       <<NotP1:i\d+>>       Xor [<<P1>>,<<Const1>>]
## CHECK-DAG:       <<NotP2:i\d+>>       Xor [<<P2>>,<<Const1>>]
## CHECK-DAG:       <<Xor:i\d+>>         Xor [<<NotP1>>,<<NotP2>>]
## CHECK-DAG:                            Return [<<Xor>>]

## CHECK-START: boolean SmaliTests.$opt$noinline$booleanNotXorToXor(boolean, boolean) instruction_simplifier (after)
## CHECK-DAG:       <<Cond1:z\d+>>       ParameterValue
## CHECK-DAG:       <<Cond2:z\d+>>       ParameterValue
## CHECK-DAG:       <<Xor:i\d+>>         Xor [<<Cond1>>,<<Cond2>>]
## CHECK-DAG:                            Return [<<Xor>>]

## CHECK-START: boolean SmaliTests.$opt$noinline$booleanNotXorToXor(boolean, boolean) instruction_simplifier$after_bce (after)
## CHECK-NOT:                            BooleanNot
.method public static $opt$noinline$booleanNotXorToXor(ZZ)Z
    .registers 4
    .param p0, "a"    # Z
    .param p1, "b"    # Z

    .prologue
    sget-boolean v0, LSmaliTests;->doThrow:Z
    if-eqz v0, :cond_a
    new-instance v0, Ljava/lang/Error;
    invoke-direct {v0}, Ljava/lang/Error;-><init>()V
    throw v0

  :cond_a
    # return !a ^ !b;
    xor-int/lit8 v0, p0, 0x1
    xor-int/lit8 v1, p1, 0x1
    xor-int/2addr v0, v1
    return v0
.end method

# Check that no transformation is done when one Not has multiple uses.

## CHECK-START: int SmaliTests.$opt$noinline$notMultipleUses(int, int) instruction_simplifier (before)
## CHECK-DAG:       <<P1:i\d+>>          ParameterValue
## CHECK-DAG:       <<P2:i\d+>>          ParameterValue
## CHECK-DAG:       <<One:i\d+>>         IntConstant 1
## CHECK-DAG:       <<Not2:i\d+>>        Not [<<P2>>]
## CHECK-DAG:       <<And2:i\d+>>        And [<<Not2>>,<<One>>]
## CHECK-DAG:       <<Not1:i\d+>>        Not [<<P1>>]
## CHECK-DAG:       <<And1:i\d+>>        And [<<Not1>>,<<Not2>>]
## CHECK-DAG:       <<Add:i\d+>>         Add [<<And2>>,<<And1>>]
## CHECK-DAG:                            Return [<<Add>>]

## CHECK-START: int SmaliTests.$opt$noinline$notMultipleUses(int, int) instruction_simplifier (after)
## CHECK-DAG:       <<P1:i\d+>>          ParameterValue
## CHECK-DAG:       <<P2:i\d+>>          ParameterValue
## CHECK-DAG:       <<One:i\d+>>         IntConstant 1
## CHECK-DAG:       <<Not2:i\d+>>        Not [<<P2>>]
## CHECK-DAG:       <<And2:i\d+>>        And [<<Not2>>,<<One>>]
## CHECK-DAG:       <<Not1:i\d+>>        Not [<<P1>>]
## CHECK-DAG:       <<And1:i\d+>>        And [<<Not1>>,<<Not2>>]
## CHECK-DAG:       <<Add:i\d+>>         Add [<<And2>>,<<And1>>]
## CHECK-DAG:                            Return [<<Add>>]

## CHECK-START: int SmaliTests.$opt$noinline$notMultipleUses(int, int) instruction_simplifier (after)
## CHECK-NOT:                            Or
.method public static $opt$noinline$notMultipleUses(II)I
    .registers 5
    .param p0, "a"    # I
    .param p1, "b"    # I

    .prologue
    sget-boolean v1, LSmaliTests;->doThrow:Z
    if-eqz v1, :cond_a
    new-instance v1, Ljava/lang/Error;
    invoke-direct {v1}, Ljava/lang/Error;-><init>()V
    throw v1

  :cond_a
    # int tmp = ~b;
    not-int v0, p1
    # return (tmp & 0x1) + (~a & tmp);
    and-int/lit8 v1, v0, 0x1
    not-int v2, p0
    and-int/2addr v2, v0
    add-int/2addr v1, v2
    return v1
.end method

# static fields
.field static doThrow:Z # boolean

# direct methods
.method static constructor <clinit>()V
    .registers 1

    .prologue
    # doThrow = false
    const/4 v0, 0x0
    sput-boolean v0, LSmaliTests;->doThrow:Z
    return-void
.end method


# Test transformation of Not/Not/And into Or/Not.

# Note: before the instruction_simplifier pass, Xor's are used instead of
# Not's (the simplification happens during the same pass).
## CHECK-START: int SmaliTests.$opt$noinline$andToOrV2(int, int) instruction_simplifier (before)
## CHECK-DAG:       <<P1:i\d+>>          ParameterValue
## CHECK-DAG:       <<P2:i\d+>>          ParameterValue
## CHECK-DAG:       <<CstM1:i\d+>>       IntConstant -1
## CHECK-DAG:       <<Not1:i\d+>>        Xor [<<P1>>,<<CstM1>>]
## CHECK-DAG:       <<Not2:i\d+>>        Xor [<<P2>>,<<CstM1>>]
## CHECK-DAG:       <<And:i\d+>>         And [<<Not1>>,<<Not2>>]
## CHECK-DAG:                            Return [<<And>>]

## CHECK-START: int SmaliTests.$opt$noinline$andToOrV2(int, int) instruction_simplifier (after)
## CHECK-DAG:       <<P1:i\d+>>          ParameterValue
## CHECK-DAG:       <<P2:i\d+>>          ParameterValue
## CHECK-DAG:       <<Or:i\d+>>          Or [<<P1>>,<<P2>>]
## CHECK-DAG:       <<Not:i\d+>>         Not [<<Or>>]
## CHECK-DAG:                            Return [<<Not>>]

## CHECK-START: int SmaliTests.$opt$noinline$andToOrV2(int, int) instruction_simplifier (after)
## CHECK-DAG:                            Not
## CHECK-NOT:                            Not

## CHECK-START: int SmaliTests.$opt$noinline$andToOrV2(int, int) instruction_simplifier (after)
## CHECK-NOT:                            And

# Original java source:
#
#     public static int $opt$noinline$andToOr(int a, int b) {
#       if (doThrow) throw new Error();
#       return ~a & ~b;
#     }

.method public static $opt$noinline$andToOrV2(II)I
    .registers 4
    .param p0, "a"    # I
    .param p1, "b"    # I

    .prologue
    .line 85
    sget-boolean v0, LMain;->doThrow:Z

    if-eqz v0, :cond_a

    new-instance v0, Ljava/lang/Error;

    invoke-direct {v0}, Ljava/lang/Error;-><init>()V

    throw v0

    .line 86
    :cond_a
    xor-int/lit8 v0, p0, -0x1

    xor-int/lit8 v1, p1, -0x1

    and-int/2addr v0, v1

    return v0
.end method


# Test transformation of Not/Not/And into Or/Not for boolean negations.
# Note that the graph before this instruction simplification pass does not
# contain `HBooleanNot` instructions. This is because this transformation
# follows the optimization of `HSelect` to `HBooleanNot` occurring in the
# same pass.

## CHECK-START: boolean SmaliTests.$opt$noinline$booleanAndToOrV2(boolean, boolean) instruction_simplifier$after_gvn (before)
## CHECK-DAG:       <<P1:z\d+>>          ParameterValue
## CHECK-DAG:       <<P2:z\d+>>          ParameterValue
## CHECK-DAG:       <<Const0:i\d+>>      IntConstant 0
## CHECK-DAG:       <<Const1:i\d+>>      IntConstant 1
## CHECK-DAG:       <<Select1:i\d+>>     Select [<<Const1>>,<<Const0>>,<<P1>>]
## CHECK-DAG:       <<Select2:i\d+>>     Select [<<Const1>>,<<Const0>>,<<P2>>]
## CHECK-DAG:       <<And:i\d+>>         And [<<Select1>>,<<Select2>>]
## CHECK-DAG:                            Return [<<And>>]

## CHECK-START: boolean SmaliTests.$opt$noinline$booleanAndToOrV2(boolean, boolean) instruction_simplifier$after_gvn (after)
## CHECK-DAG:       <<Cond1:z\d+>>       ParameterValue
## CHECK-DAG:       <<Cond2:z\d+>>       ParameterValue
## CHECK-DAG:       <<Or:i\d+>>          Or [<<Cond1>>,<<Cond2>>]
## CHECK-DAG:       <<BooleanNot:z\d+>>  BooleanNot [<<Or>>]
## CHECK-DAG:                            Return [<<BooleanNot>>]

## CHECK-START: boolean SmaliTests.$opt$noinline$booleanAndToOrV2(boolean, boolean) instruction_simplifier$after_bce (after)
## CHECK-DAG:                            BooleanNot
## CHECK-NOT:                            BooleanNot

## CHECK-START: boolean SmaliTests.$opt$noinline$booleanAndToOrV2(boolean, boolean) instruction_simplifier$after_bce (after)
## CHECK-NOT:                            And

# Original java source:
#
#     public static boolean $opt$noinline$booleanAndToOr(boolean a, boolean b) {
#       if (doThrow) throw new Error();
#       return !a & !b;
#     }

.method public static $opt$noinline$booleanAndToOrV2(ZZ)Z
    .registers 5
    .param p0, "a"    # Z
    .param p1, "b"    # Z

    .prologue
    const/4 v0, 0x1

    const/4 v1, 0x0

    .line 122
    sget-boolean v2, LMain;->doThrow:Z

    if-eqz v2, :cond_c

    new-instance v0, Ljava/lang/Error;

    invoke-direct {v0}, Ljava/lang/Error;-><init>()V

    throw v0

    .line 123
    :cond_c
    if-nez p0, :cond_13

    move v2, v0

    :goto_f
    if-nez p1, :cond_15

    :goto_11
    and-int/2addr v0, v2

    return v0

    :cond_13
    move v2, v1

    goto :goto_f

    :cond_15
    move v0, v1

    goto :goto_11
.end method


# Test transformation of Not/Not/Or into And/Not.

# See note above.
# The second Xor has its arguments reversed for no obvious reason.
## CHECK-START: long SmaliTests.$opt$noinline$orToAndV2(long, long) instruction_simplifier (before)
## CHECK-DAG:       <<P1:j\d+>>          ParameterValue
## CHECK-DAG:       <<P2:j\d+>>          ParameterValue
## CHECK-DAG:       <<CstM1:j\d+>>       LongConstant -1
## CHECK-DAG:       <<Not1:j\d+>>        Xor [<<P1>>,<<CstM1>>]
## CHECK-DAG:       <<Not2:j\d+>>        Xor [<<CstM1>>,<<P2>>]
## CHECK-DAG:       <<Or:j\d+>>          Or [<<Not1>>,<<Not2>>]
## CHECK-DAG:                            Return [<<Or>>]

## CHECK-START: long SmaliTests.$opt$noinline$orToAndV2(long, long) instruction_simplifier (after)
## CHECK-DAG:       <<P1:j\d+>>          ParameterValue
## CHECK-DAG:       <<P2:j\d+>>          ParameterValue
## CHECK-DAG:       <<And:j\d+>>         And [<<P1>>,<<P2>>]
## CHECK-DAG:       <<Not:j\d+>>         Not [<<And>>]
## CHECK-DAG:                            Return [<<Not>>]

## CHECK-START: long SmaliTests.$opt$noinline$orToAndV2(long, long) instruction_simplifier (after)
## CHECK-DAG:                            Not
## CHECK-NOT:                            Not

## CHECK-START: long SmaliTests.$opt$noinline$orToAndV2(long, long) instruction_simplifier (after)
## CHECK-NOT:                            Or

# Original java source:
#
#     public static long $opt$noinline$orToAnd(long a, long b) {
#       if (doThrow) throw new Error();
#       return ~a | ~b;
#     }

.method public static $opt$noinline$orToAndV2(JJ)J
    .registers 8
    .param p0, "a"    # J
    .param p2, "b"    # J

    .prologue
    const-wide/16 v2, -0x1

    .line 156
    sget-boolean v0, LMain;->doThrow:Z

    if-eqz v0, :cond_c

    new-instance v0, Ljava/lang/Error;

    invoke-direct {v0}, Ljava/lang/Error;-><init>()V

    throw v0

    .line 157
    :cond_c
    xor-long v0, p0, v2

    xor-long/2addr v2, p2

    or-long/2addr v0, v2

    return-wide v0
.end method

# Test transformation of Not/Not/Or into Or/And for boolean negations.
# Note that the graph before this instruction simplification pass does not
# contain `HBooleanNot` instructions. This is because this transformation
# follows the optimization of `HSelect` to `HBooleanNot` occurring in the
# same pass.

## CHECK-START: boolean SmaliTests.$opt$noinline$booleanOrToAndV2(boolean, boolean) instruction_simplifier$after_gvn (before)
## CHECK-DAG:       <<P1:z\d+>>          ParameterValue
## CHECK-DAG:       <<P2:z\d+>>          ParameterValue
## CHECK-DAG:       <<Const0:i\d+>>      IntConstant 0
## CHECK-DAG:       <<Const1:i\d+>>      IntConstant 1
## CHECK-DAG:       <<Select1:i\d+>>     Select [<<Const1>>,<<Const0>>,<<P1>>]
## CHECK-DAG:       <<Select2:i\d+>>     Select [<<Const1>>,<<Const0>>,<<P2>>]
## CHECK-DAG:       <<Or:i\d+>>          Or [<<Select1>>,<<Select2>>]
## CHECK-DAG:                            Return [<<Or>>]

## CHECK-START: boolean SmaliTests.$opt$noinline$booleanOrToAndV2(boolean, boolean) instruction_simplifier$after_gvn (after)
## CHECK-DAG:       <<Cond1:z\d+>>       ParameterValue
## CHECK-DAG:       <<Cond2:z\d+>>       ParameterValue
## CHECK-DAG:       <<And:i\d+>>         And [<<Cond1>>,<<Cond2>>]
## CHECK-DAG:       <<BooleanNot:z\d+>>  BooleanNot [<<And>>]
## CHECK-DAG:                            Return [<<BooleanNot>>]

## CHECK-START: boolean SmaliTests.$opt$noinline$booleanOrToAndV2(boolean, boolean) instruction_simplifier$after_bce (after)
## CHECK-DAG:                            BooleanNot
## CHECK-NOT:                            BooleanNot

## CHECK-START: boolean SmaliTests.$opt$noinline$booleanOrToAndV2(boolean, boolean) instruction_simplifier$after_bce (after)
## CHECK-NOT:                            Or

# Original java source:
#
#     public static boolean $opt$noinline$booleanOrToAnd(boolean a, boolean b) {
#       if (doThrow) throw new Error();
#       return !a | !b;
#     }

.method public static $opt$noinline$booleanOrToAndV2(ZZ)Z
    .registers 5
    .param p0, "a"    # Z
    .param p1, "b"    # Z

    .prologue
    const/4 v0, 0x1

    const/4 v1, 0x0

    .line 193
    sget-boolean v2, LMain;->doThrow:Z

    if-eqz v2, :cond_c

    new-instance v0, Ljava/lang/Error;

    invoke-direct {v0}, Ljava/lang/Error;-><init>()V

    throw v0

    .line 194
    :cond_c
    if-nez p0, :cond_13

    move v2, v0

    :goto_f
    if-nez p1, :cond_15

    :goto_11
    or-int/2addr v0, v2

    return v0

    :cond_13
    move v2, v1

    goto :goto_f

    :cond_15
    move v0, v1

    goto :goto_11
.end method


# Test that the transformation copes with inputs being separated from the
# bitwise operations.
# This is a regression test. The initial logic was inserting the new bitwise
# operation incorrectly.

## CHECK-START: int SmaliTests.$opt$noinline$regressInputsAwayV2(int, int) instruction_simplifier (before)
## CHECK-DAG:       <<P1:i\d+>>          ParameterValue
## CHECK-DAG:       <<P2:i\d+>>          ParameterValue
## CHECK-DAG:       <<Cst1:i\d+>>        IntConstant 1
## CHECK-DAG:       <<CstM1:i\d+>>       IntConstant -1
## CHECK-DAG:       <<AddP1:i\d+>>       Add [<<P1>>,<<Cst1>>]
## CHECK-DAG:       <<Not1:i\d+>>        Xor [<<AddP1>>,<<CstM1>>]
## CHECK-DAG:       <<AddP2:i\d+>>       Add [<<P2>>,<<Cst1>>]
## CHECK-DAG:       <<Not2:i\d+>>        Xor [<<AddP2>>,<<CstM1>>]
## CHECK-DAG:       <<Or:i\d+>>          Or [<<Not1>>,<<Not2>>]
## CHECK-DAG:                            Return [<<Or>>]

## CHECK-START: int SmaliTests.$opt$noinline$regressInputsAwayV2(int, int) instruction_simplifier (after)
## CHECK-DAG:       <<P1:i\d+>>          ParameterValue
## CHECK-DAG:       <<P2:i\d+>>          ParameterValue
## CHECK-DAG:       <<Cst1:i\d+>>        IntConstant 1
## CHECK-DAG:       <<AddP1:i\d+>>       Add [<<P1>>,<<Cst1>>]
## CHECK-DAG:       <<AddP2:i\d+>>       Add [<<P2>>,<<Cst1>>]
## CHECK-DAG:       <<And:i\d+>>         And [<<AddP1>>,<<AddP2>>]
## CHECK-DAG:       <<Not:i\d+>>         Not [<<And>>]
## CHECK-DAG:                            Return [<<Not>>]

## CHECK-START: int SmaliTests.$opt$noinline$regressInputsAwayV2(int, int) instruction_simplifier (after)
## CHECK-DAG:                            Not
## CHECK-NOT:                            Not

## CHECK-START: int SmaliTests.$opt$noinline$regressInputsAwayV2(int, int) instruction_simplifier (after)
## CHECK-NOT:                            Or

# Original java source:
#
#     public static int $opt$noinline$regressInputsAway(int a, int b) {
#       if (doThrow) throw new Error();
#       int a1 = a + 1;
#       int not_a1 = ~a1;
#       int b1 = b + 1;
#       int not_b1 = ~b1;
#       return not_a1 | not_b1;
#     }

.method public static $opt$noinline$regressInputsAwayV2(II)I
    .registers 7
    .param p0, "a"    # I
    .param p1, "b"    # I

    .prologue
    .line 234
    sget-boolean v4, LMain;->doThrow:Z

    if-eqz v4, :cond_a

    new-instance v4, Ljava/lang/Error;

    invoke-direct {v4}, Ljava/lang/Error;-><init>()V

    throw v4

    .line 235
    :cond_a
    add-int/lit8 v0, p0, 0x1

    .line 236
    .local v0, "a1":I
    xor-int/lit8 v2, v0, -0x1

    .line 237
    .local v2, "not_a1":I
    add-int/lit8 v1, p1, 0x1

    .line 238
    .local v1, "b1":I
    xor-int/lit8 v3, v1, -0x1

    .line 239
    .local v3, "not_b1":I
    or-int v4, v2, v3

    return v4
.end method


# Test transformation of Not/Not/Xor into Xor.

# See first note above.
## CHECK-START: int SmaliTests.$opt$noinline$notXorToXorV2(int, int) instruction_simplifier (before)
## CHECK-DAG:       <<P1:i\d+>>          ParameterValue
## CHECK-DAG:       <<P2:i\d+>>          ParameterValue
## CHECK-DAG:       <<CstM1:i\d+>>       IntConstant -1
## CHECK-DAG:       <<Not1:i\d+>>        Xor [<<P1>>,<<CstM1>>]
## CHECK-DAG:       <<Not2:i\d+>>        Xor [<<P2>>,<<CstM1>>]
## CHECK-DAG:       <<Xor:i\d+>>         Xor [<<Not1>>,<<Not2>>]
## CHECK-DAG:                            Return [<<Xor>>]

## CHECK-START: int SmaliTests.$opt$noinline$notXorToXorV2(int, int) instruction_simplifier (after)
## CHECK-DAG:       <<P1:i\d+>>          ParameterValue
## CHECK-DAG:       <<P2:i\d+>>          ParameterValue
## CHECK-DAG:       <<Xor:i\d+>>         Xor [<<P1>>,<<P2>>]
## CHECK-DAG:                            Return [<<Xor>>]

## CHECK-START: int SmaliTests.$opt$noinline$notXorToXorV2(int, int) instruction_simplifier (after)
## CHECK-NOT:                            Not

# Original java source:
#
#     public static int $opt$noinline$notXorToXor(int a, int b) {
#       if (doThrow) throw new Error();
#       return ~a ^ ~b;
#     }

.method public static $opt$noinline$notXorToXorV2(II)I
    .registers 4
    .param p0, "a"    # I
    .param p1, "b"    # I

    .prologue
    .line 266
    sget-boolean v0, LMain;->doThrow:Z

    if-eqz v0, :cond_a

    new-instance v0, Ljava/lang/Error;

    invoke-direct {v0}, Ljava/lang/Error;-><init>()V

    throw v0

    .line 267
    :cond_a
    xor-int/lit8 v0, p0, -0x1

    xor-int/lit8 v1, p1, -0x1

    xor-int/2addr v0, v1

    return v0
.end method


# Test transformation of Not/Not/Xor into Xor for boolean negations.
# Note that the graph before this instruction simplification pass does not
# contain `HBooleanNot` instructions. This is because this transformation
# follows the optimization of `HSelect` to `HBooleanNot` occurring in the
# same pass.

## CHECK-START: boolean SmaliTests.$opt$noinline$booleanNotXorToXorV2(boolean, boolean) instruction_simplifier$after_gvn (before)
## CHECK-DAG:       <<P1:z\d+>>          ParameterValue
## CHECK-DAG:       <<P2:z\d+>>          ParameterValue
## CHECK-DAG:       <<Const0:i\d+>>      IntConstant 0
## CHECK-DAG:       <<Const1:i\d+>>      IntConstant 1
## CHECK-DAG:       <<Select1:i\d+>>     Select [<<Const1>>,<<Const0>>,<<P1>>]
## CHECK-DAG:       <<Select2:i\d+>>     Select [<<Const1>>,<<Const0>>,<<P2>>]
## CHECK-DAG:       <<Xor:i\d+>>         Xor [<<Select1>>,<<Select2>>]
## CHECK-DAG:                            Return [<<Xor>>]

## CHECK-START: boolean SmaliTests.$opt$noinline$booleanNotXorToXorV2(boolean, boolean) instruction_simplifier$after_gvn (after)
## CHECK-DAG:       <<Cond1:z\d+>>       ParameterValue
## CHECK-DAG:       <<Cond2:z\d+>>       ParameterValue
## CHECK-DAG:       <<Xor:i\d+>>         Xor [<<Cond1>>,<<Cond2>>]
## CHECK-DAG:                            Return [<<Xor>>]

## CHECK-START: boolean SmaliTests.$opt$noinline$booleanNotXorToXorV2(boolean, boolean) instruction_simplifier$after_bce (after)
## CHECK-NOT:                            BooleanNot

# Original java source:
#
#     public static boolean $opt$noinline$booleanNotXorToXor(boolean a, boolean b) {
#       if (doThrow) throw new Error();
#       return !a ^ !b;
#     }

.method public static $opt$noinline$booleanNotXorToXorV2(ZZ)Z
    .registers 5
    .param p0, "a"    # Z
    .param p1, "b"    # Z

    .prologue
    const/4 v0, 0x1

    const/4 v1, 0x0

    .line 298
    sget-boolean v2, LMain;->doThrow:Z

    if-eqz v2, :cond_c

    new-instance v0, Ljava/lang/Error;

    invoke-direct {v0}, Ljava/lang/Error;-><init>()V

    throw v0

    .line 299
    :cond_c
    if-nez p0, :cond_13

    move v2, v0

    :goto_f
    if-nez p1, :cond_15

    :goto_11
    xor-int/2addr v0, v2

    return v0

    :cond_13
    move v2, v1

    goto :goto_f

    :cond_15
    move v0, v1

    goto :goto_11
.end method


# Check that no transformation is done when one Not has multiple uses.

## CHECK-START: int SmaliTests.$opt$noinline$notMultipleUsesV2(int, int) instruction_simplifier (before)
## CHECK-DAG:       <<P1:i\d+>>          ParameterValue
## CHECK-DAG:       <<P2:i\d+>>          ParameterValue
## CHECK-DAG:       <<CstM1:i\d+>>       IntConstant -1
## CHECK-DAG:       <<One:i\d+>>         IntConstant 1
## CHECK-DAG:       <<Not2:i\d+>>        Xor [<<P2>>,<<CstM1>>]
## CHECK-DAG:       <<And2:i\d+>>        And [<<Not2>>,<<One>>]
## CHECK-DAG:       <<Not1:i\d+>>        Xor [<<P1>>,<<CstM1>>]
## CHECK-DAG:       <<And1:i\d+>>        And [<<Not1>>,<<Not2>>]
## CHECK-DAG:       <<Add:i\d+>>         Add [<<And2>>,<<And1>>]
## CHECK-DAG:                            Return [<<Add>>]

## CHECK-START: int SmaliTests.$opt$noinline$notMultipleUsesV2(int, int) instruction_simplifier (after)
## CHECK-DAG:       <<P1:i\d+>>          ParameterValue
## CHECK-DAG:       <<P2:i\d+>>          ParameterValue
## CHECK-DAG:       <<One:i\d+>>         IntConstant 1
## CHECK-DAG:       <<Not2:i\d+>>        Not [<<P2>>]
## CHECK-DAG:       <<And2:i\d+>>        And [<<Not2>>,<<One>>]
## CHECK-DAG:       <<Not1:i\d+>>        Not [<<P1>>]
## CHECK-DAG:       <<And1:i\d+>>        And [<<Not1>>,<<Not2>>]
## CHECK-DAG:       <<Add:i\d+>>         Add [<<And2>>,<<And1>>]
## CHECK-DAG:                            Return [<<Add>>]

## CHECK-START: int SmaliTests.$opt$noinline$notMultipleUsesV2(int, int) instruction_simplifier (after)
## CHECK-NOT:                            Or

# Original java source:
#
#     public static int $opt$noinline$notMultipleUses(int a, int b) {
#       if (doThrow) throw new Error();
#       int tmp = ~b;
#       return (tmp & 0x1) + (~a & tmp);
#     }

.method public static $opt$noinline$notMultipleUsesV2(II)I
    .registers 5
    .param p0, "a"    # I
    .param p1, "b"    # I

    .prologue
    .line 333
    sget-boolean v1, LMain;->doThrow:Z

    if-eqz v1, :cond_a

    new-instance v1, Ljava/lang/Error;

    invoke-direct {v1}, Ljava/lang/Error;-><init>()V

    throw v1

    .line 334
    :cond_a
    xor-int/lit8 v0, p1, -0x1

    .line 335
    .local v0, "tmp":I
    and-int/lit8 v1, v0, 0x1

    xor-int/lit8 v2, p0, -0x1

    and-int/2addr v2, v0

    add-int/2addr v1, v2

    return v1
.end method
