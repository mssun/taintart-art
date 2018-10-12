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

.method public constructor <init>()V
    .registers 2
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V
    const/4 v0, 0x1
    sput v0, LTest;->field:I
    return-void
.end method


.method public testEmpty()V
  .registers 2
  const/4 p0, 0x1
  invoke-static {}, LMain;->getThisOfCaller()Ljava/lang/Object;
  move-result-object v0
  sput-object v0, LMain;->field:Ljava/lang/Object;
  return-void
.end method

.method public testPrimitive()I
  .registers 2
  sget p0, LTest;->field:I
  invoke-static {}, LMain;->getThisOfCaller()Ljava/lang/Object;
  move-result-object v0
  sput-object v0, LMain;->field:Ljava/lang/Object;
  return p0
.end method

.field static public field:I
