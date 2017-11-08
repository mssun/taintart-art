/* Copyright (C) 2017 The Android Open Source Project
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This file implements interfaces from the file jvmti.h. This implementation
 * is licensed under the same terms as the file jvmti.h.  The
 * copyright and license information for the file jvmti.h follows.
 *
 * Copyright (c) 2003, 2011, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

#include <vector>

#include "ti_extension.h"

#include "art_jvmti.h"
#include "ti_allocator.h"
#include "ti_heap.h"

namespace openjdkjvmti {

struct CParamInfo {
  const char* name;
  jvmtiParamKind kind;
  jvmtiParamTypes base_type;
  jboolean null_ok;

  jvmtiParamInfo ToParamInfo(jvmtiEnv* env,
                             /*out*/std::vector<JvmtiUniquePtr<char[]>>* char_buffers,
                             /*out*/jvmtiError* err) const {
    JvmtiUniquePtr<char[]> param_name = CopyString(env, name, err);
    char* name_ptr = param_name.get();
    char_buffers->push_back(std::move(param_name));
    return jvmtiParamInfo { name_ptr, kind, base_type, null_ok }; // NOLINT [whitespace/braces] [4]
  }
};

jvmtiError ExtensionUtil::GetExtensionFunctions(jvmtiEnv* env,
                                                jint* extension_count_ptr,
                                                jvmtiExtensionFunctionInfo** extensions) {
  if (extension_count_ptr == nullptr || extensions == nullptr) {
    return ERR(NULL_POINTER);
  }

  std::vector<jvmtiExtensionFunctionInfo> ext_vector;

  // Holders for allocated values.
  std::vector<JvmtiUniquePtr<char[]>> char_buffers;
  std::vector<JvmtiUniquePtr<jvmtiParamInfo[]>> param_buffers;
  std::vector<JvmtiUniquePtr<jvmtiError[]>> error_buffers;

  auto add_extension = [&](jvmtiExtensionFunction func,
                           const char* id,
                           const char* short_description,
                           const std::vector<CParamInfo>& params,
                           const std::vector<jvmtiError>& errors) {
    jvmtiExtensionFunctionInfo func_info;
    jvmtiError error;

    func_info.func = func;

    JvmtiUniquePtr<char[]> id_ptr = CopyString(env, id, &error);
    if (id_ptr == nullptr) {
      return error;
    }
    func_info.id = id_ptr.get();
    char_buffers.push_back(std::move(id_ptr));

    JvmtiUniquePtr<char[]> descr = CopyString(env, short_description, &error);
    if (descr == nullptr) {
      return error;
    }
    func_info.short_description = descr.get();
    char_buffers.push_back(std::move(descr));

    func_info.param_count = params.size();
    if (!params.empty()) {
      JvmtiUniquePtr<jvmtiParamInfo[]> params_ptr =
          AllocJvmtiUniquePtr<jvmtiParamInfo[]>(env, params.size(), &error);
      if (params_ptr == nullptr) {
        return error;
      }
      func_info.params = params_ptr.get();
      param_buffers.push_back(std::move(params_ptr));

      for (jint i = 0; i != func_info.param_count; ++i) {
        func_info.params[i] = params[i].ToParamInfo(env, &char_buffers, &error);
        if (error != OK) {
          return error;
        }
      }
    } else {
      func_info.params = nullptr;
    }

    func_info.error_count = errors.size();
    if (!errors.empty()) {
      JvmtiUniquePtr<jvmtiError[]> errors_ptr =
          AllocJvmtiUniquePtr<jvmtiError[]>(env, errors.size(), &error);
      if (errors_ptr == nullptr) {
        return error;
      }
      func_info.errors = errors_ptr.get();
      error_buffers.push_back(std::move(errors_ptr));

      for (jint i = 0; i != func_info.error_count; ++i) {
        func_info.errors[i] = errors[i];
      }
    } else {
      func_info.errors = nullptr;
    }

    ext_vector.push_back(func_info);

    return ERR(NONE);
  };

  jvmtiError error;

  // Heap extensions.
  error = add_extension(
      reinterpret_cast<jvmtiExtensionFunction>(HeapExtensions::GetObjectHeapId),
      "com.android.art.heap.get_object_heap_id",
      "Retrieve the heap id of the the object tagged with the given argument. An "
          "arbitrary object is chosen if multiple objects exist with the same tag.",
      {                                                          // NOLINT [whitespace/braces] [4]
          { "tag", JVMTI_KIND_IN, JVMTI_TYPE_JLONG, false},
          { "heap_id", JVMTI_KIND_OUT, JVMTI_TYPE_JINT, false}
      },
      { JVMTI_ERROR_NOT_FOUND });
  if (error != ERR(NONE)) {
    return error;
  }

  error = add_extension(
      reinterpret_cast<jvmtiExtensionFunction>(HeapExtensions::GetHeapName),
      "com.android.art.heap.get_heap_name",
      "Retrieve the name of the heap with the given id.",
      {                                                          // NOLINT [whitespace/braces] [4]
          { "heap_id", JVMTI_KIND_IN, JVMTI_TYPE_JINT, false},
          { "heap_name", JVMTI_KIND_ALLOC_BUF, JVMTI_TYPE_CCHAR, false}
      },
      { JVMTI_ERROR_ILLEGAL_ARGUMENT });
  if (error != ERR(NONE)) {
    return error;
  }

  error = add_extension(
      reinterpret_cast<jvmtiExtensionFunction>(HeapExtensions::IterateThroughHeapExt),
      "com.android.art.heap.iterate_through_heap_ext",
      "Iterate through a heap. This is equivalent to the standard IterateThroughHeap function,"
      " except for additionally passing the heap id of the current object. The jvmtiHeapCallbacks"
      " structure is reused, with the callbacks field overloaded to a signature of "
      "jint (*)(jlong, jlong, jlong*, jint length, void*, jint).",
      {                                                          // NOLINT [whitespace/braces] [4]
          { "heap_filter", JVMTI_KIND_IN, JVMTI_TYPE_JINT, false},
          { "klass", JVMTI_KIND_IN, JVMTI_TYPE_JCLASS, true},
          { "callbacks", JVMTI_KIND_IN_PTR, JVMTI_TYPE_CVOID, false},
          { "user_data", JVMTI_KIND_IN_PTR, JVMTI_TYPE_CVOID, true}
      },
      {                                                          // NOLINT [whitespace/braces] [4]
          ERR(MUST_POSSESS_CAPABILITY),
          ERR(INVALID_CLASS),
          ERR(NULL_POINTER),
      });
  if (error != ERR(NONE)) {
    return error;
  }

  error = add_extension(
      reinterpret_cast<jvmtiExtensionFunction>(AllocUtil::GetGlobalJvmtiAllocationState),
      "com.android.art.alloc.get_global_jvmti_allocation_state",
      "Returns the total amount of memory currently allocated by all jvmtiEnvs through the"
      " 'Allocate' jvmti function. This does not include any memory that has been deallocated"
      " through the 'Deallocate' function. This number is approximate and might not correspond"
      " exactly to the sum of the sizes of all not freed allocations.",
      {                                                          // NOLINT [whitespace/braces] [4]
          { "currently_allocated", JVMTI_KIND_OUT, JVMTI_TYPE_JLONG, false},
      },
      { ERR(NULL_POINTER) });
  if (error != ERR(NONE)) {
    return error;
  }

  // Copy into output buffer.

  *extension_count_ptr = ext_vector.size();
  JvmtiUniquePtr<jvmtiExtensionFunctionInfo[]> out_data =
      AllocJvmtiUniquePtr<jvmtiExtensionFunctionInfo[]>(env, ext_vector.size(), &error);
  if (out_data == nullptr) {
    return error;
  }
  memcpy(out_data.get(),
          ext_vector.data(),
          ext_vector.size() * sizeof(jvmtiExtensionFunctionInfo));
  *extensions = out_data.release();

  // Release all the buffer holders, we're OK now.
  for (auto& holder : char_buffers) {
    holder.release();
  }
  for (auto& holder : param_buffers) {
    holder.release();
  }
  for (auto& holder : error_buffers) {
    holder.release();
  }

  return OK;
}


jvmtiError ExtensionUtil::GetExtensionEvents(jvmtiEnv* env ATTRIBUTE_UNUSED,
                                             jint* extension_count_ptr,
                                             jvmtiExtensionEventInfo** extensions) {
  // We don't have any extension events at the moment.
  *extension_count_ptr = 0;
  *extensions = nullptr;
  return OK;
}

jvmtiError ExtensionUtil::SetExtensionEventCallback(jvmtiEnv* env ATTRIBUTE_UNUSED,
                                                    jint extension_event_index ATTRIBUTE_UNUSED,
                                                    jvmtiExtensionEvent callback ATTRIBUTE_UNUSED) {
  // We do not have any extension events, so any call is illegal.
  return ERR(ILLEGAL_ARGUMENT);
}

}  // namespace openjdkjvmti
