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

  private static void test(ClassLoader loader,
                           boolean expectedHasVdexFile,
                           boolean expectedBackedByOat,
                           boolean invokeMethod) throws Exception {
    // If ART created a vdex file, it must have verified all the classes.
    // That happens if and only if we expect a vdex at the end of the test but
    // do not expect it to have been loaded.
    boolean expectedClassesVerified = expectedHasVdexFile && !expectedBackedByOat;

    waitForVerifier();
    check(expectedClassesVerified, areClassesVerified(loader), "areClassesVerified");
    check(expectedHasVdexFile, hasVdexFile(loader), "areClassesVerified");
    check(expectedBackedByOat, isBackedByOatFile(loader), "isBackedByOatFile");
    check(expectedBackedByOat, areClassesPreverified(loader), "areClassesPreverified");

    if (invokeMethod) {
      loader.loadClass("art.ClassB").getDeclaredMethod("printHello").invoke(null);
    }
  }

  public static void main(String[] args) throws Exception {
    System.loadLibrary(args[0]);
    ClassLoader[] loaders = null;

    // Feature only enabled for target SDK version Q and later.
    setTargetSdkVersion(/* Q */ 29);

    // Feature is disabled in debuggable mode because runtime threads are not
    // allowed to load classes.
    boolean featureEnabled = !isDebuggable();

    // Data directory not set. Background verification job should not have run
    // and vdex should not have been created.
    test(singleLoader(), /*hasVdex*/ false, /*backedByOat*/ false, /*invokeMethod*/ true);

    // Set data directory for this process.
    setProcessDataDir(DEX_LOCATION);

    // Data directory is now set. Background verification job should have run,
    // should have verified classes and written results to a vdex.
    test(singleLoader(), /*hasVdex*/ featureEnabled, /*backedByOat*/ false, /*invokeMethod*/ true);
    test(singleLoader(), /*hasVdex*/ featureEnabled, /*backedByOat*/ featureEnabled,
        /*invokeMethod*/ true);

    // Test loading the two dex files with separate class loaders.
    // Background verification task should still verify all classes.
    loaders = multiLoader();
    test(loaders[0], /*hasVdex*/ featureEnabled, /*backedByOat*/ false, /*invokeMethod*/ false);
    test(loaders[1], /*hasVdex*/ featureEnabled, /*backedByOat*/ false, /*invokeMethod*/ true);

    loaders = multiLoader();
    test(loaders[0], /*hasVdex*/ featureEnabled, /*backedByOat*/ featureEnabled,
        /*invokeMethod*/ false);
    test(loaders[1], /*hasVdex*/ featureEnabled, /*backedByOat*/ featureEnabled,
        /*invokeMethod*/ true);

    // Change boot classpath checksum.
    appendToBootClassLoader(DEX_EXTRA, /*isCorePlatform*/ false);

    loaders = multiLoader();
    test(loaders[0], /*hasVdex*/ featureEnabled, /*backedByOat*/ false, /*invokeMethod*/ false);
    test(loaders[1], /*hasVdex*/ featureEnabled, /*backedByOat*/ false, /*invokeMethod*/ true);

    loaders = multiLoader();
    test(loaders[0], /*hasVdex*/ featureEnabled, /*backedByOat*/ featureEnabled,
        /*invokeMethod*/ false);
    test(loaders[1], /*hasVdex*/ featureEnabled, /*backedByOat*/ featureEnabled,
        /*invokeMethod*/ true);
  }

  private static native boolean isDebuggable();
  private static native int setTargetSdkVersion(int version);
  private static native void setProcessDataDir(String path);
  private static native void waitForVerifier();
  private static native boolean areClassesVerified(ClassLoader loader);
  private static native boolean hasVdexFile(ClassLoader loader);
  private static native boolean isBackedByOatFile(ClassLoader loader);
  private static native boolean areClassesPreverified(ClassLoader loader);

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
