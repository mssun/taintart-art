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

package android.os;

import java.lang.String;

/** Fake android.os.Binder class that just holds a descriptor.
 *
 * Note that having this class will cause Proguard to issue warnings when
 * building ahat-test-dump with 'mm' or 'mma':
 *
 * Warning: Library class android.net.wifi.rtt.IWifiRttManager$Stub extends
 * program class android.os.Binder
 *
 * This is because when building for the device, proguard will include the
 * framework jars, which contain Stub classes that extend android.os.Binder,
 * which is defined again here.
 *
 * Since we don't actually run this code on the device, these warnings can
 * be ignored.
 */
public class Binder implements IBinder {
  public Binder() {
    mDescriptor = null;
  }

  public Binder(String descriptor) {
    mDescriptor = descriptor;
  }

  private String mDescriptor;
}
