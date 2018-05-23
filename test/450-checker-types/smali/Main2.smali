# Copyright 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

.class LMain2;
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

## CHECK-START: void Main2.testArraySimpleRemove() instruction_simplifier (before)
## CHECK:         CheckCast

## CHECK-START: void Main2.testArraySimpleRemove() instruction_simplifier (after)
## CHECK-NOT:     CheckCast

.method static testArraySimpleRemove()V
    .registers 3

    .prologue
    .line 19
    const/16 v2, 0xa

    new-array v0, v2, [LSubclassA;

    .local v0, "b":[LSuper;
    move-object v1, v0

    .line 20
    check-cast v1, [LSubclassA;

    .line 21
    .local v1, "c":[LSubclassA;
    return-void
.end method
