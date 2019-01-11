/*
 * Copyright 2019 The Android Open Source Project
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

import java.lang.reflect.Field;
import java.lang.reflect.Constructor;
import java.lang.reflect.Method;

class Main {
  public static void main(String[] args) {
    System.loadLibrary(args[0]);
    System.out.println("App image loaded " + checkAppImageLoaded());
    int regionSize = getRegionSize();
    int objectsSectionSize = checkAppImageSectionSize(Main.class);
    System.out.println("Region size " + regionSize);
    System.out.println("App image section size large enough " + (objectsSectionSize > regionSize));
    if (objectsSectionSize <= regionSize) {
      System.out.println("Section size " + objectsSectionSize);
    }
  }

  public static native boolean checkAppImageLoaded();
  public static native int getRegionSize();
  public static native int checkAppImageSectionSize(Class c);
}
