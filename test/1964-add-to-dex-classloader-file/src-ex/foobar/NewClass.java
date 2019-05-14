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

package foobar;
import art.Breakpoint;
import art.StackTrace;

public class NewClass {
  static void sayHi() throws Exception {
    System.out.println("Hello from NewClass sayHi function");
    // Doing this would be nice but it would make compiling the test more tricky. Just use
    // reflection and check the classloader is the same.
    // TestClass.sayBye();
    StackTrace.StackFrameData[] stack = StackTrace.GetStackTrace(Thread.currentThread());
    StackTrace.StackFrameData caller = null;
    System.out.println("Nearby stack:");
    for (StackTrace.StackFrameData sfd : stack) {
      String caller_name = sfd.method.getDeclaringClass().getName();
      if (caller_name.startsWith("art.") || caller_name.startsWith("foobar.")) {
        System.out.println("\t" + sfd.method + "(line: " +
                           Breakpoint.locationToLine(sfd.method, sfd.current_location) + ")");
        caller = sfd;
      } else {
        break;
      }
    }
    if (NewClass.class.getClassLoader() != caller.method.getDeclaringClass().getClassLoader()) {
      System.out.println("Different classloader for TestClass and my class.");
    }
  }
}