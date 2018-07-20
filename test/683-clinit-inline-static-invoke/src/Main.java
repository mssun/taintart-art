/*
 * Copyright (C) 2018 The Android Open Source Project
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

public class Main {
  public static void main(String[] args) {
    // The following is a simple static field getter that can be inlined, referenced
    // through a subclass with the declaring class having no TypeId in current DexFile.
    // When we inline this getter, we're left with HLoadClass+HClinitCheck which cannot
    // be merged back to the InvokeStaticOrDirect for implicit class init check.
    // The declaring class is in the boot image, so the LoadClass can load it using the
    // .data.bimg.rel.ro section. However, the ClinitCheck entrypoint was previously
    // taking a type index of the declaring class and since we did not have a valid
    // TypeId in the current DexFile, we erroneously provided the type index from the
    // declaring DexFile and that caused a crash. This was fixed by changing the
    // ClinitCheck entrypoint to take the Class reference from LoadClass.
    int dummy = MyTimeZone.getDefaultTimeZoneType();
  }
}
