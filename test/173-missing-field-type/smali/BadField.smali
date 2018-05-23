#
# Copyright (C) 2018 The Android Open Source Project
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

.class public LBadField;
.super Ljava/lang/Object;

# This is a bad field since there is no class Widget in this test.
.field public static widget:LWidget;

.method public constructor <init>()V
    .registers 2
    invoke-direct {v1}, Ljava/lang/Object;-><init>()V
    return-void
.end method

.method public static constructor <clinit>()V
    .registers 1
    new-instance v0, Ljava/lang/Object;
    invoke-direct {v0}, Ljava/lang/Object;-><init>()V
    sput-object v0, LBadField;->widget:LWidget;
    return-void
.end method