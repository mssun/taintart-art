/*
 * Copyright (C) 2016 The Android Open Source Project
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

import java.util.function.Consumer;

class Transform {
  public native void nativeSayHi(Consumer<Consumer<String>> r, Consumer<String> rep);
  /*
   * {
   *   sayHi(r, rep);
   * }
   */

  public void sayHi(Consumer<Consumer<String>> r, Consumer<String> reporter) {
    reporter.accept("Hello - Start method sayHi");
    r.accept(reporter);
    reporter.accept("Hello - End method sayHi");
  }
}
