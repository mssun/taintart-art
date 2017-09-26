// Copyright (C) 2017 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

import java.lang.reflect.Method;

public class Test {
  // Returns list of all methods in Generated.java
  // This is to avoid having to introspect classes with extra code
  // (for example, we ignore <init> methods).
  public static Method[] getTestMethods() throws NoSuchMethodException, SecurityException {
    Method[] all_methods = new Method[72];
    all_methods[0] = A.class.getDeclaredMethod("m_a$$$");
    all_methods[1] = A.class.getDeclaredMethod("m_a$Hot$$");
    all_methods[2] = A.class.getDeclaredMethod("m_a$$Startup$");
    all_methods[3] = A.class.getDeclaredMethod("m_a$Hot$Startup$");
    all_methods[4] = A.class.getDeclaredMethod("m_a$$$Poststartup");
    all_methods[5] = A.class.getDeclaredMethod("m_a$Hot$$Poststartup");
    all_methods[6] = A.class.getDeclaredMethod("m_a$$Startup$Poststartup");
    all_methods[7] = A.class.getDeclaredMethod("m_a$Hot$Startup$Poststartup");
    all_methods[8] = A.class.getDeclaredMethod("m_b$$$");
    all_methods[9] = A.class.getDeclaredMethod("m_b$Hot$$");
    all_methods[10] = A.class.getDeclaredMethod("m_b$$Startup$");
    all_methods[11] = A.class.getDeclaredMethod("m_b$Hot$Startup$");
    all_methods[12] = A.class.getDeclaredMethod("m_b$$$Poststartup");
    all_methods[13] = A.class.getDeclaredMethod("m_b$Hot$$Poststartup");
    all_methods[14] = A.class.getDeclaredMethod("m_b$$Startup$Poststartup");
    all_methods[15] = A.class.getDeclaredMethod("m_b$Hot$Startup$Poststartup");
    all_methods[16] = A.class.getDeclaredMethod("m_c$$$");
    all_methods[17] = A.class.getDeclaredMethod("m_c$Hot$$");
    all_methods[18] = A.class.getDeclaredMethod("m_c$$Startup$");
    all_methods[19] = A.class.getDeclaredMethod("m_c$Hot$Startup$");
    all_methods[20] = A.class.getDeclaredMethod("m_c$$$Poststartup");
    all_methods[21] = A.class.getDeclaredMethod("m_c$Hot$$Poststartup");
    all_methods[22] = A.class.getDeclaredMethod("m_c$$Startup$Poststartup");
    all_methods[23] = A.class.getDeclaredMethod("m_c$Hot$Startup$Poststartup");
    all_methods[24] = B.class.getDeclaredMethod("m_a$$$");
    all_methods[25] = B.class.getDeclaredMethod("m_a$Hot$$");
    all_methods[26] = B.class.getDeclaredMethod("m_a$$Startup$");
    all_methods[27] = B.class.getDeclaredMethod("m_a$Hot$Startup$");
    all_methods[28] = B.class.getDeclaredMethod("m_a$$$Poststartup");
    all_methods[29] = B.class.getDeclaredMethod("m_a$Hot$$Poststartup");
    all_methods[30] = B.class.getDeclaredMethod("m_a$$Startup$Poststartup");
    all_methods[31] = B.class.getDeclaredMethod("m_a$Hot$Startup$Poststartup");
    all_methods[32] = B.class.getDeclaredMethod("m_b$$$");
    all_methods[33] = B.class.getDeclaredMethod("m_b$Hot$$");
    all_methods[34] = B.class.getDeclaredMethod("m_b$$Startup$");
    all_methods[35] = B.class.getDeclaredMethod("m_b$Hot$Startup$");
    all_methods[36] = B.class.getDeclaredMethod("m_b$$$Poststartup");
    all_methods[37] = B.class.getDeclaredMethod("m_b$Hot$$Poststartup");
    all_methods[38] = B.class.getDeclaredMethod("m_b$$Startup$Poststartup");
    all_methods[39] = B.class.getDeclaredMethod("m_b$Hot$Startup$Poststartup");
    all_methods[40] = B.class.getDeclaredMethod("m_c$$$");
    all_methods[41] = B.class.getDeclaredMethod("m_c$Hot$$");
    all_methods[42] = B.class.getDeclaredMethod("m_c$$Startup$");
    all_methods[43] = B.class.getDeclaredMethod("m_c$Hot$Startup$");
    all_methods[44] = B.class.getDeclaredMethod("m_c$$$Poststartup");
    all_methods[45] = B.class.getDeclaredMethod("m_c$Hot$$Poststartup");
    all_methods[46] = B.class.getDeclaredMethod("m_c$$Startup$Poststartup");
    all_methods[47] = B.class.getDeclaredMethod("m_c$Hot$Startup$Poststartup");
    all_methods[48] = C.class.getDeclaredMethod("m_a$$$");
    all_methods[49] = C.class.getDeclaredMethod("m_a$Hot$$");
    all_methods[50] = C.class.getDeclaredMethod("m_a$$Startup$");
    all_methods[51] = C.class.getDeclaredMethod("m_a$Hot$Startup$");
    all_methods[52] = C.class.getDeclaredMethod("m_a$$$Poststartup");
    all_methods[53] = C.class.getDeclaredMethod("m_a$Hot$$Poststartup");
    all_methods[54] = C.class.getDeclaredMethod("m_a$$Startup$Poststartup");
    all_methods[55] = C.class.getDeclaredMethod("m_a$Hot$Startup$Poststartup");
    all_methods[56] = C.class.getDeclaredMethod("m_b$$$");
    all_methods[57] = C.class.getDeclaredMethod("m_b$Hot$$");
    all_methods[58] = C.class.getDeclaredMethod("m_b$$Startup$");
    all_methods[59] = C.class.getDeclaredMethod("m_b$Hot$Startup$");
    all_methods[60] = C.class.getDeclaredMethod("m_b$$$Poststartup");
    all_methods[61] = C.class.getDeclaredMethod("m_b$Hot$$Poststartup");
    all_methods[62] = C.class.getDeclaredMethod("m_b$$Startup$Poststartup");
    all_methods[63] = C.class.getDeclaredMethod("m_b$Hot$Startup$Poststartup");
    all_methods[64] = C.class.getDeclaredMethod("m_c$$$");
    all_methods[65] = C.class.getDeclaredMethod("m_c$Hot$$");
    all_methods[66] = C.class.getDeclaredMethod("m_c$$Startup$");
    all_methods[67] = C.class.getDeclaredMethod("m_c$Hot$Startup$");
    all_methods[68] = C.class.getDeclaredMethod("m_c$$$Poststartup");
    all_methods[69] = C.class.getDeclaredMethod("m_c$Hot$$Poststartup");
    all_methods[70] = C.class.getDeclaredMethod("m_c$$Startup$Poststartup");
    all_methods[71] = C.class.getDeclaredMethod("m_c$Hot$Startup$Poststartup");
    return all_methods;
  }
}

