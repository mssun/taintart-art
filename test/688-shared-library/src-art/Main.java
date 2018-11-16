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

import dalvik.system.DelegateLastClassLoader;
import dalvik.system.PathClassLoader;

public class Main {
    static final String TEST_NAME = "688-shared-library";
    static final String MAIN_JAR_FILE = System.getenv("DEX_LOCATION") + "/" + TEST_NAME + ".jar";
    static final String EX_JAR_FILE = System.getenv("DEX_LOCATION") + "/" + TEST_NAME + "-ex.jar";
    static ClassLoader bootLoader = Object.class.getClassLoader();

    public static void main(String[] args) throws Exception {
      testNoLibrary();
      testOneLibrary();
      testTwoLibraries1();
      testTwoLibraries2();
      testTransitive1();
      testTransitive2();
      testTransitive3();
      testTransitive4();
    }

    public static void assertIdentical(Object expected, Object actual) {
      if (expected != actual) {
        throw new Error("Expected " + expected + ", got " + actual);
      }
    }

    public static void testNoLibrary() throws Exception {
      ClassLoader loader = new PathClassLoader(MAIN_JAR_FILE, null, bootLoader);
      Class<?> cls = loader.loadClass("Main");
      assertIdentical(loader, cls.getClassLoader());
    }

    public static void testOneLibrary() throws Exception {
      ClassLoader[] sharedLibraries = {
          new PathClassLoader(EX_JAR_FILE, null, bootLoader),
      };
      ClassLoader delegateFirst =
          new PathClassLoader(MAIN_JAR_FILE, null, bootLoader, sharedLibraries);
      Class<?> cls = delegateFirst.loadClass("Main");
      assertIdentical(sharedLibraries[0], cls.getClassLoader());
      cls = delegateFirst.loadClass("SharedLibraryOne");
      assertIdentical(sharedLibraries[0], cls.getClassLoader());
      
      ClassLoader delegateLast =
          new DelegateLastClassLoader(MAIN_JAR_FILE, null, bootLoader, sharedLibraries);
      cls = delegateLast.loadClass("Main");
      assertIdentical(sharedLibraries[0], cls.getClassLoader());
      cls = delegateLast.loadClass("SharedLibraryOne");
      assertIdentical(sharedLibraries[0], cls.getClassLoader());
    }
    
    public static void testTwoLibraries1() throws Exception {
      ClassLoader[] sharedLibraries = {
          new PathClassLoader(MAIN_JAR_FILE, null, bootLoader),
          new PathClassLoader(EX_JAR_FILE, null, bootLoader),
      };
      ClassLoader delegateFirst = new PathClassLoader("", null, bootLoader, sharedLibraries);
      Class<?> cls = delegateFirst.loadClass("Main");
      assertIdentical(sharedLibraries[0], cls.getClassLoader());
      cls = delegateFirst.loadClass("SharedLibraryOne");
      assertIdentical(sharedLibraries[1], cls.getClassLoader());
      
      ClassLoader delegateLast =
          new DelegateLastClassLoader(MAIN_JAR_FILE, null, bootLoader, sharedLibraries);
      cls = delegateLast.loadClass("Main");
      assertIdentical(sharedLibraries[0], cls.getClassLoader());
      cls = delegateLast.loadClass("SharedLibraryOne");
      assertIdentical(sharedLibraries[1], cls.getClassLoader());
    }
    
    public static void testTwoLibraries2() throws Exception {
      ClassLoader[] sharedLibraries = {
          new PathClassLoader(EX_JAR_FILE, null, bootLoader),
          new PathClassLoader(MAIN_JAR_FILE, null, bootLoader),
      };
      ClassLoader delegateFirst = new PathClassLoader("", null, bootLoader, sharedLibraries);
      Class<?> cls = delegateFirst.loadClass("Main");
      assertIdentical(sharedLibraries[0], cls.getClassLoader());
      cls = delegateFirst.loadClass("SharedLibraryOne");
      assertIdentical(sharedLibraries[0], cls.getClassLoader());
      
      ClassLoader delegateLast = new DelegateLastClassLoader("", null, bootLoader, sharedLibraries);
      cls = delegateLast.loadClass("Main");
      assertIdentical(sharedLibraries[0], cls.getClassLoader());
      cls = delegateLast.loadClass("SharedLibraryOne");
      assertIdentical(sharedLibraries[0], cls.getClassLoader());
    }

