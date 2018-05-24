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

import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Base64;
import java.util.LinkedList;

public class Main {
  /**
   * NB This test cannot be run on the RI.
   * TODO We should make this run on the RI.
   */

  private static final String LISTENER_LOCATION =
      System.getenv("DEX_LOCATION") + "/980-redefine-object-ex.jar";

  private static Method doEnableReporting;
  private static Method doDisableReporting;

  private static void DisableReporting() {
    if (doDisableReporting == null) {
      return;
    }
    try {
      doDisableReporting.invoke(null);
    } catch (Exception e) {
      throw new Error("Unable to disable reporting!");
    }
  }

  private static void EnableReporting() {
    if (doEnableReporting == null) {
      return;
    }
    try {
      doEnableReporting.invoke(null);
    } catch (Exception e) {
      throw new Error("Unable to enable reporting!");
    }
  }

  public static void main(String[] args) {
    doTest();
  }

  private static void ensureTestWatcherInitialized() {
    try {
      // Make sure the TestWatcher class can be found from the Object <init> function.
      addToBootClassLoader(LISTENER_LOCATION);
      // Load TestWatcher from the bootclassloader and make sure it is initialized.
      Class<?> testwatcher_class = Class.forName("art.test.TestWatcher", true, null);
      doEnableReporting = testwatcher_class.getDeclaredMethod("EnableReporting");
      doDisableReporting = testwatcher_class.getDeclaredMethod("DisableReporting");
    } catch (Exception e) {
      throw new Error("Exception while making testwatcher", e);
    }
  }

  // NB This function will cause 2 objects of type "Ljava/nio/HeapCharBuffer;" and
  // "Ljava/nio/HeapCharBuffer;" to be allocated each time it is called.
  private static void safePrintln(Object o) {
    DisableReporting();
    System.out.println("\t" + o);
    EnableReporting();
  }

  private static void throwFrom(int depth) throws Exception {
    if (depth <= 0) {
      throw new Exception("Throwing the exception");
    } else {
      throwFrom(depth - 1);
    }
  }

  public static void doTest() {
    safePrintln("Initializing and loading the TestWatcher class that will (eventually) be " +
                "notified of object allocations");
    // Make sure the TestWatcher class is initialized before we do anything else.
    ensureTestWatcherInitialized();
    safePrintln("Allocating an j.l.Object before redefining Object class");
    // Make sure these aren't shown.
    Object o = new Object();
    safePrintln("Allocating a Transform before redefining Object class");
    Transform t = new Transform();

    // Redefine the Object Class.
    safePrintln("Redefining the Object class to add a hook into the <init> method");
    addMemoryTrackingCall(Object.class, Thread.currentThread());

    safePrintln("Allocating an j.l.Object after redefining Object class");
    Object o2 = new Object();
    safePrintln("Allocating a Transform after redefining Object class");
    Transform t2 = new Transform();

    // This shouldn't cause the Object constructor to be run.
    safePrintln("Allocating an int[] after redefining Object class");
    int[] abc = new int[12];

    // Try adding stuff to an array list.
    safePrintln("Allocating an array list");
    ArrayList<Object> al = new ArrayList<>();
    safePrintln("Adding a bunch of stuff to the array list");
    al.add(new Object());
    al.add(new Object());
    al.add(o2);
    al.add(o);
    al.add(t);
    al.add(t2);
    al.add(new Transform());

    // Try adding stuff to a LinkedList
    safePrintln("Allocating a linked list");
    LinkedList<Object> ll = new LinkedList<>();
    safePrintln("Adding a bunch of stuff to the linked list");
    ll.add(new Object());
    ll.add(new Object());
    ll.add(o2);
    ll.add(o);
    ll.add(t);
    ll.add(t2);
    ll.add(new Transform());

    // Try making an exception.
    safePrintln("Throwing from down 4 stack frames");
    try {
      throwFrom(4);
    } catch (Exception e) {
      safePrintln("Exception caught.");
    }

    safePrintln("Finishing test!");
  }

  // This is from 929-search/search.cc
  private static native void addToBootClassLoader(String s);
  // This is from 980-redefine-object/redef_object.cc
  // It will add a call to Lart/test/TestWatcher;->NotifyConstructed()V in the Object <init>()V
  // function.
  private static native void addMemoryTrackingCall(Class c, Thread thr);
}
