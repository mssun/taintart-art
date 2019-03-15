/*
 * Copyright (C) 2014 The Android Open Source Project
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

#ifndef ART_RUNTIME_READ_BARRIER_OPTION_H_
#define ART_RUNTIME_READ_BARRIER_OPTION_H_
namespace art {

// Options for performing a read barrier or not.
//
// Besides disabled GC and GC's internal usage, there are a few cases where the read
// barrier is unnecessary and can be avoided to reduce code size and improve performance.
// In the following cases, the result of the operation or chain of operations shall be the
// same whether we go through the from-space or to-space:
//
// 1. We're reading a reference known to point to an un-reclaimable immune space object.
//    (For example boot image class and string references, read by compiled code from
//    .data.bimg.rel.ro . Similarly, such references constructed using position independent
//    code in the compiled boot image code do not need a read barrier.)
// 2. We're reading the reference for comparison involving a non-moving space reference.
//    (Whether the non-moving space reference is the one we're reading or the one we shall
//    compare it with, the result is the same with and without read barrier.)
// 3. We're reading the reference for comparison with null.
//    (Similar to 2 above, given that null is "non-moving".)
// 4. We're reading a reference to a holder from which we shall read
//      - constant primitive field, or
//      - constant reference field known to point to an un-reclaimable immune space object, or
//      - constant reference field for comparison involving a non-moving space reference, or
//      - constant reference field for comparison with null, or
//      - constant reference fields in a chain leading to one or more of the above purposes;
//        the entire chain needs to be read without read barrier.
//    The term "constant" refers to fields set to their final value between allocating
//    the holder and the next opportunity for the holder to be moved by the GC, i.e.
//    before the first suspend point after the allocation, or seen by another thread.
//    This includes several fields in the Class object, such as the primitive type or
//    component type but not the superclass.
//
// References read without a read barrier must not remain live at the next suspend point,
// with the exception of references to un-reclaimable immune space objects.
//
// For un-reclaimable immune space objects, we rely on graying dirty objects in the FlipCallback
// pause (we try to gray them just before flipping thread roots but the FlipCallback has to re-scan
// for newly dirtied objects) and clean objects conceptually become black at that point
// (marking them through is a no-op as all reference fields must also point to immune spaces),
// so mutator threads can never miss a read barrier as they never see white immune space object.
enum ReadBarrierOption {
  kWithReadBarrier,     // Perform a read barrier.
  kWithoutReadBarrier,  // Don't perform a read barrier.
};

}  // namespace art

#endif  // ART_RUNTIME_READ_BARRIER_OPTION_H_
