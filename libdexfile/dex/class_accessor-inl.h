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

#ifndef ART_LIBDEXFILE_DEX_CLASS_ACCESSOR_INL_H_
#define ART_LIBDEXFILE_DEX_CLASS_ACCESSOR_INL_H_

#include "class_accessor.h"

#include "base/leb128.h"
#include "class_iterator.h"
#include "code_item_accessors-inl.h"

namespace art {

inline ClassAccessor::ClassAccessor(const ClassIteratorData& data)
    : ClassAccessor(data.dex_file_, data.dex_file_.GetClassDef(data.class_def_idx_)) {}

inline ClassAccessor::ClassAccessor(const DexFile& dex_file, const DexFile::ClassDef& class_def)
    : dex_file_(dex_file),
      descriptor_index_(class_def.class_idx_),
      ptr_pos_(dex_file.GetClassData(class_def)),
      num_static_fields_(ptr_pos_ != nullptr ? DecodeUnsignedLeb128(&ptr_pos_) : 0u),
      num_instance_fields_(ptr_pos_ != nullptr ? DecodeUnsignedLeb128(&ptr_pos_) : 0u),
      num_direct_methods_(ptr_pos_ != nullptr ? DecodeUnsignedLeb128(&ptr_pos_) : 0u),
      num_virtual_methods_(ptr_pos_ != nullptr ? DecodeUnsignedLeb128(&ptr_pos_) : 0u) {}

inline const uint8_t* ClassAccessor::Method::Read(const uint8_t* ptr) {
  method_idx_ += DecodeUnsignedLeb128(&ptr);
  access_flags_ = DecodeUnsignedLeb128(&ptr);
  code_off_ = DecodeUnsignedLeb128(&ptr);
  return ptr;
}

inline const uint8_t* ClassAccessor::Field::Read(const uint8_t* ptr) {
  field_idx_ += DecodeUnsignedLeb128(&ptr);
  access_flags_ = DecodeUnsignedLeb128(&ptr);
  return ptr;
}

template <typename StaticFieldVisitor,
          typename InstanceFieldVisitor,
          typename DirectMethodVisitor,
          typename VirtualMethodVisitor>
inline void ClassAccessor::VisitMethodsAndFields(
    const StaticFieldVisitor& static_field_visitor,
    const InstanceFieldVisitor& instance_field_visitor,
    const DirectMethodVisitor& direct_method_visitor,
    const VirtualMethodVisitor& virtual_method_visitor) const {
  const uint8_t* ptr = ptr_pos_;
  {
    Field data;
    for (size_t i = 0; i < num_static_fields_; ++i) {
      ptr = data.Read(ptr);
      static_field_visitor(data);
    }
  }
  {
    Field data;
    for (size_t i = 0; i < num_instance_fields_; ++i) {
      ptr = data.Read(ptr);
      instance_field_visitor(data);
    }
  }
  {
    Method data(dex_file_);
    for (size_t i = 0; i < num_direct_methods_; ++i) {
      ptr = data.Read(ptr);
      direct_method_visitor(data);
    }
  }
  {
    Method data(dex_file_);
    for (size_t i = 0; i < num_virtual_methods_; ++i) {
      ptr = data.Read(ptr);
      virtual_method_visitor(data);
    }
  }
}

template <typename DirectMethodVisitor,
          typename VirtualMethodVisitor>
inline void ClassAccessor::VisitMethods(const DirectMethodVisitor& direct_method_visitor,
                                        const VirtualMethodVisitor& virtual_method_visitor) const {
  VisitMethodsAndFields(VoidFunctor(),
                        VoidFunctor(),
                        direct_method_visitor,
                        virtual_method_visitor);
}

// Visit direct and virtual methods.
template <typename MethodVisitor>
inline void ClassAccessor::VisitMethods(const MethodVisitor& method_visitor) const {
  VisitMethods(method_visitor, method_visitor);
}

inline const DexFile::CodeItem* ClassAccessor::GetCodeItem(const Method& method) const {
  return dex_file_.GetCodeItem(method.GetCodeItemOffset());
}

inline CodeItemInstructionAccessor ClassAccessor::Method::GetInstructions() const {
  return CodeItemInstructionAccessor(dex_file_, dex_file_.GetCodeItem(GetCodeItemOffset()));
}

}  // namespace art

#endif  // ART_LIBDEXFILE_DEX_CLASS_ACCESSOR_INL_H_
