/*
 * Copyright (C) 2015 The Android Open Source Project
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

public class Main {

  public static void main(String[] args) {}
  public static int foo = 42;

  // Primitive array initialization is trivial for purposes of the ClinitCheck. It cannot
  // leak instances of erroneous classes or initialize subclasses of erroneous classes.
  public static int[] array1 = new int[] { 1, 2, 3 };
  public static int[] array2;
  static {
    int[] a = new int[4];
    a[0] = 42;
    array2 = a;
  }

  /// CHECK-START: void Main.inlinedMethod() builder (after)
  /// CHECK:                        ClinitCheck

  /// CHECK-START: void Main.inlinedMethod() inliner (after)
  /// CHECK:                        ClinitCheck
  /// CHECK-NOT:                    ClinitCheck
  /// CHECK-NOT:                    InvokeStaticOrDirect
  public void inlinedMethod() {
    SubSub.bar();
  }
}

class Sub extends Main {
  /// CHECK-START: void Sub.invokeSuperClass() builder (after)
  /// CHECK-NOT:                    ClinitCheck
  public void invokeSuperClass() {
    // No Class initialization check as Main.<clinit> is trivial. b/62478025
    int a = Main.foo;
  }

  /// CHECK-START: void Sub.invokeItself() builder (after)
  /// CHECK-NOT:                    ClinitCheck
  public void invokeItself() {
    // No Class initialization check as Sub.<clinit> and Main.<clinit> are trivial. b/62478025
    int a = foo;
  }

  /// CHECK-START: void Sub.invokeSubClass() builder (after)
  /// CHECK:                        ClinitCheck
  public void invokeSubClass() {
    int a = SubSub.foo;
  }

  public static int foo = 42;
}

class SubSub {
  public static void bar() {
    int a = Main.foo;
  }
  public static int foo = 42;
}

class NonTrivial {
  public static int staticFoo = 42;
  public int instanceFoo;

  static {
    System.out.println("NonTrivial.<clinit>");
  }

  /// CHECK-START: void NonTrivial.<init>() builder (after)
  /// CHECK-NOT:                    ClinitCheck

  /// CHECK-START: void NonTrivial.<init>() builder (after)
  /// CHECK:                        StaticFieldGet
  public NonTrivial() {
    // ClinitCheck is eliminated because this is a constructor and therefore the
    // corresponding new-instance in the caller must have performed the check.
    instanceFoo = staticFoo;
  }
}
