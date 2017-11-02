/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.ahat.heapdump;

public enum Type {
  OBJECT("Object", 4),
  BOOLEAN("boolean", 1),
  CHAR("char", 2),
  FLOAT("float", 4),
  DOUBLE("double", 8),
  BYTE("byte", 1),
  SHORT("short", 2),
  INT("int", 4),
  LONG("long", 8);

  public final String name;
  final int size;

  Type(String name, int size) {
    this.name = name;
    this.size = size;
  }

  @Override
  public String toString() {
    return name;
  }
}
