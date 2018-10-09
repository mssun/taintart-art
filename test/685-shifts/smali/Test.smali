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

.class public LTest;
.super Ljava/lang/Object;

.method public static shlZero(J)J
    .registers 6
    const v2, 0x0
    shl-long v0, p0, v2
    return-wide v0
.end method

.method public static shrZero(J)J
    .registers 6
    const v2, 0x0
    shr-long v0, p0, v2
    return-wide v0
.end method

.method public static ushrZero(J)J
    .registers 6
    const v2, 0x0
    ushr-long v0, p0, v2
    return-wide v0
.end method

.method public static shlOne(J)J
    .registers 6
    const v2, 0x1
    shl-long v0, p0, v2
    return-wide v0
.end method

.method public static shrOne(J)J
    .registers 6
    const v2, 0x1
    shr-long v0, p0, v2
    return-wide v0
.end method

.method public static ushrOne(J)J
    .registers 6
    const v2, 0x1
    ushr-long v0, p0, v2
    return-wide v0
.end method
