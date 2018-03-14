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

public class OverrideClass extends ParentClass {

  @Override public int methodPublicWhitelist() { return -411; }
  @Override int methodPackageWhitelist() { return -412; }
  @Override protected int methodProtectedWhitelist() { return -413; }

  @Override public int methodPublicLightGreylist() { return -421; }
  @Override int methodPackageLightGreylist() { return -422; }
  @Override protected int methodProtectedLightGreylist() { return -423; }

  @Override public int methodPublicDarkGreylist() { return -431; }
  @Override int methodPackageDarkGreylist() { return -432; }
  @Override protected int methodProtectedDarkGreylist() { return -433; }

  @Override public int methodPublicBlacklist() { return -441; }
  @Override int methodPackageBlacklist() { return -442; }
  @Override protected int methodProtectedBlacklist() { return -443; }

}
