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

public class Main {
  public static void main(String args[]) {
    simpleTest();
    hierarchyTest();
  }

  public static void simpleTest() {
    // Partial initialization of Bad; ignoring the error.
    Error badClinit = null;
    try {
      new Bad(11);
    } catch (Error e) {
      badClinit = e;
    }
    // Call foo() on the escaped instance of Bad.
    try {
      bad.foo();
    } catch (NoClassDefFoundError ncdfe) {
      // On RI, the NCDFE has no cause. On ART, the badClinit is the cause.
      if (ncdfe.getCause() == badClinit || ncdfe.getCause() == null) {
        System.out.println("Caught NoClassDefFoundError.");
      } else {
        ncdfe.printStackTrace();
      }
    }
    // Call bar() on the escaped instance of Bad.
    try {
      bad.bar();
    } catch (NoClassDefFoundError ncdfe) {
      // On RI, the NCDFE has no cause. On ART, the badClinit is the cause.
      if (ncdfe.getCause() == badClinit || ncdfe.getCause() == null) {
        System.out.println("Caught NoClassDefFoundError.");
      } else {
        ncdfe.printStackTrace();
      }
    }
  }

  public static void hierarchyTest() {
    // Partial initialization of BadSuper; ignoring the error. Fully initializes BadSub.
    Error badClinit = null;
    try {
      new BadSuper(0);
    } catch (Error e) {
      badClinit = e;
    }
    // Call BadSuper.foo() on the escaped instance of BadSuper.
    try {
      badSuper.foo();
    } catch (NoClassDefFoundError ncdfe) {
      // On RI, the NCDFE has no cause. On ART, the badClinit is the cause.
      if (ncdfe.getCause() == badClinit || ncdfe.getCause() == null) {
        System.out.println("Caught NoClassDefFoundError.");
      } else {
        ncdfe.printStackTrace();
      }
    }

    // Call BadSub.bar() on the escaped instance of BadSub.
    try {
      badSub.bar();
    } catch (NoClassDefFoundError ncdfe) {
      // On RI, the NCDFE has no cause. On ART, the badClinit is the cause.
      if (ncdfe.getCause() == badClinit || ncdfe.getCause() == null) {
        System.out.println("Caught NoClassDefFoundError.");
      } else {
        ncdfe.printStackTrace();
      }
    }

    // Test that we can even create instances of BadSub with erroneous superclass BadSuper.
    try {
      new BadSub(-1, -2).bar();
    } catch (NoClassDefFoundError ncdfe) {
      // On RI, the NCDFE has no cause. On ART, the badClinit is the cause.
      if (ncdfe.getCause() == badClinit || ncdfe.getCause() == null) {
        System.out.println("Caught NoClassDefFoundError.");
      } else {
        ncdfe.printStackTrace();
      }
    }

    // Test that we cannot create instances of BadSuper from BadSub.
    try {
      badSub.allocSuper(11111);  // Should throw.
      System.out.println("Allocated BadSuper!");
    } catch (NoClassDefFoundError ncdfe) {
      // On RI, the NCDFE has no cause. On ART, the badClinit is the cause.
      if (ncdfe.getCause() == badClinit || ncdfe.getCause() == null) {
        System.out.println("Caught NoClassDefFoundError.");
      } else {
        ncdfe.printStackTrace();
      }
    }
  }

  public static Bad bad;

  public static BadSuper badSuper;
  public static BadSub badSub;
}

class Bad {
  static {
    // Create an instance of Bad and let it escape in Main.bad.
    Main.bad = new Bad(33);
    staticValue = 42;
    if (true) { throw new Error("Bad <clinit>"); }
  }
  public void foo() {
    System.out.println("Bad.foo()");
    System.out.println("Bad.instanceValue = " + instanceValue);
    System.out.println("Bad.staticValue = " + staticValue);
  }
  public void bar() {
    System.out.println("Bad.bar()");
    System.out.println("Bad.staticValue [indirect] = " + Helper.$inline$getBadStaticValue());
  }
  public Bad(int iv) { instanceValue = iv; }
  public int instanceValue;
  public static int staticValue;

  public static class Helper {
    public static int $inline$getBadStaticValue() {
      return Bad.staticValue;
    }
  }
}

class BadSuper {
  static {
    Main.badSuper = new BadSuper(1);
    Main.badSub = new BadSub(11, 111);  // Fully initializes BadSub.
    BadSuper.superStaticValue = 42;
    BadSub.subStaticValue = 4242;
    if (true) { throw new Error("Bad <clinit>"); }
  }
  public void foo() {
    System.out.println("BadSuper.foo()");
    System.out.println("BadSuper.superInstanceValue = " + superInstanceValue);
    System.out.println("BadSuper.superStaticValue = " + superStaticValue);
  }
  public BadSuper(int superiv) { superInstanceValue = superiv; }
  public int superInstanceValue;
  public static int superStaticValue;
}

// Note: If we tried to initialize BadSub before BadSuper, it would end up erroneous
// because the superclass fails initialization. However, since we start initializing the
// BadSuper first, BadSub is initialized successfully while BadSuper is "initializing"
// and remains initialized after the BadSuper's class initializer throws.
class BadSub extends BadSuper {
  public void bar() {
    System.out.println("BadSub.bar()");
    System.out.println("BadSub.subInstanceValue = " + subInstanceValue);
    System.out.println("BadSub.subStaticValue = " + subStaticValue);
    System.out.println("BadSuper.superInstanceValue = " + superInstanceValue);
    System.out.println("BadSuper.superStaticValue = " + superStaticValue);
  }
  public BadSuper allocSuper(int superiv) {
    System.out.println("BadSub.allocSuper(.)");
    return new BadSuper(superiv);
  }
  public BadSub(int subiv, int superiv) {
    super(superiv);
    subInstanceValue = subiv;
  }
  public int subInstanceValue;
  public static int subStaticValue;
}
