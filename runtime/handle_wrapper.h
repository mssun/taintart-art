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

#ifndef ART_RUNTIME_HANDLE_WRAPPER_H_
#define ART_RUNTIME_HANDLE_WRAPPER_H_

#include "handle.h"
#include "obj_ptr.h"

namespace art {

// A wrapper which wraps around Object** and restores the pointer in the destructor.
// TODO: Delete
template<class T>
class HandleWrapper : public MutableHandle<T> {
 public:
  HandleWrapper(T** obj, const MutableHandle<T>& handle)
     : MutableHandle<T>(handle), obj_(obj) {
  }

  HandleWrapper(const HandleWrapper&) = default;

  ~HandleWrapper() {
    *obj_ = MutableHandle<T>::Get();
  }

 private:
  T** const obj_;
};


// A wrapper which wraps around ObjPtr<Object>* and restores the pointer in the destructor.
// TODO: Add more functionality.
template<class T>
class HandleWrapperObjPtr : public MutableHandle<T> {
 public:
  HandleWrapperObjPtr(ObjPtr<T>* obj, const MutableHandle<T>& handle)
      : MutableHandle<T>(handle), obj_(obj) {}

  HandleWrapperObjPtr(const HandleWrapperObjPtr&) = default;

  ~HandleWrapperObjPtr() {
    *obj_ = MutableHandle<T>::Get();
  }

 private:
  ObjPtr<T>* const obj_;
};

}  // namespace art

#endif  // ART_RUNTIME_HANDLE_WRAPPER_H_
