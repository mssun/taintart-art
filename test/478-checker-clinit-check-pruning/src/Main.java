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

  /*
   * Ensure an inlined static invoke explicitly triggers the
   * initialization check of the called method's declaring class, and
   * that the corresponding load class instruction does not get
   * removed before register allocation & code generation.
   */

  /// CHECK-START: void Main.invokeStaticInlined() builder (after)
  /// CHECK-DAG:     <<LoadClass:l\d+>>    LoadClass gen_clinit_check:false
  /// CHECK-DAG:     <<ClinitCheck:l\d+>>  ClinitCheck [<<LoadClass>>]
  /// CHECK-DAG:                           InvokeStaticOrDirect [{{[ij]\d+}},<<ClinitCheck>>]

  /// CHECK-START: void Main.invokeStaticInlined() inliner (after)
  /// CHECK-DAG:     <<LoadClass:l\d+>>    LoadClass gen_clinit_check:false
  /// CHECK-DAG:     <<ClinitCheck:l\d+>>  ClinitCheck [<<LoadClass>>]

  /// CHECK-START: void Main.invokeStaticInlined() inliner (after)
  /// CHECK-NOT:                           InvokeStaticOrDirect

  // The following checks ensure the clinit check instruction added by
  // the builder is pruned by the PrepareForRegisterAllocation, while
  // the load class instruction is preserved.  As the control flow
  // graph is not dumped after (nor before) this step, we check the
  // CFG as it is before the next pass (liveness analysis) instead.

  /// CHECK-START: void Main.invokeStaticInlined() liveness (before)
  /// CHECK-DAG:                           LoadClass gen_clinit_check:true

  /// CHECK-START: void Main.invokeStaticInlined() liveness (before)
  /// CHECK-NOT:                           ClinitCheck
  /// CHECK-NOT:                           InvokeStaticOrDirect

  static void invokeStaticInlined() {
    ClassWithClinit1.$opt$inline$StaticMethod();
  }

  static class ClassWithClinit1 {
    static {
      System.out.println("Main$ClassWithClinit1's static initializer");
    }

    static void $opt$inline$StaticMethod() {
    }
  }

  /*
   * Ensure a non-inlined static invoke eventually has an implicit
   * initialization check of the called method's declaring class.
   */

  /// CHECK-START: void Main.invokeStaticNotInlined() builder (after)
  /// CHECK:         <<LoadClass:l\d+>>    LoadClass gen_clinit_check:false
  /// CHECK:         <<ClinitCheck:l\d+>>  ClinitCheck [<<LoadClass>>]
  /// CHECK:                               InvokeStaticOrDirect [{{[ij]\d+}},<<ClinitCheck>>]

  /// CHECK-START: void Main.invokeStaticNotInlined() inliner (after)
  /// CHECK:         <<LoadClass:l\d+>>    LoadClass gen_clinit_check:false
  /// CHECK:         <<ClinitCheck:l\d+>>  ClinitCheck [<<LoadClass>>]
  /// CHECK:                               InvokeStaticOrDirect [{{([ij]\d+,)?}}<<ClinitCheck>>]

  // The following checks ensure the clinit check and load class
  // instructions added by the builder are pruned by the
  // PrepareForRegisterAllocation.  As the control flow graph is not
  // dumped after (nor before) this step, we check the CFG as it is
  // before the next pass (liveness analysis) instead.

  /// CHECK-START: void Main.invokeStaticNotInlined() liveness (before)
  /// CHECK:                               InvokeStaticOrDirect clinit_check:implicit

  /// CHECK-START: void Main.invokeStaticNotInlined() liveness (before)
  /// CHECK-NOT:                           LoadClass
  /// CHECK-NOT:                           ClinitCheck

  static void invokeStaticNotInlined() {
    ClassWithClinit2.$noinline$staticMethod();
  }

  static class ClassWithClinit2 {
    static {
      System.out.println("Main$ClassWithClinit2's static initializer");
    }

    static boolean staticField = false;

    static void $noinline$staticMethod() {
    }
  }

  /*
   * Ensure an inlined call from a static method to a static method
   * of the same class does not require an explicit clinit check
   * (already initialized or initializing in the same thread).
   */

  /// CHECK-START: void Main$ClassWithClinit3Static.invokeStaticInlined() builder (after)
  /// CHECK-DAG:                           InvokeStaticOrDirect

  /// CHECK-START: void Main$ClassWithClinit3Static.invokeStaticInlined() builder (after)
  /// CHECK-NOT:                           LoadClass
  /// CHECK-NOT:                           ClinitCheck

  /// CHECK-START: void Main$ClassWithClinit3Static.invokeStaticInlined() inliner (after)
  /// CHECK-NOT:                           LoadClass
  /// CHECK-NOT:                           ClinitCheck
  /// CHECK-NOT:                           InvokeStaticOrDirect

  static class ClassWithClinit3Static {
    static void invokeStaticInlined() {
      // The invocation of invokeStaticInlined happens only after a clinit check
      // of ClassWithClinit3Static, meaning that the hereinbelow call to
      // $opt$inline$StaticMethod does not need another clinit check.
      $opt$inline$StaticMethod();
    }

    static {
      System.out.println("Main$ClassWithClinit3Static's static initializer");
    }

    static void $opt$inline$StaticMethod() {
    }
  }

  /*
   * Ensure an inlined call from an instance method to a static method
   * of the same class actually requires an explicit clinit check when
   * the class has a non-trivial initialization as we could be executing
   * the instance method on an escaped object of an erroneous class. b/62478025
   */

  /// CHECK-START: void Main$ClassWithClinit3Instance.invokeStaticInlined() builder (after)
  /// CHECK-DAG:                           LoadClass
  /// CHECK-DAG:                           ClinitCheck
  /// CHECK-DAG:                           InvokeStaticOrDirect

  /// CHECK-START: void Main$ClassWithClinit3Instance.invokeStaticInlined() inliner (after)
  /// CHECK-DAG:                           LoadClass
  /// CHECK-DAG:                           ClinitCheck

  /// CHECK-START: void Main$ClassWithClinit3Instance.invokeStaticInlined() inliner (after)
  /// CHECK-NOT:                           InvokeStaticOrDirect

  static class ClassWithClinit3Instance {
    void invokeStaticInlined() {
      // ClinitCheck required.
      $opt$inline$StaticMethod();
    }

    static {
      System.out.println("Main$ClassWithClinit3Instance's static initializer");
    }

    static void $opt$inline$StaticMethod() {
    }
  }

  /*
   * Ensure a non-inlined call from a static method to a static method
   * of the same class does not require an explicit clinit check
   * (already initialized or initializing in the same thread).
   */

  /// CHECK-START: void Main$ClassWithClinit4Static.invokeStaticNotInlined() builder (after)
  /// CHECK-DAG:                           InvokeStaticOrDirect

  /// CHECK-START: void Main$ClassWithClinit4Static.invokeStaticNotInlined() builder (after)
  /// CHECK-NOT:                           LoadClass
  /// CHECK-NOT:                           ClinitCheck

  /// CHECK-START: void Main$ClassWithClinit4Static.invokeStaticNotInlined() inliner (after)
  /// CHECK-DAG:                           InvokeStaticOrDirect

  /// CHECK-START: void Main$ClassWithClinit4Static.invokeStaticNotInlined() inliner (after)
  /// CHECK-NOT:                           LoadClass
  /// CHECK-NOT:                           ClinitCheck

  static class ClassWithClinit4Static {
    static void invokeStaticNotInlined() {
      // The invocation of invokeStaticNotInlined triggers the
      // initialization of ClassWithClinit4Static, meaning that the
      // call to staticMethod below does not need a clinit
      // check.
      $noinline$staticMethod();
    }

    static {
      System.out.println("Main$ClassWithClinit4Static's static initializer");
    }

    static void $noinline$staticMethod() {
    }
  }

  /*
   * Ensure a non-inlined call from an instance method to a static method
   * of the same class actually requires an explicit clinit check when
   * the class has a non-trivial initialization as we could be executing
   * the instance method on an escaped object of an erroneous class. b/62478025
   */

  /// CHECK-START: void Main$ClassWithClinit4Instance.invokeStaticNotInlined() builder (after)
  /// CHECK-DAG:                           LoadClass
  /// CHECK-DAG:                           ClinitCheck
  /// CHECK-DAG:                           InvokeStaticOrDirect

  /// CHECK-START: void Main$ClassWithClinit4Instance.invokeStaticNotInlined() inliner (after)
  /// CHECK-DAG:                           LoadClass
  /// CHECK-DAG:                           ClinitCheck
  /// CHECK-DAG:                           InvokeStaticOrDirect

  static class ClassWithClinit4Instance {
    void invokeStaticNotInlined() {
      // ClinitCheck required.
      $noinline$staticMethod();
    }

    static {
      System.out.println("Main$ClassWithClinit4Instance's static initializer");
    }

    static void $noinline$staticMethod() {
    }
  }

  /*
   * We used to remove clinit check for calls to static methods in a superclass. However, this
   * is not a valid optimization when instances of erroneous classes can escape, therefore
   * we avoid this optimization for classes with non-trivial initialization. b/62478025
   */

  /// CHECK-START: void Main$SubClassOfClassWithClinit5.invokeStaticInlined() builder (after)
  /// CHECK-DAG:                           LoadClass
  /// CHECK-DAG:                           ClinitCheck
  /// CHECK-DAG:                           InvokeStaticOrDirect

  /// CHECK-START: void Main$SubClassOfClassWithClinit5.invokeStaticInlined() inliner (after)
  /// CHECK-DAG:                           LoadClass
  /// CHECK-DAG:                           ClinitCheck
  /// CHECK-NOT:                           InvokeStaticOrDirect

  static class ClassWithClinit5 {
    static void $opt$inline$StaticMethod() {
    }

    static {
      System.out.println("Main$ClassWithClinit5's static initializer");
    }
  }

  static class SubClassOfClassWithClinit5 extends ClassWithClinit5 {
    static void invokeStaticInlined() {
      ClassWithClinit5.$opt$inline$StaticMethod();
    }
  }

  /*
   * Ensure an inlined call to a static method whose declaring class is a super class
   * of the caller's class does not require an explicit clinit check if the declaring
   * class has a trivial initialization. b/62478025
   */

  /// CHECK-START: void Main$SubClassOfClassWithoutClinit5.invokeStaticInlined() builder (after)
  /// CHECK-DAG:                           InvokeStaticOrDirect

  /// CHECK-START: void Main$SubClassOfClassWithoutClinit5.invokeStaticInlined() builder (after)
  /// CHECK-NOT:                           LoadClass
  /// CHECK-NOT:                           ClinitCheck

  /// CHECK-START: void Main$SubClassOfClassWithoutClinit5.invokeStaticInlined() inliner (after)
  /// CHECK-NOT:                           LoadClass
  /// CHECK-NOT:                           ClinitCheck
  /// CHECK-NOT:                           InvokeStaticOrDirect

  static class ClassWithoutClinit5 {  // Mimicks ClassWithClinit5 but without the <clinit>.
    static void $opt$inline$StaticMethod() {
    }
  }

  static class SubClassOfClassWithoutClinit5 extends ClassWithoutClinit5 {
    static {
      System.out.println("Main$SubClassOfClassWithoutClinit5's static initializer");
    }

    static void invokeStaticInlined() {
      ClassWithoutClinit5.$opt$inline$StaticMethod();
    }
  }

  /*
   * We used to remove clinit check for calls to static methods in a superclass. However, this
   * is not a valid optimization when instances of erroneous classes can escape, therefore
   * we avoid this optimization for classes with non-trivial initialization. b/62478025
   */

  /// CHECK-START: void Main$SubClassOfClassWithClinit6.invokeStaticNotInlined() builder (after)
  /// CHECK-DAG:                           InvokeStaticOrDirect

  /// CHECK-START: void Main$SubClassOfClassWithClinit6.invokeStaticNotInlined() builder (after)
  /// CHECK-DAG:                           LoadClass
  /// CHECK-DAG:                           ClinitCheck

  /// CHECK-START: void Main$SubClassOfClassWithClinit6.invokeStaticNotInlined() inliner (after)
  /// CHECK-DAG:                           LoadClass
  /// CHECK-DAG:                           ClinitCheck
  /// CHECK-DAG:                           InvokeStaticOrDirect

  static class ClassWithClinit6 {
    static void $noinline$staticMethod() {
    }

    static {
      System.out.println("Main$ClassWithClinit6's static initializer");
    }
  }

  static class SubClassOfClassWithClinit6 extends ClassWithClinit6 {
    static void invokeStaticNotInlined() {
      ClassWithClinit6.$noinline$staticMethod();
    }
  }

  /*
   * Ensure a non-inlined call to a static method whose declaring class is a super class
   * of the caller's class does not require an explicit clinit check if the declaring
   * class has a trivial initialization. b/62478025
   */

  /// CHECK-START: void Main$SubClassOfClassWithoutClinit6.invokeStaticNotInlined() builder (after)
  /// CHECK-DAG:                           InvokeStaticOrDirect

  /// CHECK-START: void Main$SubClassOfClassWithoutClinit6.invokeStaticNotInlined() builder (after)
  /// CHECK-NOT:                           LoadClass
  /// CHECK-NOT:                           ClinitCheck

  /// CHECK-START: void Main$SubClassOfClassWithoutClinit6.invokeStaticNotInlined() inliner (after)
  /// CHECK-DAG:                           InvokeStaticOrDirect

  /// CHECK-START: void Main$SubClassOfClassWithoutClinit6.invokeStaticNotInlined() inliner (after)
  /// CHECK-NOT:                           LoadClass
  /// CHECK-NOT:                           ClinitCheck

  static class ClassWithoutClinit6 {  // Mimicks ClassWithClinit6 but without the <clinit>.
    static void $noinline$staticMethod() {
    }
  }

  static class SubClassOfClassWithoutClinit6 extends ClassWithoutClinit6 {
    static {
      System.out.println("Main$SubClassOfClassWithoutClinit6's static initializer");
    }

    static void invokeStaticNotInlined() {
      ClassWithoutClinit6.$noinline$staticMethod();
    }
  }

  /*
   * Verify that if we have a static call immediately after the load class
   * we don't do generate a clinit check.
   */

  /// CHECK-START: void Main.noClinitBecauseOfInvokeStatic() liveness (before)
  /// CHECK-DAG:     <<IntConstant:i\d+>>  IntConstant 0
  /// CHECK-DAG:     <<LoadClass:l\d+>>    LoadClass gen_clinit_check:false
  /// CHECK-DAG:                           InvokeStaticOrDirect clinit_check:implicit
  /// CHECK-DAG:                           StaticFieldSet [<<LoadClass>>,<<IntConstant>>]

  /// CHECK-START: void Main.noClinitBecauseOfInvokeStatic() liveness (before)
  /// CHECK-NOT:                           ClinitCheck

  static void noClinitBecauseOfInvokeStatic() {
    ClassWithClinit2.$noinline$staticMethod();
    ClassWithClinit2.staticField = false;
  }

  /*
   * Verify that if the static call is after a field access, the load class
   * will generate a clinit check.
   */

  /// CHECK-START: void Main.clinitBecauseOfFieldAccess() liveness (before)
  /// CHECK-DAG:     <<IntConstant:i\d+>>  IntConstant 0
  /// CHECK-DAG:     <<LoadClass:l\d+>>    LoadClass gen_clinit_check:true
  /// CHECK-DAG:                           StaticFieldSet [<<LoadClass>>,<<IntConstant>>]
  /// CHECK-DAG:                           InvokeStaticOrDirect clinit_check:none

  /// CHECK-START: void Main.clinitBecauseOfFieldAccess() liveness (before)
  /// CHECK-NOT:                           ClinitCheck
  static void clinitBecauseOfFieldAccess() {
    ClassWithClinit2.staticField = false;
    ClassWithClinit2.$noinline$staticMethod();
  }

  /*
   * Verify that LoadClass from const-class is not merged with
   * later invoke-static (or it's ClinitCheck).
   */

  /// CHECK-START: void Main.constClassAndInvokeStatic(java.lang.Iterable) liveness (before)
  /// CHECK:                               LoadClass gen_clinit_check:false
  /// CHECK:                               InvokeStaticOrDirect clinit_check:implicit

  /// CHECK-START: void Main.constClassAndInvokeStatic(java.lang.Iterable) liveness (before)
  /// CHECK-NOT:                           ClinitCheck

  static void constClassAndInvokeStatic(Iterable<?> it) {
    $opt$inline$ignoreClass(ClassWithClinit7.class);
    ClassWithClinit7.$noinline$someStaticMethod(it);
  }

  static void $opt$inline$ignoreClass(Class<?> c) {
  }

  static class ClassWithClinit7 {
    static {
      System.out.println("Main$ClassWithClinit7's static initializer");
    }

    static void $noinline$someStaticMethod(Iterable<?> it) {
      it.iterator();
    }
  }

  /*
   * Verify that LoadClass from sget is not merged with later invoke-static.
   */

  /// CHECK-START: void Main.sgetAndInvokeStatic(java.lang.Iterable) liveness (before)
  /// CHECK:                               LoadClass gen_clinit_check:true
  /// CHECK:                               InvokeStaticOrDirect clinit_check:none

  /// CHECK-START: void Main.sgetAndInvokeStatic(java.lang.Iterable) liveness (before)
  /// CHECK-NOT:                           ClinitCheck

  static void sgetAndInvokeStatic(Iterable<?> it) {
    $opt$inline$ignoreInt(ClassWithClinit8.value);
    ClassWithClinit8.$noinline$someStaticMethod(it);
  }

  static void $opt$inline$ignoreInt(int i) {
  }

  static class ClassWithClinit8 {
    public static int value = 0;
    static {
      System.out.println("Main$ClassWithClinit8's static initializer");
    }

    static void $noinline$someStaticMethod(Iterable<?> it) {
      it.iterator();
    }
  }

  /*
   * Verify that LoadClass from const-class, ClinitCheck from sget and
   * InvokeStaticOrDirect from invoke-static are not merged.
   */

  /// CHECK-START: void Main.constClassSgetAndInvokeStatic(java.lang.Iterable) liveness (before)
  /// CHECK:                               LoadClass gen_clinit_check:false
  /// CHECK:                               ClinitCheck
  /// CHECK:                               InvokeStaticOrDirect clinit_check:none

  static void constClassSgetAndInvokeStatic(Iterable<?> it) {
    $opt$inline$ignoreClass(ClassWithClinit9.class);
    $opt$inline$ignoreInt(ClassWithClinit9.value);
    ClassWithClinit9.$noinline$someStaticMethod(it);
  }

  static class ClassWithClinit9 {
    public static int value = 0;
    static {
      System.out.println("Main$ClassWithClinit9's static initializer");
    }

    static void $noinline$someStaticMethod(Iterable<?> it) {
      it.iterator();
    }
  }

  /*
   * Verify that LoadClass from a fully-inlined invoke-static is not merged
   * with InvokeStaticOrDirect from a later invoke-static to the same method.
   */

  /// CHECK-START: void Main.inlinedInvokeStaticViaNonStatic(java.lang.Iterable) liveness (before)
  /// CHECK:                               LoadClass gen_clinit_check:true
  /// CHECK:                               InvokeStaticOrDirect clinit_check:none

  /// CHECK-START: void Main.inlinedInvokeStaticViaNonStatic(java.lang.Iterable) liveness (before)
  /// CHECK-NOT:                           ClinitCheck

  static void inlinedInvokeStaticViaNonStatic(Iterable<?> it) {
    if (it != null) {
      inlinedInvokeStaticViaNonStaticHelper(null);
      inlinedInvokeStaticViaNonStaticHelper(it);
    }
  }

  static void inlinedInvokeStaticViaNonStaticHelper(Iterable<?> it) {
    ClassWithClinit10.inlinedForNull(it);
  }

  static class ClassWithClinit10 {
    public static int value = 0;
    static {
      System.out.println("Main$ClassWithClinit10's static initializer");
    }

    static void inlinedForNull(Iterable<?> it) {
      if (it != null) {
        it.iterator();
        // We're not inlining methods that always throw.
        throw new Error("");
      }
    }
  }

  /*
   * Check that the LoadClass from an invoke-static C.foo() doesn't get merged with
   * an invoke-static inside C.foo(). This would mess up the stack walk in the
   * resolution trampoline where we would have to load C (if C isn't loaded yet)
   * which is not permitted there.
   *
   * Note: In case of failure, we would get an failed assertion during compilation,
   * so we wouldn't really get to the checker tests below.
   */

  /// CHECK-START: void Main.inlinedInvokeStaticViaStatic(java.lang.Iterable) liveness (before)
  /// CHECK:                               LoadClass gen_clinit_check:true
  /// CHECK:                               InvokeStaticOrDirect clinit_check:none

  /// CHECK-START: void Main.inlinedInvokeStaticViaStatic(java.lang.Iterable) liveness (before)
  /// CHECK-NOT:                           ClinitCheck

  static void inlinedInvokeStaticViaStatic(Iterable<?> it) {
    if (it != null) {
      ClassWithClinit11.callInlinedForNull(it);
    }
  }

  static class ClassWithClinit11 {
    public static int value = 0;
    static {
      System.out.println("Main$ClassWithClinit11's static initializer");
    }

    static void callInlinedForNull(Iterable<?> it) {
      inlinedForNull(it);
    }

    static void inlinedForNull(Iterable<?> it) {
      it.iterator();
      if (it != null) {
        // We're not inlining methods that always throw.
        throw new Error("");
      }
    }
  }

  /*
   * A test similar to inlinedInvokeStaticViaStatic() but doing the indirect invoke
   * twice with the first one to be fully inlined.
   */

  /// CHECK-START: void Main.inlinedInvokeStaticViaStaticTwice(java.lang.Iterable) liveness (before)
  /// CHECK:                               LoadClass gen_clinit_check:true
  /// CHECK:                               InvokeStaticOrDirect clinit_check:none

  /// CHECK-START: void Main.inlinedInvokeStaticViaStaticTwice(java.lang.Iterable) liveness (before)
  /// CHECK-NOT:                           ClinitCheck

  static void inlinedInvokeStaticViaStaticTwice(Iterable<?> it) {
    if (it != null) {
      ClassWithClinit12.callInlinedForNull(null);
      ClassWithClinit12.callInlinedForNull(it);
    }
  }

  static class ClassWithClinit12 {
    public static int value = 0;
    static {
      System.out.println("Main$ClassWithClinit12's static initializer");
    }

    static void callInlinedForNull(Iterable<?> it) {
      inlinedForNull(it);
    }

    static void inlinedForNull(Iterable<?> it) {
      if (it != null) {
        // We're not inlining methods that always throw.
        throw new Error("");
      }
    }
  }

  static class ClassWithClinit13 {
    static {
      System.out.println("Main$ClassWithClinit13's static initializer");
    }

    public static void $inline$forwardToGetIterator(Iterable<?> it) {
      $noinline$getIterator(it);
    }

    public static void $noinline$getIterator(Iterable<?> it) {
      it.iterator();
    }
  }

  // TODO: Write checker statements.
  static Object $noinline$testInliningAndNewInstance(Iterable<?> it) {
    ClassWithClinit13.$inline$forwardToGetIterator(it);
    return new ClassWithClinit13();
  }

  // TODO: Add a test for the case of a static method whose declaring
  // class type index is not available (i.e. when `storage_index`
  // equals `dex::kDexNoIndex` in
  // art::HGraphBuilder::BuildInvoke).

  public static void main(String[] args) {
    invokeStaticInlined();
    invokeStaticNotInlined();
    ClassWithClinit3Static.invokeStaticInlined();
    new ClassWithClinit3Instance().invokeStaticInlined();
    ClassWithClinit4Static.invokeStaticNotInlined();
    new ClassWithClinit4Instance().invokeStaticNotInlined();
    SubClassOfClassWithClinit5.invokeStaticInlined();
    SubClassOfClassWithoutClinit5.invokeStaticInlined();
    SubClassOfClassWithClinit6.invokeStaticNotInlined();
    SubClassOfClassWithoutClinit6.invokeStaticNotInlined();
    Iterable it = new Iterable() { public java.util.Iterator iterator() { return null; } };
    constClassAndInvokeStatic(it);
    sgetAndInvokeStatic(it);
    constClassSgetAndInvokeStatic(it);
    try {
      inlinedInvokeStaticViaNonStatic(it);
    } catch (Error e) {
      // Expected
    }
    try {
      inlinedInvokeStaticViaStatic(it);
    } catch (Error e) {
      // Expected
    }
    try{
      inlinedInvokeStaticViaStaticTwice(it);
    } catch (Error e) {
      // Expected
    }
    $noinline$testInliningAndNewInstance(it);
  }
}
