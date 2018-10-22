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

import java.util.Base64;
public class Test1957 {

  static class Transform {
    public void sayHi() {
      // Use lower 'h' to make sure the string will have a different string id
      // than the transformation (the transformation code is the same except
      // the actual printed String, which was making the test inacurately passing
      // in JIT mode when loading the string from the dex cache, as the string ids
      // of the two different strings were the same).
      // We know the string ids will be different because lexicographically:
      // "Goodbye" < "LTransform;" < "hello".
      System.out.println("hello");
    }
  }

  /**
   * base64 encoded class/dex file for
   * class Transform {
   * }
   */
  private static final byte[] CLASS_BYTES = Base64.getDecoder().decode(
    "yv66vgAAADUAEQoAAwAKBwAMBwAPAQAGPGluaXQ+AQADKClWAQAEQ29kZQEAD0xpbmVOdW1iZXJU" +
    "YWJsZQEAClNvdXJjZUZpbGUBAA1UZXN0MTk1Ny5qYXZhDAAEAAUHABABABZhcnQvVGVzdDE5NTck" +
    "VHJhbnNmb3JtAQAJVHJhbnNmb3JtAQAMSW5uZXJDbGFzc2VzAQAQamF2YS9sYW5nL09iamVjdAEA" +
    "DGFydC9UZXN0MTk1NwAgAAIAAwAAAAAAAQAAAAQABQABAAYAAAAdAAEAAQAAAAUqtwABsQAAAAEA" +
    "BwAAAAYAAQAAAAYAAgAIAAAAAgAJAA4AAAAKAAEAAgALAA0ACA==");
  private static final byte[] DEX_BYTES = Base64.getDecoder().decode(
    "ZGV4CjAzNQAQiK+oahCb4T18bDge0pSvp7rka4UQ2AY0AwAAcAAAAHhWNBIAAAAAAAAAAIgCAAAN" +
    "AAAAcAAAAAYAAACkAAAAAQAAALwAAAAAAAAAAAAAAAIAAADIAAAAAQAAANgAAAA8AgAA+AAAABQB" +
    "AAAcAQAANgEAAEYBAABqAQAAigEAAJ4BAACtAQAAuAEAALsBAADIAQAAzgEAANUBAAABAAAAAgAA" +
    "AAMAAAAEAAAABQAAAAgAAAAIAAAABQAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAEAAAA" +
    "AAAAAAYAAAB4AgAAWwIAAAAAAAABAAEAAQAAABABAAAEAAAAcBABAAAADgAGAA4ABjxpbml0PgAY" +
    "TGFydC9UZXN0MTk1NyRUcmFuc2Zvcm07AA5MYXJ0L1Rlc3QxOTU3OwAiTGRhbHZpay9hbm5vdGF0" +
    "aW9uL0VuY2xvc2luZ0NsYXNzOwAeTGRhbHZpay9hbm5vdGF0aW9uL0lubmVyQ2xhc3M7ABJMamF2" +
    "YS9sYW5nL09iamVjdDsADVRlc3QxOTU3LmphdmEACVRyYW5zZm9ybQABVgALYWNjZXNzRmxhZ3MA" +
    "BG5hbWUABXZhbHVlAHV+fkQ4eyJjb21waWxhdGlvbi1tb2RlIjoiZGVidWciLCJtaW4tYXBpIjox" +
    "LCJzaGEtMSI6Ijg0NjI2ZDE0MmRiMmY4NzVhY2E2YjVlOWVmYWU3OThjYWQ5ZDlhNTAiLCJ2ZXJz" +
    "aW9uIjoiMS40LjItZGV2In0AAgIBCxgBAgMCCQQIChcHAAABAACAgAT4AQAAAAAAAAACAAAATAIA" +
    "AFICAABsAgAAAAAAAAAAAAAAAAAADgAAAAAAAAABAAAAAAAAAAEAAAANAAAAcAAAAAIAAAAGAAAA" +
    "pAAAAAMAAAABAAAAvAAAAAUAAAACAAAAyAAAAAYAAAABAAAA2AAAAAEgAAABAAAA+AAAAAMgAAAB" +
    "AAAAEAEAAAIgAAANAAAAFAEAAAQgAAACAAAATAIAAAAgAAABAAAAWwIAAAMQAAACAAAAaAIAAAYg" +
    "AAABAAAAeAIAAAAQAAABAAAAiAIAAA==");

  public static void run() {
    Redefinition.setTestConfiguration(Redefinition.Config.COMMON_REDEFINE);
    Transform t = new Transform();
    System.out.println("LastError is: " + getLastErrorOrException());
    try {
      Redefinition.doCommonClassRedefinition(Transform.class, CLASS_BYTES, DEX_BYTES);
    } catch (Throwable e) {
      System.out.println("Got " + e.getClass().toString() + ": " + e.getMessage());
    }
    System.out.println("LastError is: " + getLastErrorOrException());
    clearLastError();
    System.out.println("LastError is: " + getLastErrorOrException());
  }

  public static String getLastErrorOrException() {
    try {
      return getLastError();
    } catch (Throwable t) {
      return "<call returned error: " + t.getClass().toString() + ": " + t.getMessage() + ">";
    }
  }
  public static native String getLastError();
  public static native void clearLastError();
}
