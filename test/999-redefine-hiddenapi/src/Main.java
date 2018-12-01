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

import java.io.File;
import java.lang.reflect.Method;
import java.util.Base64;

public class Main {
  public static void main(String[] args) throws ClassNotFoundException {
    System.loadLibrary(args[0]);

    // Run the initialization routine. This will enable hidden API checks in
    // the runtime, in case they are not enabled by default.
    init();

    // Load the '-ex' APK and attach it to the boot class path.
    appendToBootClassLoader(DEX_EXTRA);

    // Find the test class in boot class loader and verify that its members are hidden.
    Class<?> klass = Class.forName("art.Test999", true, BOOT_CLASS_LOADER);
    assertFieldIsHidden(klass, "before redefinition");
    assertMethodIsHidden(klass, "before redefinition");

    // Redefine the class using JVMTI. Use dex file without hiddenapi flags.
    art.Redefinition.setTestConfiguration(art.Redefinition.Config.COMMON_REDEFINE);
    art.Redefinition.doCommonClassRedefinition(klass, CLASS_BYTES, DEX_BYTES);

    // Verify that the class members are still hidden.
    assertFieldIsHidden(klass, "after first redefinition");
    assertMethodIsHidden(klass, "after first redefinition");
  }

  private static void assertMethodIsHidden(Class<?> klass, String msg) {
    try {
      klass.getDeclaredMethod("foo");
      // Unexpected. Should have thrown NoSuchMethodException.
      throw new RuntimeException("Method should not be accessible " + msg);
    } catch (NoSuchMethodException ex) {
    }
  }

  private static void assertFieldIsHidden(Class<?> klass, String msg) {
    try {
      klass.getDeclaredField("bar");
      // Unexpected. Should have thrown NoSuchFieldException.
      throw new RuntimeException("Field should not be accessible " + msg);
    } catch (NoSuchFieldException ex) {
    }
  }

  private static final String DEX_EXTRA =
      new File(System.getenv("DEX_LOCATION"), "999-redefine-hiddenapi-ex.jar").getAbsolutePath();

  private static ClassLoader BOOT_CLASS_LOADER = Object.class.getClassLoader();

  // Native functions. Note that these are implemented in 674-hiddenapi/hiddenapi.cc.
  private static native void appendToBootClassLoader(String dexPath);
  private static native void init();

  /**
   * base64 encoded class/dex file for
   *
   * public class Test999 {
   *   public void foo() {
   *     System.out.println("Goodbye");
   *   }
   *
   *   public int bar = 64;
   * }
   */
  private static final byte[] CLASS_BYTES = Base64.getDecoder().decode(
    "yv66vgAAADUAIAoABwARCQAGABIJABMAFAgAFQoAFgAXBwAYBwAZAQADYmFyAQABSQEABjxpbml0" +
    "PgEAAygpVgEABENvZGUBAA9MaW5lTnVtYmVyVGFibGUBAANmb28BAApTb3VyY2VGaWxlAQAMVGVz" +
    "dDk5OS5qYXZhDAAKAAsMAAgACQcAGgwAGwAcAQAHR29vZGJ5ZQcAHQwAHgAfAQALYXJ0L1Rlc3Q5" +
    "OTkBABBqYXZhL2xhbmcvT2JqZWN0AQAQamF2YS9sYW5nL1N5c3RlbQEAA291dAEAFUxqYXZhL2lv" +
    "L1ByaW50U3RyZWFtOwEAE2phdmEvaW8vUHJpbnRTdHJlYW0BAAdwcmludGxuAQAVKExqYXZhL2xh" +
    "bmcvU3RyaW5nOylWACEABgAHAAAAAQABAAgACQAAAAIAAQAKAAsAAQAMAAAAJwACAAEAAAALKrcA" +
    "ASoQQLUAArEAAAABAA0AAAAKAAIAAAATAAQAGAABAA4ACwABAAwAAAAlAAIAAQAAAAmyAAMSBLYA" +
    "BbEAAAABAA0AAAAKAAIAAAAVAAgAFgABAA8AAAACABA=");
  private static final byte[] DEX_BYTES = Base64.getDecoder().decode(
    "ZGV4CjAzNQDlfmgFfKulToQpDF+P4dsgeOkgfzzH+5lgAwAAcAAAAHhWNBIAAAAAAAAAALQCAAAQ" +
    "AAAAcAAAAAcAAACwAAAAAgAAAMwAAAACAAAA5AAAAAQAAAD0AAAAAQAAABQBAAAsAgAANAEAAIYB" +
    "AACOAQAAlwEAAJoBAACpAQAAwAEAANQBAADoAQAA/AEAAAoCAAANAgAAEQIAABYCAAAbAgAAIAIA" +
    "ACkCAAACAAAAAwAAAAQAAAAFAAAABgAAAAcAAAAJAAAACQAAAAYAAAAAAAAACgAAAAYAAACAAQAA" +
    "AQAAAAsAAAAFAAIADQAAAAEAAAAAAAAAAQAAAAwAAAACAAEADgAAAAMAAAAAAAAAAQAAAAEAAAAD" +
    "AAAAAAAAAAgAAAAAAAAAoAIAAAAAAAACAAEAAQAAAHQBAAAIAAAAcBADAAEAEwBAAFkQAAAOAAMA" +
    "AQACAAAAeQEAAAgAAABiAAEAGgEBAG4gAgAQAA4AEwAOQAAVAA54AAAAAQAAAAQABjxpbml0PgAH" +
    "R29vZGJ5ZQABSQANTGFydC9UZXN0OTk5OwAVTGphdmEvaW8vUHJpbnRTdHJlYW07ABJMamF2YS9s" +
    "YW5nL09iamVjdDsAEkxqYXZhL2xhbmcvU3RyaW5nOwASTGphdmEvbGFuZy9TeXN0ZW07AAxUZXN0" +
    "OTk5LmphdmEAAVYAAlZMAANiYXIAA2ZvbwADb3V0AAdwcmludGxuAHV+fkQ4eyJjb21waWxhdGlv" +
    "bi1tb2RlIjoiZGVidWciLCJtaW4tYXBpIjoxLCJzaGEtMSI6ImQyMmFiNGYxOWI3NTYxNDQ3NTI4" +
    "NTdjYTg2YjJjZWU0ZGQ5Y2ExNjYiLCJ2ZXJzaW9uIjoiMS40LjktZGV2In0AAAEBAQABAIGABLQC" +
    "AQHUAgAAAAAOAAAAAAAAAAEAAAAAAAAAAQAAABAAAABwAAAAAgAAAAcAAACwAAAAAwAAAAIAAADM" +
    "AAAABAAAAAIAAADkAAAABQAAAAQAAAD0AAAABgAAAAEAAAAUAQAAASAAAAIAAAA0AQAAAyAAAAIA" +
    "AAB0AQAAARAAAAEAAACAAQAAAiAAABAAAACGAQAAACAAAAEAAACgAgAAAxAAAAEAAACwAgAAABAA" +
    "AAEAAAC0AgAA");
}
