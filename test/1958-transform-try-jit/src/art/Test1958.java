/*
 * Copyright (C) 2016 The Android Open Source Project
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

package art;

import java.lang.reflect.Method;
import java.util.Base64;

public class Test1958 {
  static class Runner {
    public static String doSayHi() {
      return sayHiHelper(true);
    }

    public static String dontSayHi() {
      return sayHiHelper(false);
    }

    // We are trying to get the definition of Transform.sayHi inlined into this function.
    public static String sayHiHelper(boolean sayHi) {
      if (sayHi) {
        return Transform.sayHi();
      } else {
        return "NOPE!";
      }
    }
  }

  static class Transform {
    public static String sayHi() {
      // Use lower 'h' to make sure the string will have a different string id
      // than the transformation (the transformation code is the same except
      // the actual printed String, which was making the test inacurately passing
      // in JIT mode when loading the string from the dex cache, as the string ids
      // of the two different strings were the same).
      // We know the string ids will be different because lexicographically:
      // "Goodbye" < "LTransform;" < "hello".
      return "hello";
    }
  }

  /**
   * base64 encoded class/dex file for
   * static class Transform {
   *   public static String sayHi() {
   *    return "Goodbye";
   *   }
   * }
   */
  private static final byte[] CLASS_BYTES = Base64.getDecoder().decode(
    "yv66vgAAADUAFQoABAANCAAOBwAQBwATAQAGPGluaXQ+AQADKClWAQAEQ29kZQEAD0xpbmVOdW1i" +
    "ZXJUYWJsZQEABXNheUhpAQAUKClMamF2YS9sYW5nL1N0cmluZzsBAApTb3VyY2VGaWxlAQANVGVz" +
    "dDE5NTguamF2YQwABQAGAQAHR29vZGJ5ZQcAFAEAFmFydC9UZXN0MTk1OCRUcmFuc2Zvcm0BAAlU" +
    "cmFuc2Zvcm0BAAxJbm5lckNsYXNzZXMBABBqYXZhL2xhbmcvT2JqZWN0AQAMYXJ0L1Rlc3QxOTU4" +
    "ACAAAwAEAAAAAAACAAAABQAGAAEABwAAAB0AAQABAAAABSq3AAGxAAAAAQAIAAAABgABAAAABgAJ" +
    "AAkACgABAAcAAAAbAAEAAAAAAAMSArAAAAABAAgAAAAGAAEAAAAIAAIACwAAAAIADAASAAAACgAB" +
    "AAMADwARAAg=");
  private static final byte[] DEX_BYTES = Base64.getDecoder().decode(
    "ZGV4CjAzNQCRmEaLPLzpKe+CHcDM8YhIJCPWwcFR5yegAwAAcAAAAHhWNBIAAAAAAAAAAPQCAAAR" +
    "AAAAcAAAAAcAAAC0AAAAAgAAANAAAAAAAAAAAAAAAAMAAADoAAAAAQAAAAABAACAAgAAIAEAAFgB" +
    "AABgAQAAaQEAAGwBAACGAQAAlgEAALoBAADaAQAA7gEAAAICAAARAgAAHAIAAB8CAAAsAgAAMgIA" +
    "ADkCAABAAgAAAwAAAAQAAAAFAAAABgAAAAcAAAAIAAAACwAAAAIAAAAFAAAAAAAAAAsAAAAGAAAA" +
    "AAAAAAAAAQAAAAAAAAAAAA4AAAAEAAEAAAAAAAAAAAAAAAAABAAAAAAAAAAJAAAA5AIAAMYCAAAA" +
    "AAAAAQAAAAAAAABUAQAAAwAAABoAAQARAAAAAQABAAEAAABQAQAABAAAAHAQAgAAAA4ABgAOAAgA" +
    "DgAGPGluaXQ+AAdHb29kYnllAAFMABhMYXJ0L1Rlc3QxOTU4JFRyYW5zZm9ybTsADkxhcnQvVGVz" +
    "dDE5NTg7ACJMZGFsdmlrL2Fubm90YXRpb24vRW5jbG9zaW5nQ2xhc3M7AB5MZGFsdmlrL2Fubm90" +
    "YXRpb24vSW5uZXJDbGFzczsAEkxqYXZhL2xhbmcvT2JqZWN0OwASTGphdmEvbGFuZy9TdHJpbmc7" +
    "AA1UZXN0MTk1OC5qYXZhAAlUcmFuc2Zvcm0AAVYAC2FjY2Vzc0ZsYWdzAARuYW1lAAVzYXlIaQAF" +
    "dmFsdWUAdX5+RDh7ImNvbXBpbGF0aW9uLW1vZGUiOiJkZWJ1ZyIsIm1pbi1hcGkiOjEsInNoYS0x" +
    "IjoiNTFjYWNlMWFiZGQwOGIzMjBmMjVmYjgxMTZjMjQzMmIwMmYwOTI5NSIsInZlcnNpb24iOiIx" +
    "LjQuNS1kZXYifQACAgEPGAECAwIMBAgNFwoAAAIAAICABLgCAQmgAgAAAAACAAAAtwIAAL0CAADY" +
    "AgAAAAAAAAAAAAAAAAAADgAAAAAAAAABAAAAAAAAAAEAAAARAAAAcAAAAAIAAAAHAAAAtAAAAAMA" +
    "AAACAAAA0AAAAAUAAAADAAAA6AAAAAYAAAABAAAAAAEAAAEgAAACAAAAIAEAAAMgAAACAAAAUAEA" +
    "AAIgAAARAAAAWAEAAAQgAAACAAAAtwIAAAAgAAABAAAAxgIAAAMQAAACAAAA1AIAAAYgAAABAAAA" +
    "5AIAAAAQAAABAAAA9AIAAA==");

  public static void run() throws Exception {
    Redefinition.setTestConfiguration(Redefinition.Config.COMMON_REDEFINE);
    Method doSayHi = Runner.class.getDeclaredMethod("doSayHi");
    Method dontSayHi = Runner.class.getDeclaredMethod("dontSayHi");
    // Run the method enough times that the jit thinks it's interesting (default 10000).
    for (int i = 0; i < 20000; i++) {
      doSayHi.invoke(null);
      dontSayHi.invoke(null);
    }
    // Sleep for 10 seconds to let the jit finish any work it's doing.
    Thread.sleep(10 * 1000);
    // Check what we get right now.
    System.out.println("Before redefinition: " + doSayHi.invoke(null));
    Redefinition.doCommonClassRedefinition(Transform.class, CLASS_BYTES, DEX_BYTES);
    System.out.println("After redefinition: " + doSayHi.invoke(null));
  }
}
