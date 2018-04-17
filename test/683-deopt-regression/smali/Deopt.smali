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

.class public LDeopt;

.super Ljava/lang/Object;

.method public constructor <init>()V
.registers 1
    invoke-direct {v0}, Ljava/lang/Object;-><init>()V
    return-void
.end method

.method public static testCase([I)I
    .registers 8

    const v0, 0x0          # counter
    const v1, 0xF          # loop max
    const v2, 0x0          # result

    :try_start
    # Something throwing to start the try block. v6 contains a reference.
    move-object v6, p0
    aget v3, p0, v0

    # Invalidate v6 before entering the loop.
    const-wide v5, 0x0

    :loop_start
    # Set v6 to a different reference (creates a catch phi).
    const v6, 0x0

    aget v3, p0, v0
    add-int/2addr v2, v3
    add-int/lit8 v0, v0, 0x1
    if-lt v0, v1, :loop_start

    :try_end
    .catchall {:try_start .. :try_end} :catch

    :exit
    return v2

    :catch
    invoke-virtual {v6}, Ljava/lang/Object;->hashCode()I   # use v6 as a reference
    goto :exit

.end method

