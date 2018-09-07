/*
 * Copyright (C) 2011 The Android Open Source Project
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

// This test depends on the exact format of the DEX file. Since dx is deprecated,
// the classes.dex file is packaged as a test input. It was created with:
//
// $ javac -g -Xlint:-options -source 1.7 -target 1.7 -d classes src/Main.java
// $ dx --debug --dex --output=classes.dex classes

public class Main {
  public Main() {
  }

  int $noinline$f() throws Exception {
    $noinline$g(1);
    $noinline$g(2);
    return 0;
  }

  void $noinline$g(int num_calls) {
    if (num_calls == 1) {
      System.out.println("1st call");
    } else if (num_calls == 2) {
      System.out.println("2nd call");
    }
    System.out.println(shlemiel());
  }

  String shlemiel() {
    String s0 = new String("0");
    String s1 = new String("1");
    String s2 = new String("2");
    String s3 = new String("3");
    String s4 = new String("4");
    String s5 = new String("5");
    String s6 = new String("6");
    String s7 = new String("7");
    String s8 = new String("8");
    String s9 = new String("9");
    String s10 = new String("10");
    String s11 = new String("11");
    String s12 = new String("12");
    String s13 = new String("13");
    String s14 = new String("14");
    String s15 = new String("15");
    String s16 = new String("16");
    String s17 = new String("17");
    String s18 = new String("18");
    String s19 = new String("19");
    String s20 = new String("20");
    String s = new String();
    s += s0;
    s += s1;
    s += s2;
    s += s3;
    s += s4;
    s += s5;
    s += s6;
    s += s7;
    s += s8;
    s += s9;
    s += s10;
    s += s11;
    s += s12;
    s += s13;
    s += s14;
    s += s15;
    s += s16;
    s += s17;
    s += s18;
    s += s19;
    s += s20;

    s += s6;
    s += s5;
    s += s2;
    s += s3;

    s10 = s + s10;
    s10 += s20;

    s20 += s10;
    s = s17 + s20;

    s4 = s18 = s19;
    s += s4;
    s += s18;
    // Add a branch to workaround ART's large methods without branches heuristic.
    if (testStackWalk(0) != 0) {
      return s;
    }
    return s18;
  }

  native int testStackWalk(int x);

  public static void main(String[] args) throws Exception {
    System.loadLibrary(args[0]);
    Main st = new Main();
    st.$noinline$f();
  }
}
