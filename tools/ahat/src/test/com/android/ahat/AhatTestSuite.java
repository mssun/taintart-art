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

package com.android.ahat;

import org.junit.runner.JUnitCore;
import org.junit.runner.RunWith;
import org.junit.runners.Suite;

@RunWith(Suite.class)
@Suite.SuiteClasses({
  DiffFieldsTest.class,
  DiffTest.class,
  DominatorsTest.class,
  HtmlEscaperTest.class,
  InstanceTest.class,
  NativeAllocationTest.class,
  ObjectHandlerTest.class,
  ObjectsHandlerTest.class,
  OverviewHandlerTest.class,
  PerformanceTest.class,
  ProguardMapTest.class,
  RootedHandlerTest.class,
  QueryTest.class,
  RiTest.class,
  SiteHandlerTest.class,
  SiteTest.class
})

public class AhatTestSuite {
  public static void main(String[] args) {
    if (args.length == 0) {
      args = new String[]{"com.android.ahat.AhatTestSuite"};
    }
    JUnitCore.main(args);
  }
}
