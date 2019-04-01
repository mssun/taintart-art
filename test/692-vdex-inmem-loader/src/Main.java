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

import dalvik.system.InMemoryDexClassLoader;
import java.lang.reflect.Method;
import java.io.File;
import java.nio.ByteBuffer;
import java.util.Base64;

public class Main {
  private static void check(boolean expected, boolean actual, String message) {
    if (expected != actual) {
      System.err.println(
          "ERROR: " + message + " (expected=" + expected + ", actual=" + actual + ")");
    }
  }

  private static ClassLoader singleLoader() {
    return new InMemoryDexClassLoader(
        new ByteBuffer[] { ByteBuffer.wrap(DEX_BYTES_A), ByteBuffer.wrap(DEX_BYTES_B) },
        /*parent*/null);
  }

  private static ClassLoader[] multiLoader() {
    ClassLoader clA = new InMemoryDexClassLoader(ByteBuffer.wrap(DEX_BYTES_A), /*parent*/ null);
    ClassLoader clB = new InMemoryDexClassLoader(ByteBuffer.wrap(DEX_BYTES_B), /*parent*/ clA);
    return new ClassLoader[] { clA, clB };
  }

  private static void test(ClassLoader loader, boolean invokeMethod) throws Exception {
    waitForVerifier();
    check(true, areClassesVerified(loader), "areClassesVerified");

    if (invokeMethod) {
      loader.loadClass("art.ClassB").getDeclaredMethod("printHello").invoke(null);
    }
  }

  public static void main(String[] args) throws Exception {
    System.loadLibrary(args[0]);
    ClassLoader[] loaders = null;

    // Test loading both dex files in a single class loader.
    // Background verification task should verify all their classes.
    test(singleLoader(), /*invokeMethod*/true);

    // Test loading the two dex files with separate class loaders.
    // Background verification task should still verify all classes.
    loaders = multiLoader();
    test(loaders[0], /*invokeMethod*/false);
    test(loaders[1], /*invokeMethod*/true);
  }

  private static native void waitForVerifier();
  private static native boolean areClassesVerified(ClassLoader loader);

  // Defined in 674-hiddenapi.
  private static native void appendToBootClassLoader(String dexPath, boolean isCorePlatform);

  private static final String DEX_LOCATION = System.getenv("DEX_LOCATION");
  private static final String DEX_EXTRA =
      new File(DEX_LOCATION, "692-vdex-inmem-loader-ex.jar").getAbsolutePath();

