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
.source "Main.java"

# direct methods

## CHECK-START: int Main2.test4(TestClass, boolean) load_store_elimination (before)
## CHECK: InstanceFieldSet
## CHECK: InstanceFieldGet
## CHECK: Return
## CHECK: InstanceFieldSet

## CHECK-START: int Main2.test4(TestClass, boolean) load_store_elimination (after)
## CHECK: InstanceFieldSet
## CHECK-NOT: NullCheck
## CHECK-NOT: InstanceFieldGet
## CHECK: Return
## CHECK: InstanceFieldSet

# Set and merge the same value in two branches.

# Original java source:
#
#     static int test4(TestClass obj, boolean b) {
#       if (b) {
#         obj.i = 1;
#       } else {
#         obj.i = 1;
#       }
#       return obj.i;
#     }

.method public static test4(LTestClass;Z)I
    .registers 3
    .param p0, "obj"    # LTestClass;
    .param p1, "b"    # Z

    .prologue
    const/4 v0, 0x1

    .line 185
    if-eqz p1, :cond_8

    .line 186
    iput v0, p0, LTestClass;->i:I

    .line 190
    :goto_5
    iget v0, p0, LTestClass;->i:I

    return v0

    .line 188
    :cond_8
    iput v0, p0, LTestClass;->i:I

    goto :goto_5
.end method

## CHECK-START: int Main2.test5(TestClass, boolean) load_store_elimination (before)
## CHECK: InstanceFieldSet
## CHECK: InstanceFieldGet
## CHECK: Return
## CHECK: InstanceFieldSet

## CHECK-START: int Main2.test5(TestClass, boolean) load_store_elimination (after)
## CHECK: InstanceFieldSet
## CHECK: InstanceFieldGet
## CHECK: Return
## CHECK: InstanceFieldSet

# Set and merge different values in two branches.
# Original java source:
#
#     static int test5(TestClass obj, boolean b) {
#       if (b) {
#         obj.i = 1;
#       } else {
#         obj.i = 2;
#       }
#       return obj.i;
#     }

.method public static test5(LTestClass;Z)I
    .registers 3
    .param p0, "obj"    # LTestClass;
    .param p1, "b"    # Z

    .prologue
    .line 207
    if-eqz p1, :cond_8

    .line 208
    const/4 v0, 0x1

    iput v0, p0, LTestClass;->i:I

    .line 212
    :goto_5
    iget v0, p0, LTestClass;->i:I

    return v0

    .line 210
    :cond_8
    const/4 v0, 0x2

    iput v0, p0, LTestClass;->i:I

    goto :goto_5
.end method

## CHECK-START: int Main2.test23(boolean) load_store_elimination (before)
## CHECK: NewInstance
## CHECK: InstanceFieldSet
## CHECK: InstanceFieldGet
## CHECK: InstanceFieldSet
## CHECK: InstanceFieldGet
## CHECK: Return
## CHECK: InstanceFieldGet
## CHECK: InstanceFieldSet

## CHECK-START: int Main2.test23(boolean) load_store_elimination (after)
## CHECK: NewInstance
## CHECK-NOT: InstanceFieldSet
## CHECK-NOT: InstanceFieldGet
## CHECK: InstanceFieldSet
## CHECK: InstanceFieldGet
## CHECK: Return
## CHECK-NOT: InstanceFieldGet
## CHECK: InstanceFieldSet

# Test store elimination on merging.

# Original java source:
#
#     static int test23(boolean b) {
#       TestClass obj = new TestClass();
#       obj.i = 3;      // This store can be eliminated since the value flows into each branch.
#       if (b) {
#         obj.i += 1;   // This store cannot be eliminated due to the merge later.
#       } else {
#         obj.i += 2;   // This store cannot be eliminated due to the merge later.
#       }
#       return obj.i;
#     }

.method public static test23(Z)I
    .registers 3
    .param p0, "b"    # Z

    .prologue
    .line 582
    new-instance v0, LTestClass;

    invoke-direct {v0}, LTestClass;-><init>()V

    .line 583
    .local v0, "obj":LTestClass;
    const/4 v1, 0x3

    iput v1, v0, LTestClass;->i:I

    .line 584
    if-eqz p0, :cond_13

    .line 585
    iget v1, v0, LTestClass;->i:I

    add-int/lit8 v1, v1, 0x1

    iput v1, v0, LTestClass;->i:I

    .line 589
    :goto_10
    iget v1, v0, LTestClass;->i:I

    return v1

    .line 587
    :cond_13
    iget v1, v0, LTestClass;->i:I

    add-int/lit8 v1, v1, 0x2

    iput v1, v0, LTestClass;->i:I

    goto :goto_10
.end method

## CHECK-START: float Main2.test24() load_store_elimination (before)
## CHECK-DAG:     <<True:i\d+>>     IntConstant 1
## CHECK-DAG:     <<Float8:f\d+>>   FloatConstant 8
## CHECK-DAG:     <<Float42:f\d+>>  FloatConstant 42
## CHECK-DAG:     <<Obj:l\d+>>      NewInstance
## CHECK-DAG:                       InstanceFieldSet [<<Obj>>,<<True>>]
## CHECK-DAG:                       InstanceFieldSet [<<Obj>>,<<Float8>>]
## CHECK-DAG:     <<GetTest:z\d+>>  InstanceFieldGet [<<Obj>>]
## CHECK-DAG:     <<GetField:f\d+>> InstanceFieldGet [<<Obj>>]
## CHECK-DAG:     <<Select:f\d+>>   Select [<<Float42>>,<<GetField>>,<<GetTest>>]
## CHECK-DAG:                       Return [<<Select>>]

## CHECK-START: float Main2.test24() load_store_elimination (after)
## CHECK-DAG:     <<True:i\d+>>     IntConstant 1
## CHECK-DAG:     <<Float8:f\d+>>   FloatConstant 8
## CHECK-DAG:     <<Float42:f\d+>>  FloatConstant 42
## CHECK-DAG:     <<Select:f\d+>>   Select [<<Float42>>,<<Float8>>,<<True>>]
## CHECK-DAG:                       Return [<<Select>>]

# Original java source:
#
#     static float test24() {
#       float a = 42.0f;
#       TestClass3 obj = new TestClass3();
#       if (obj.test1) {
#         a = obj.floatField;
#       }
#       return a;
#     }

.method public static test24()F
    .registers 3

    .prologue
    .line 612
    const/high16 v0, 0x42280000    # 42.0f

    .line 613
    .local v0, "a":F
    new-instance v1, LTestClass3;

    invoke-direct {v1}, LTestClass3;-><init>()V

    .line 614
    .local v1, "obj":LTestClass3;
    iget-boolean v2, v1, LTestClass3;->test1:Z

    if-eqz v2, :cond_d

    .line 615
    iget v0, v1, LTestClass3;->floatField:F

    .line 617
    :cond_d
    return v0
.end method
