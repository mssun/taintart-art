/*
 * Copyright (C) 2019 The Android Open Source Project
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

import art.Redefinition;
import java.lang.reflect.Constructor;
import java.lang.reflect.Method;
import java.net.URL;
import java.net.URLClassLoader;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Base64;

public class Main {
  private static String TEST_NAME = "1964-add-to-dex-classloader-file";
  private static boolean IS_ART = System.getProperty("java.vm.name").equals("Dalvik");

  private static String TEST_CLASS_NAME = "foobar.TestClass";
  private static String NEW_CLASS_NAME = "foobar.NewClass";

  /**
   * base64 encoded class/dex file for
   * package foobar;
   * public class TestClass {
   *   public static void sayHi() {
   *    System.out.println("Hello again from TestClass sayHi function");
   *    TestClass.sayBye();
   *   }
   *   static void sayBye() {
   *    System.out.println("Goodbye from TestClass!");
   *   }
   * }
   */
  private static byte[] CLASS_BYTES = Base64.getDecoder().decode(
      "yv66vgAAADUAIQoACAARCQASABMIABQKABUAFgoABwAXCAAYBwAZBwAaAQAGPGluaXQ+AQADKClW"
      + "AQAEQ29kZQEAD0xpbmVOdW1iZXJUYWJsZQEABXNheUhpAQAGc2F5QnllAQAKU291cmNlRmlsZQEA"
      + "DlRlc3RDbGFzcy5qYXZhDAAJAAoHABsMABwAHQEAI0hlbGxvIGZyb20gVGVzdENsYXNzIHNheUhp"
      + "IGZ1bmN0aW9uBwAeDAAfACAMAA4ACgEAF0dvb2RieWUgZnJvbSBUZXN0Q2xhc3MhAQAQZm9vYmFy"
      + "L1Rlc3RDbGFzcwEAEGphdmEvbGFuZy9PYmplY3QBABBqYXZhL2xhbmcvU3lzdGVtAQADb3V0AQAV"
      + "TGphdmEvaW8vUHJpbnRTdHJlYW07AQATamF2YS9pby9QcmludFN0cmVhbQEAB3ByaW50bG4BABUo"
      + "TGphdmEvbGFuZy9TdHJpbmc7KVYAIQAHAAgAAAAAAAMAAQAJAAoAAQALAAAAHQABAAEAAAAFKrcA"
      + "AbEAAAABAAwAAAAGAAEAAAACAAkADQAKAAEACwAAACwAAgAAAAAADLIAAhIDtgAEuAAFsQAAAAEA"
      + "DAAAAA4AAwAAAAQACAAFAAsABgAIAA4ACgABAAsAAAAlAAIAAAAAAAmyAAISBrYABLEAAAABAAwA"
      + "AAAKAAIAAAAIAAgACQABAA8AAAACABA=");

  private static byte[] DEX_BYTES = Base64.getDecoder().decode(
      "ZGV4CjAzNQARmtFTPdWXebnrTNy5b71tEiJKC96qIPXAAwAAcAAAAHhWNBIAAAAAAAAAABQDAAAQ"
      + "AAAAcAAAAAYAAACwAAAAAgAAAMgAAAABAAAA4AAAAAUAAADoAAAAAQAAABABAACQAgAAMAEAAKYB"
      + "AACuAQAAxwEAAOwBAAAAAgAAFwIAACsCAAA/AgAAUwIAAGMCAABmAgAAagIAAG8CAAB4AgAAgAIA"
      + "AIcCAAADAAAABAAAAAUAAAAGAAAABwAAAAkAAAAJAAAABQAAAAAAAAAKAAAABQAAAKABAAAEAAEA"
      + "CwAAAAAAAAAAAAAAAAAAAA0AAAAAAAAADgAAAAEAAQAMAAAAAgAAAAAAAAAAAAAAAQAAAAIAAAAA"
      + "AAAACAAAAAAAAAD+AgAAAAAAAAEAAQABAAAAjgEAAAQAAABwEAQAAAAOAAIAAAACAAAAkgEAAAgA"
      + "AABiAAAAGgEBAG4gAwAQAA4AAgAAAAIAAACXAQAACwAAAGIAAAAaAQIAbiADABAAcQABAAAADgAC"
      + "AA4ACAAOeAAEAA54PAAAAAABAAAAAwAGPGluaXQ+ABdHb29kYnllIGZyb20gVGVzdENsYXNzIQAj"
      + "SGVsbG8gZnJvbSBUZXN0Q2xhc3Mgc2F5SGkgZnVuY3Rpb24AEkxmb29iYXIvVGVzdENsYXNzOwAV"
      + "TGphdmEvaW8vUHJpbnRTdHJlYW07ABJMamF2YS9sYW5nL09iamVjdDsAEkxqYXZhL2xhbmcvU3Ry"
      + "aW5nOwASTGphdmEvbGFuZy9TeXN0ZW07AA5UZXN0Q2xhc3MuamF2YQABVgACVkwAA291dAAHcHJp"
      + "bnRsbgAGc2F5QnllAAVzYXlIaQB1fn5EOHsiY29tcGlsYXRpb24tbW9kZSI6ImRlYnVnIiwibWlu"
      + "LWFwaSI6MSwic2hhLTEiOiJkMzI4MmI4ZjU0N2MyMzRjNGU0YzkzMDljMzZjNzk1YTI5ODU2ZWFi"
      + "IiwidmVyc2lvbiI6IjEuNi4xLWRldiJ9AAAAAwAAgYAEsAIBCMgCAQnoAgAAAAAOAAAAAAAAAAEA"
      + "AAAAAAAAAQAAABAAAABwAAAAAgAAAAYAAACwAAAAAwAAAAIAAADIAAAABAAAAAEAAADgAAAABQAA"
      + "AAUAAADoAAAABgAAAAEAAAAQAQAAASAAAAMAAAAwAQAAAyAAAAMAAACOAQAAARAAAAEAAACgAQAA"
      + "AiAAABAAAACmAQAAACAAAAEAAAD+AgAAAxAAAAEAAAAQAwAAABAAAAEAAAAUAwAA");
  /**
   * base64 encoded class/dex file for
   * package foobar;
   * public class TestClass {
   *   public static void sayHi() {
   *    System.out.println("Hello again from TestClass sayHi function");
   *    NewClass.sayHi();
   *   }
   *   static void sayBye() {
   *    System.out.println("Goodbye again from TestClass!");
   *   }
   * }
   */
  private static byte[] REDEF_CLASS_BYTES = Base64.getDecoder().decode(
      "yv66vgAAADUAIwoACAARCQASABMIABQKABUAFgoAFwAYCAAZBwAaBwAbAQAGPGluaXQ+AQADKClW"
      + "AQAEQ29kZQEAD0xpbmVOdW1iZXJUYWJsZQEABXNheUhpAQAGc2F5QnllAQAKU291cmNlRmlsZQEA"
      + "DlRlc3RDbGFzcy5qYXZhDAAJAAoHABwMAB0AHgEAKUhlbGxvIGFnYWluIGZyb20gVGVzdENsYXNz"
      + "IHNheUhpIGZ1bmN0aW9uBwAfDAAgACEHACIMAA0ACgEAHUdvb2RieWUgYWdhaW4gZnJvbSBUZXN0"
      + "Q2xhc3MhAQAQZm9vYmFyL1Rlc3RDbGFzcwEAEGphdmEvbGFuZy9PYmplY3QBABBqYXZhL2xhbmcv"
      + "U3lzdGVtAQADb3V0AQAVTGphdmEvaW8vUHJpbnRTdHJlYW07AQATamF2YS9pby9QcmludFN0cmVh"
      + "bQEAB3ByaW50bG4BABUoTGphdmEvbGFuZy9TdHJpbmc7KVYBAA9mb29iYXIvTmV3Q2xhc3MAIQAH"
      + "AAgAAAAAAAMAAQAJAAoAAQALAAAAHQABAAEAAAAFKrcAAbEAAAABAAwAAAAGAAEAAAACAAkADQAK"
      + "AAEACwAAACwAAgAAAAAADLIAAhIDtgAEuAAFsQAAAAEADAAAAA4AAwAAAAQACAAFAAsABgAIAA4A"
      + "CgABAAsAAAAlAAIAAAAAAAmyAAISBrYABLEAAAABAAwAAAAKAAIAAAAIAAgACQABAA8AAAACABA=");

  private static byte[] REDEF_DEX_BYTES = Base64.getDecoder().decode(
      "ZGV4CjAzNQA2plEeYRH4vl6wJgnAZOVcZ537QN9NXB3wAwAAcAAAAHhWNBIAAAAAAAAAAEQDAAAR"
      + "AAAAcAAAAAcAAAC0AAAAAgAAANAAAAABAAAA6AAAAAYAAADwAAAAAQAAACABAACwAgAAQAEAALYB"
      + "AAC+AQAA3QEAAAgCAAAbAgAALwIAAEYCAABaAgAAbgIAAIICAACSAgAAlQIAAJkCAACeAgAApwIA"
      + "AK8CAAC2AgAAAwAAAAQAAAAFAAAABgAAAAcAAAAIAAAACgAAAAoAAAAGAAAAAAAAAAsAAAAGAAAA"
      + "sAEAAAUAAgAMAAAAAAAAAA8AAAABAAAAAAAAAAEAAAAOAAAAAQAAAA8AAAACAAEADQAAAAMAAAAA"
      + "AAAAAQAAAAEAAAADAAAAAAAAAAkAAAAAAAAALQMAAAAAAAABAAEAAQAAAJ4BAAAEAAAAcBAFAAAA"
      + "DgACAAAAAgAAAKIBAAAIAAAAYgAAABoBAQBuIAQAEAAOAAIAAAACAAAApwEAAAsAAABiAAAAGgEC"
      + "AG4gBAAQAHEAAAAAAA4AAgAOAAgADngABAAOeDwAAAAAAQAAAAQABjxpbml0PgAdR29vZGJ5ZSBh"
      + "Z2FpbiBmcm9tIFRlc3RDbGFzcyEAKUhlbGxvIGFnYWluIGZyb20gVGVzdENsYXNzIHNheUhpIGZ1"
      + "bmN0aW9uABFMZm9vYmFyL05ld0NsYXNzOwASTGZvb2Jhci9UZXN0Q2xhc3M7ABVMamF2YS9pby9Q"
      + "cmludFN0cmVhbTsAEkxqYXZhL2xhbmcvT2JqZWN0OwASTGphdmEvbGFuZy9TdHJpbmc7ABJMamF2"
      + "YS9sYW5nL1N5c3RlbTsADlRlc3RDbGFzcy5qYXZhAAFWAAJWTAADb3V0AAdwcmludGxuAAZzYXlC"
      + "eWUABXNheUhpAHV+fkQ4eyJjb21waWxhdGlvbi1tb2RlIjoiZGVidWciLCJtaW4tYXBpIjoxLCJz"
      + "aGEtMSI6ImQzMjgyYjhmNTQ3YzIzNGM0ZTRjOTMwOWMzNmM3OTVhMjk4NTZlYWIiLCJ2ZXJzaW9u"
      + "IjoiMS42LjEtZGV2In0AAAADAAGBgATAAgEI2AIBCfgCAAAAAAAOAAAAAAAAAAEAAAAAAAAAAQAA"
      + "ABEAAABwAAAAAgAAAAcAAAC0AAAAAwAAAAIAAADQAAAABAAAAAEAAADoAAAABQAAAAYAAADwAAAA"
      + "BgAAAAEAAAAgAQAAASAAAAMAAABAAQAAAyAAAAMAAACeAQAAARAAAAEAAACwAQAAAiAAABEAAAC2"
      + "AQAAACAAAAEAAAAtAwAAAxAAAAEAAABAAwAAABAAAAEAAABEAwAA");

  public static void SafePrintCause(Throwable t) {
    StackTraceElement cause = t.getStackTrace()[0];
    System.out.println(" --- " + t.getClass().getName() + " At " + cause.getClassName() + "." +
                       cause.getMethodName() + "(" + cause.getFileName() + ":" +
                       cause.getLineNumber() + ")");
  }

  public static void run() throws Exception {
    System.out.println(" - Run while adding new referenced class.");
    try {
      run(true);
    } catch (Exception e) {
      // Unfortunately art and RI have different messages here so just return the type.
      System.out.println(" -- Exception caught when running test with new class added! " +
                         e.getCause().getClass().getName());
      SafePrintCause(e.getCause());
      System.out.println(e);
      e.printStackTrace();
    }
    System.out.println(" - Run without adding new referenced class.");
    try {
      run(false);
    } catch (Exception e) {
      // Unfortunately art and RI have different messages here so just return the type.
      System.out.println(" -- Exception caught when running test without new class added! " +
                         e.getCause().getClass().getName());
      SafePrintCause(e.getCause());
    }
  }

  public static void run(boolean add_new) throws Exception {
    ClassLoader cl = getClassLoader();
    Class<?> target = cl.loadClass(TEST_CLASS_NAME);
    Method sayHi = target.getDeclaredMethod("sayHi");
    System.out.println(" -- Running sayHi before redefinition");
    sayHi.invoke(null);
    if (add_new) {
      System.out.println(" -- Adding NewClass to classloader!");
      addToClassLoader(cl);
    }
    System.out.println(" -- Redefine the TestClass");
    Redefinition.doCommonClassRedefinition(target, REDEF_CLASS_BYTES, REDEF_DEX_BYTES);
    System.out.println(" -- call TestClass again, now with NewClass refs");
    sayHi.invoke(null);
  }

  public static class ExtensibleClassLoader extends URLClassLoader {
    public ExtensibleClassLoader() {
      // Initially we don't have any URLs
      super(new URL[] {}, ExtensibleClassLoader.class.getClassLoader());
    }

    public void addSingleUrl(String file) throws Exception {
      this.addURL(new URL("file://" + file));
    }

    protected Class<?> findClass(String name) throws ClassNotFoundException {
      // Just define the TestClass without other jars.
      if (name.equals(TEST_CLASS_NAME)) {
        return this.defineClass(TEST_CLASS_NAME, CLASS_BYTES, 0, CLASS_BYTES.length);
      } else {
        return super.findClass(name);
      }
    }
  }

  public static ClassLoader getClassLoader() throws Exception {
    if (!IS_ART) {
      return new ExtensibleClassLoader();
    } else {
      Class<?> class_loader_class = Class.forName("dalvik.system.InMemoryDexClassLoader");
      Constructor<?> ctor = class_loader_class.getConstructor(ByteBuffer.class, ClassLoader.class);
      return (ClassLoader)ctor.newInstance(ByteBuffer.wrap(DEX_BYTES), Main.class.getClassLoader());
    }
  }

  public static void addToClassLoader(ClassLoader cl) throws Exception {
    if (IS_ART) {
      addToClassLoaderNative(cl, System.getenv("DEX_LOCATION") + "/" + TEST_NAME + "-ex.jar");
    } else {
      ((ExtensibleClassLoader)cl).addSingleUrl(System.getenv("DEX_LOCATION") + "/classes-ex/");
    }
  }

  public static native void addToClassLoaderNative(ClassLoader loader, String segment);
  public static void main(String[] args) throws Exception {
    run();
  }
}