  private static final byte[] DEX_BYTES_A = Base64.getDecoder().decode(
    "ZGV4CjAzNQBxYu/tdPfiHaRPYr5yaT6ko9V/xMinr1OwAgAAcAAAAHhWNBIAAAAAAAAAABwCAAAK" +
    "AAAAcAAAAAQAAACYAAAAAgAAAKgAAAAAAAAAAAAAAAMAAADAAAAAAQAAANgAAAC4AQAA+AAAADAB" +
    "AAA4AQAARQEAAEwBAABPAQAAXQEAAHEBAACFAQAAiAEAAJIBAAAEAAAABQAAAAYAAAAHAAAAAwAA" +
    "AAIAAAAAAAAABwAAAAMAAAAAAAAAAAABAAAAAAAAAAAACAAAAAEAAQAAAAAAAAAAAAEAAAABAAAA" +
    "AAAAAAEAAAAAAAAACQIAAAAAAAABAAAAAAAAACwBAAADAAAAGgACABEAAAABAAEAAQAAACgBAAAE" +
    "AAAAcBACAAAADgATAA4AFQAOAAY8aW5pdD4AC0NsYXNzQS5qYXZhAAVIZWxsbwABTAAMTGFydC9D" +
    "bGFzc0E7ABJMamF2YS9sYW5nL09iamVjdDsAEkxqYXZhL2xhbmcvU3RyaW5nOwABVgAIZ2V0SGVs" +
    "bG8AdX5+RDh7ImNvbXBpbGF0aW9uLW1vZGUiOiJkZWJ1ZyIsIm1pbi1hcGkiOjEsInNoYS0xIjoi" +
    "OTY2MDhmZDdiYmNjZGQyMjc2Y2Y4OTI4M2QyYjgwY2JmYzRmYzgxYyIsInZlcnNpb24iOiIxLjUu" +
    "NC1kZXYifQAAAAIAAIGABJACAQn4AQAAAAAADAAAAAAAAAABAAAAAAAAAAEAAAAKAAAAcAAAAAIA" +
    "AAAEAAAAmAAAAAMAAAACAAAAqAAAAAUAAAADAAAAwAAAAAYAAAABAAAA2AAAAAEgAAACAAAA+AAA" +
    "AAMgAAACAAAAKAEAAAIgAAAKAAAAMAEAAAAgAAABAAAACQIAAAMQAAABAAAAGAIAAAAQAAABAAAA" +
    "HAIAAA==");
  private static final byte[] DEX_BYTES_B = Base64.getDecoder().decode(
    "ZGV4CjAzNQB+hWvce73hXt7ZVNgp9RAyMLSwQzsWUjV4AwAAcAAAAHhWNBIAAAAAAAAAAMwCAAAQ" +
    "AAAAcAAAAAcAAACwAAAAAwAAAMwAAAABAAAA8AAAAAUAAAD4AAAAAQAAACABAAA4AgAAQAEAAI4B" +
    "AACWAQAAowEAAKYBAAC0AQAAwgEAANkBAADtAQAAAQIAABUCAAAYAgAAHAIAACYCAAArAgAANwIA" +
    "AEACAAADAAAABAAAAAUAAAAGAAAABwAAAAgAAAAJAAAAAgAAAAQAAAAAAAAACQAAAAYAAAAAAAAA" +
    "CgAAAAYAAACIAQAABQACAAwAAAAAAAAACwAAAAEAAQAAAAAAAQABAA0AAAACAAIADgAAAAMAAQAA" +
    "AAAAAQAAAAEAAAADAAAAAAAAAAEAAAAAAAAAtwIAAAAAAAABAAEAAQAAAHwBAAAEAAAAcBAEAAAA" +
    "DgACAAAAAgAAAIABAAAKAAAAYgAAAHEAAAAAAAwBbiADABAADgATAA4AFQAOlgAAAAABAAAABAAG" +
    "PGluaXQ+AAtDbGFzc0IuamF2YQABTAAMTGFydC9DbGFzc0E7AAxMYXJ0L0NsYXNzQjsAFUxqYXZh" +
    "L2lvL1ByaW50U3RyZWFtOwASTGphdmEvbGFuZy9PYmplY3Q7ABJMamF2YS9sYW5nL1N0cmluZzsA" +
    "EkxqYXZhL2xhbmcvU3lzdGVtOwABVgACVkwACGdldEhlbGxvAANvdXQACnByaW50SGVsbG8AB3By" +
    "aW50bG4AdX5+RDh7ImNvbXBpbGF0aW9uLW1vZGUiOiJkZWJ1ZyIsIm1pbi1hcGkiOjEsInNoYS0x" +
    "IjoiOTY2MDhmZDdiYmNjZGQyMjc2Y2Y4OTI4M2QyYjgwY2JmYzRmYzgxYyIsInZlcnNpb24iOiIx" +
    "LjUuNC1kZXYifQAAAAIAAYGABMACAQnYAgAAAAAAAAAOAAAAAAAAAAEAAAAAAAAAAQAAABAAAABw" +
    "AAAAAgAAAAcAAACwAAAAAwAAAAMAAADMAAAABAAAAAEAAADwAAAABQAAAAUAAAD4AAAABgAAAAEA" +
    "AAAgAQAAASAAAAIAAABAAQAAAyAAAAIAAAB8AQAAARAAAAEAAACIAQAAAiAAABAAAACOAQAAACAA" +
    "AAEAAAC3AgAAAxAAAAEAAADIAgAAABAAAAEAAADMAgAA");
}
