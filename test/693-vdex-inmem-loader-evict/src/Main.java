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
  public static void main(String[] args) throws Exception {
    System.loadLibrary(args[0]);

    // Feature only enabled for target SDK version Q and later.
    setTargetSdkVersion(/* Q */ 29);

    if (isDebuggable()) {
      // Background verification is disabled in debuggable mode. This test makes
      // no sense then.
      return;
    }

    if (!hasOatFile()) {
      // We only generate vdex files if the oat directories are created.
      return;
    }

    setProcessDataDir(DEX_LOCATION);

    final int maxCacheSize = getVdexCacheSize();
    final int numDexFiles = DEX_BYTES_CHECKSUMS.length;
    if (numDexFiles <= maxCacheSize) {
      throw new IllegalStateException("Not enough dex files to test cache eviction");
    }

    // Simply load each dex file one by one.
    check(0, getCurrentCacheSize(), "There should be no vdex files in the beginning");
    for (int i = 0; i < numDexFiles; ++i) {
      ClassLoader loader = loadDex(i);
      waitForVerifier();
      check(true, hasVdexFile(loader), "Loading dex file should have produced a vdex");
      check(Math.min(i + 1, maxCacheSize), getCurrentCacheSize(),
          "Unexpected number of cache entries");
    }

    // More complicated pattern where some dex files get reused.
    for (int s = 1; s < numDexFiles; ++s) {
      for (int i = 0; i < maxCacheSize; ++i) {
        ClassLoader loader = loadDex(i);
        waitForVerifier();
        check(true, hasVdexFile(loader), "Loading dex file should have produced a vdex");
        check(maxCacheSize, getCurrentCacheSize(), "Unexpected number of cache entries");
      }
    }
  }

  private static native boolean isDebuggable();
  private static native boolean hasOatFile();
  private static native int setTargetSdkVersion(int version);
  private static native void setProcessDataDir(String path);
  private static native void waitForVerifier();
  private static native boolean hasVdexFile(ClassLoader loader);
  private static native int getVdexCacheSize();
  private static native boolean isAnonymousVdexBasename(String basename);

  private static <T> void check(T expected, T actual, String message) {
    if (!expected.equals(actual)) {
      System.err.println("ERROR: " + message + " (expected=" + expected.toString() +
          ", actual=" + actual.toString() + ")");
    }
  }

  private static int getCurrentCacheSize() {
    int count = 0;
    File folder = new File(DEX_LOCATION, "oat");
    File[] subfolders = folder.listFiles();
    if (subfolders.length != 1) {
      throw new IllegalStateException("Expect only one subfolder - isa");
    }
    folder = subfolders[0];
    for (File f : folder.listFiles()) {
      if (f.isFile() && isAnonymousVdexBasename(f.getName())) {
        count++;
      }
    }
    return count;
  }

  private static byte[] createDex(int index) {
    if (index >= 100) {
      throw new IllegalArgumentException("Not more than two decimals");
    }

    // Clone the base dex file. This is the dex file for index 0 (class ID "01").
    byte[] dex = DEX_BYTES_BASE.clone();

    // Overwrite the checksum and sha1 signature.
    System.arraycopy(DEX_BYTES_CHECKSUMS[index], 0, dex, DEX_BYTES_CHECKSUM_OFFSET,
        DEX_BYTES_CHECKSUM_SIZE);

    // Check that the class ID offsets match expectations - they should contains "01".
    if (dex[DEX_BYTES_CLASS_ID_OFFSET1 + 0] != 0x30 ||
        dex[DEX_BYTES_CLASS_ID_OFFSET1 + 1] != 0x31 ||
        dex[DEX_BYTES_CLASS_ID_OFFSET2 + 0] != 0x30 ||
        dex[DEX_BYTES_CLASS_ID_OFFSET2 + 1] != 0x31) {
      throw new IllegalStateException("Wrong class name values");
    }

    // Overwrite class ID.
    byte str_id1 = (byte) (0x30 + ((index + 1) / 10));
    byte str_id2 = (byte) (0x30 + ((index + 1) % 10));
    dex[DEX_BYTES_CLASS_ID_OFFSET1 + 0] = str_id1;
    dex[DEX_BYTES_CLASS_ID_OFFSET1 + 1] = str_id2;
    dex[DEX_BYTES_CLASS_ID_OFFSET2 + 0] = str_id1;
    dex[DEX_BYTES_CLASS_ID_OFFSET2 + 1] = str_id2;

    return dex;
  }

  private static ClassLoader loadDex(int index) {
    return new InMemoryDexClassLoader(ByteBuffer.wrap(createDex(index)), /*parent*/ null);
  }

  private static final String DEX_LOCATION = System.getenv("DEX_LOCATION");

  private static final int DEX_BYTES_CLASS_ID_OFFSET1 = 0xfd;
  private static final int DEX_BYTES_CLASS_ID_OFFSET2 = 0x11d;
  private static final int DEX_BYTES_CHECKSUM_OFFSET = 8;
  private static final int DEX_BYTES_CHECKSUM_SIZE = 24;

  // Dex file for: "public class MyClass01 {}".
  private static final byte[] DEX_BYTES_BASE = Base64.getDecoder().decode(
    "ZGV4CjAzNQBHVjDjQ9WQ2TSezZ0exFH00hvlJrenqvNEAgAAcAAAAHhWNBIAAAAAAAAAALABAAAG" +
    "AAAAcAAAAAMAAACIAAAAAQAAAJQAAAAAAAAAAAAAAAIAAACgAAAAAQAAALAAAAB0AQAA0AAAAOwA" +
    "AAD0AAAAAQEAABUBAAAlAQAAKAEAAAEAAAACAAAABAAAAAQAAAACAAAAAAAAAAAAAAAAAAAAAQAA" +
    "AAAAAAAAAAAAAQAAAAEAAAAAAAAAAwAAAAAAAACfAQAAAAAAAAEAAQABAAAA6AAAAAQAAABwEAEA" +
    "AAAOAAEADgAGPGluaXQ+AAtMTXlDbGFzczAxOwASTGphdmEvbGFuZy9PYmplY3Q7AA5NeUNsYXNz" +
    "MDEuamF2YQABVgB1fn5EOHsiY29tcGlsYXRpb24tbW9kZSI6ImRlYnVnIiwibWluLWFwaSI6MSwi" +
    "c2hhLTEiOiI4ZjI5NTlkMDExNmMyYjdmZTZlMDUxNWQ3MTQxZTRmMGY0ZTczYzBiIiwidmVyc2lv" +
    "biI6IjEuNS41LWRldiJ9AAAAAQAAgYAE0AEAAAAAAAAADAAAAAAAAAABAAAAAAAAAAEAAAAGAAAA" +
    "cAAAAAIAAAADAAAAiAAAAAMAAAABAAAAlAAAAAUAAAACAAAAoAAAAAYAAAABAAAAsAAAAAEgAAAB" +
    "AAAA0AAAAAMgAAABAAAA6AAAAAIgAAAGAAAA7AAAAAAgAAABAAAAnwEAAAMQAAABAAAArAEAAAAQ" +
    "AAABAAAAsAEAAA==");

  // Checksum/SHA1 signature diff for classes MyClass01 - MyClassXX.
  // This is just a convenient way of storing many similar dex files.
  private static final byte[][] DEX_BYTES_CHECKSUMS = new byte[][] {
    Base64.getDecoder().decode("R1Yw40PVkNk0ns2dHsRR9NIb5Sa3p6rz"),
    Base64.getDecoder().decode("i1V1U3C8nexVk4uw185lXZd9kzd82iaA"),
    Base64.getDecoder().decode("tFPbVPdpzuoDWqH71Ak5HpltBHg0frMU"),
    Base64.getDecoder().decode("eFSc7dENiK8nxviKBmd/O2s7h/NAj+l/"),
    Base64.getDecoder().decode("DlUfNQ3cuVrCHRyw/cOFhqEe+0r6wlUP"),
    Base64.getDecoder().decode("KVaBmdG8Y8kx8ltEPXWyi9OCdL14yeiW"),
    Base64.getDecoder().decode("K1bioDTHtPwmrPXkvZ0XYCiripH6KsC2"),
    Base64.getDecoder().decode("oVHctdpHG3YTNeQlVCshTkFKVra9TG4k"),
    Base64.getDecoder().decode("eVWMFHRY+w4lpn9Uo9jn+eNAmaRK4HEw"),
    Base64.getDecoder().decode("/lW3Q3U4ot5A2qkhiv4Aj+s8zv7984MA"),
    Base64.getDecoder().decode("BFRB+4HwRbuD164DB3sVy28dc+Ea5YVQ"),
    Base64.getDecoder().decode("klQBLEXyr0cviHDHlqFyWPGKaQQnqMiD"),
    Base64.getDecoder().decode("jlTcJAkpnbDI/E4msuvMyWqKxNMTN0YU"),
    Base64.getDecoder().decode("vlUOrp4aN0PxcaqQrQmm597P+Ymu5Adt"),
    Base64.getDecoder().decode("HlXyT1GoJk1m33O8OMaYxqy3K1Byyf1S"),
    Base64.getDecoder().decode("d1O5toIKjTXNZkgP3p9RiiafhuKw4gUH"),
    Base64.getDecoder().decode("11RsuG9UrFHPipOj9zjuGU9obctMJbq6"),
    Base64.getDecoder().decode("dlSW5egObqheoHSRthlR2c2jVKLGQ3QL"),
    Base64.getDecoder().decode("ulMgQEhC0XMhmKxHtgdURY6B6JEqNb3E"),
    Base64.getDecoder().decode("YFV08vrcs49xYr1OBhrza5H8Ha86FODz"),
    Base64.getDecoder().decode("jFKPxTFd3kn6K0p6n8YEPgm0hiozXW1p"),
    Base64.getDecoder().decode("LlUZdlCXwAn4qksYL6Urw+bZC/fYuJ1T"),
    Base64.getDecoder().decode("K1SuRt9xZX5lAVtbpMauOWLVXs2KooUA"),
    Base64.getDecoder().decode("2FJAWIk0JS9EdvkgHjquLL9qdcLeHaRJ"),
    Base64.getDecoder().decode("YVResABr9IvZLV8eeIhM3TXfGC+Y6/x1"),
    Base64.getDecoder().decode("UVTrkVGIh8u7FBHgcbS9flI0CY5g2E3m"),
    Base64.getDecoder().decode("oVIu6RsrT6HgnbPzNGiYZSpKS0cqNi+a"),
    Base64.getDecoder().decode("2FR/slWq9YC6kJRDEw21RVGmJhr3/uKZ"),
    Base64.getDecoder().decode("CFbaSi70ZVaumL7zsXWlD/ernHxCZPx6"),
    Base64.getDecoder().decode("7FTY+T1/qevWQM6Yoe+OwNcUdgcCUomJ"),
  };
}
