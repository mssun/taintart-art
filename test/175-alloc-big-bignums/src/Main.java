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

import java.math.BigInteger;

// This is motivated by the assumption that BigInteger allocates malloc memory
// underneath. That's true (in 2018) on Android.

public class Main {
  public static void main(String[] args) throws Exception {
    final int nIters = 20_000;  // Presumed < 1_000_000.
    final BigInteger big2_20 = BigInteger.valueOf(1024*1024); // 2^20
    BigInteger huge = BigInteger.valueOf(1).shiftLeft(4_000_000);  // ~0.5MB
    for (int i = 0; i < nIters; ++i) {  // 10 GB total
      huge = huge.add(BigInteger.ONE);
    }
    if (huge.bitLength() != 4_000_001) {
      System.out.println("Wrong answer length: " + huge.bitLength());
    } else if (huge.mod(big2_20).compareTo(BigInteger.valueOf(nIters)) != 0) {
      System.out.println("Wrong answer: ..." + huge.mod(big2_20));
    } else {
      System.out.println("Test complete");
    }
  }
}
