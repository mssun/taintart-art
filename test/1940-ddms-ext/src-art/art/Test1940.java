/*
 * Copyright (C) 2017 The Android Open Source Project
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

import org.apache.harmony.dalvik.ddmc.*;

import java.lang.reflect.Method;
import java.util.Arrays;
import java.util.zip.Adler32;
import java.nio.*;

public class Test1940 {
  public static final int DDMS_TYPE_INDEX = 0;
  public static final int DDMS_LEN_INDEX = 4;
  public static final int DDMS_HEADER_LENGTH = 8;
  public static final int MY_DDMS_TYPE = 0xDEADBEEF;
  public static final int MY_DDMS_RESPONSE_TYPE = 0xFADE7357;

  public static final class TestError extends Error {
    public TestError(String s) { super(s); }
  }

  private static void checkEq(Object a, Object b) {
    if (!a.equals(b)) {
      throw new TestError("Failure: " + a + " != " + b);
    }
  }

  private static String printChunk(Chunk k) {
    byte[] out = new byte[k.length];
    System.arraycopy(k.data, k.offset, out, 0, k.length);
    return String.format("Chunk(Type: 0x%X, Len: %d, data: %s)",
        k.type, k.length, Arrays.toString(out));
  }

  private static final class MyDdmHandler extends ChunkHandler {
    public void connected() {}
    public void disconnected() {}
    public Chunk handleChunk(Chunk req) {
      // For this test we will simply calculate the checksum
      checkEq(req.type, MY_DDMS_TYPE);
      System.out.println("MyDdmHandler: Chunk received: " + printChunk(req));
      ByteBuffer b = ByteBuffer.wrap(new byte[8]);
      Adler32 a = new Adler32();
      a.update(req.data, req.offset, req.length);
      b.order(ByteOrder.BIG_ENDIAN);
      long val = a.getValue();
      b.putLong(val);
      System.out.printf("MyDdmHandler: Putting value 0x%X\n", val);
      Chunk ret = new Chunk(MY_DDMS_RESPONSE_TYPE, b.array(), 0, 8);
      System.out.println("MyDdmHandler: Chunk returned: " + printChunk(ret));
      return ret;
    }
  }

  public static final ChunkHandler SINGLE_HANDLER = new MyDdmHandler();

  public static void HandlePublish(int type, byte[] data) {
    System.out.println("Chunk published: " + printChunk(new Chunk(type, data, 0, data.length)));
  }

  public static void run() throws Exception {
    initializeTest(
        Test1940.class,
        Test1940.class.getDeclaredMethod("HandlePublish", Integer.TYPE, new byte[0].getClass()));
    // Test sending chunk directly.
    DdmServer.registerHandler(MY_DDMS_TYPE, SINGLE_HANDLER);
    DdmServer.registrationComplete();
    byte[] data = new byte[] { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };
    System.out.println("Sending data " + Arrays.toString(data));
    Chunk res = processChunk(data);
    System.out.println("JVMTI returned chunk: " + printChunk(res));

    // Test sending chunk through DdmServer#sendChunk
    Chunk c = new Chunk(
        MY_DDMS_TYPE, new byte[] { 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10 }, 0, 8);
    System.out.println("Sending chunk: " + printChunk(c));
    DdmServer.sendChunk(c);
  }

  private static Chunk processChunk(byte[] val) {
    return processChunk(new Chunk(MY_DDMS_TYPE, val, 0, val.length));
  }

  private static native void initializeTest(Class<?> k, Method m);
  private static native Chunk processChunk(Chunk val);
}