    public static void testTransitive1() throws Exception {
      ClassLoader[] sharedLibraryLevel2 = {
          new PathClassLoader(EX_JAR_FILE, null, bootLoader),
      };
      ClassLoader[] sharedLibraryLevel1 = {
          new PathClassLoader(MAIN_JAR_FILE, null, bootLoader, sharedLibraryLevel2),
      };

      ClassLoader delegateFirst = new PathClassLoader("", null, bootLoader, sharedLibraryLevel1);
      Class<?> cls = delegateFirst.loadClass("Main");
      assertIdentical(sharedLibraryLevel2[0], cls.getClassLoader());
      cls = delegateFirst.loadClass("SharedLibraryOne");
      assertIdentical(sharedLibraryLevel2[0], cls.getClassLoader());
      
      ClassLoader delegateLast =
          new DelegateLastClassLoader("", null, bootLoader, sharedLibraryLevel1);
      cls = delegateLast.loadClass("Main");
      assertIdentical(sharedLibraryLevel2[0], cls.getClassLoader());
      cls = delegateLast.loadClass("SharedLibraryOne");
      assertIdentical(sharedLibraryLevel2[0], cls.getClassLoader());
    }
    
    public static void testTransitive2() throws Exception {
      ClassLoader[] sharedLibraryLevel2 = {
          new PathClassLoader(MAIN_JAR_FILE, null, bootLoader),
      };
      ClassLoader[] sharedLibraryLevel1 = {
          new PathClassLoader(EX_JAR_FILE, null, bootLoader, sharedLibraryLevel2),
      };

      ClassLoader delegateFirst = new PathClassLoader("", null, bootLoader, sharedLibraryLevel1);
      Class<?> cls = delegateFirst.loadClass("Main");
      assertIdentical(sharedLibraryLevel2[0], cls.getClassLoader());
      cls = delegateFirst.loadClass("SharedLibraryOne");
      assertIdentical(sharedLibraryLevel1[0], cls.getClassLoader());
      
      ClassLoader delegateLast =
          new DelegateLastClassLoader("", null, bootLoader, sharedLibraryLevel1);
      cls = delegateLast.loadClass("Main");
      assertIdentical(sharedLibraryLevel2[0], cls.getClassLoader());
      cls = delegateLast.loadClass("SharedLibraryOne");
      assertIdentical(sharedLibraryLevel1[0], cls.getClassLoader());
    }

    public static void testTransitive3() throws Exception {
      ClassLoader[] sharedLibraryLevel2 = {
          new PathClassLoader(MAIN_JAR_FILE, null, bootLoader),
      };
      ClassLoader[] sharedLibraryLevel1 = {
          new PathClassLoader(EX_JAR_FILE, null, bootLoader, sharedLibraryLevel2),
          sharedLibraryLevel2[0],
      };

      ClassLoader delegateFirst = new PathClassLoader("", null, bootLoader, sharedLibraryLevel1);
      Class<?> cls = delegateFirst.loadClass("Main");
      assertIdentical(sharedLibraryLevel2[0], cls.getClassLoader());
      cls = delegateFirst.loadClass("SharedLibraryOne");
      assertIdentical(sharedLibraryLevel1[0], cls.getClassLoader());
      
      ClassLoader delegateLast =
          new DelegateLastClassLoader("", null, bootLoader, sharedLibraryLevel1);
      cls = delegateLast.loadClass("Main");
      assertIdentical(sharedLibraryLevel2[0], cls.getClassLoader());
      cls = delegateLast.loadClass("SharedLibraryOne");
      assertIdentical(sharedLibraryLevel1[0], cls.getClassLoader());
    }
    
    public static void testTransitive4() throws Exception {
      ClassLoader[] sharedLibraryLevel2 = {
          new PathClassLoader(EX_JAR_FILE, null, bootLoader),
      };
      ClassLoader[] sharedLibraryLevel1 = {
          new PathClassLoader(MAIN_JAR_FILE, null, bootLoader, sharedLibraryLevel2),
          sharedLibraryLevel2[0],
      };

      ClassLoader delegateFirst = new PathClassLoader("", null, bootLoader, sharedLibraryLevel1);
      Class<?> cls = delegateFirst.loadClass("Main");
      assertIdentical(sharedLibraryLevel2[0], cls.getClassLoader());
      cls = delegateFirst.loadClass("SharedLibraryOne");
      assertIdentical(sharedLibraryLevel2[0], cls.getClassLoader());
      
      ClassLoader delegateLast =
          new DelegateLastClassLoader("", null, bootLoader, sharedLibraryLevel1);
      cls = delegateLast.loadClass("Main");
      assertIdentical(sharedLibraryLevel2[0], cls.getClassLoader());
      cls = delegateLast.loadClass("SharedLibraryOne");
      assertIdentical(sharedLibraryLevel2[0], cls.getClassLoader());
    }
}
