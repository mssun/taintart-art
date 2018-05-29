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
  index_ += DecodeUnsignedLeb128(&ptr);
  access_flags_ = DecodeUnsignedLeb128(&ptr);
  code_off_ = DecodeUnsignedLeb128(&ptr);
  return ptr;
}

inline const uint8_t* ClassAccessor::Field::Read(const uint8_t* ptr) {
  index_ += DecodeUnsignedLeb128(&ptr);
  access_flags_ = DecodeUnsignedLeb128(&ptr);
  return ptr;
}

template <typename DataType, typename Visitor>
inline const uint8_t* ClassAccessor::VisitMembers(size_t count,
                                                  const Visitor& visitor,
                                                  const uint8_t* ptr,
                                                  DataType* data) const {
  DCHECK(data != nullptr);
  for ( ; count != 0; --count) {
    ptr = data->Read(ptr);
    visitor(*data);
  }
  return ptr;
}

template <typename StaticFieldVisitor,
          typename InstanceFieldVisitor,
          typename DirectMethodVisitor,
          typename VirtualMethodVisitor>
inline void ClassAccessor::VisitFieldsAndMethods(
    const StaticFieldVisitor& static_field_visitor,
    const InstanceFieldVisitor& instance_field_visitor,
    const DirectMethodVisitor& direct_method_visitor,
    const VirtualMethodVisitor& virtual_method_visitor) const {
  Field field(dex_file_);
  const uint8_t* ptr = VisitMembers(num_static_fields_, static_field_visitor, ptr_pos_, &field);
  field.NextSection();
  ptr = VisitMembers(num_instance_fields_, instance_field_visitor, ptr, &field);

  Method method(dex_file_, /*is_static_or_direct*/ true);
  ptr = VisitMembers(num_direct_methods_, direct_method_visitor, ptr, &method);
  method.NextSection();
  ptr = VisitMembers(num_virtual_methods_, virtual_method_visitor, ptr, &method);
}

template <typename DirectMethodVisitor,
          typename VirtualMethodVisitor>
inline void ClassAccessor::VisitMethods(const DirectMethodVisitor& direct_method_visitor,
                                        const VirtualMethodVisitor& virtual_method_visitor) const {
  VisitFieldsAndMethods(VoidFunctor(),
                        VoidFunctor(),
                        direct_method_visitor,
                        virtual_method_visitor);
}

template <typename StaticFieldVisitor,
          typename InstanceFieldVisitor>
inline void ClassAccessor::VisitFields(const StaticFieldVisitor& static_field_visitor,
                                       const InstanceFieldVisitor& instance_field_visitor) const {
  VisitFieldsAndMethods(static_field_visitor,
                        instance_field_visitor,
                        VoidFunctor(),
                        VoidFunctor());
}

inline const DexFile::CodeItem* ClassAccessor::GetCodeItem(const Method& method) const {
  return dex_file_.GetCodeItem(method.GetCodeItemOffset());
}

inline CodeItemInstructionAccessor ClassAccessor::Method::GetInstructions() const {
  return CodeItemInstructionAccessor(dex_file_, dex_file_.GetCodeItem(GetCodeItemOffset()));
}

inline const char* ClassAccessor::GetDescriptor() const {
  return dex_file_.StringByTypeIdx(descriptor_index_);
}

inline const DexFile::CodeItem* ClassAccessor::Method::GetCodeItem() const {
  return dex_file_.GetCodeItem(code_off_);
}

inline IterationRange<ClassAccessor::DataIterator<ClassAccessor::Field>> ClassAccessor::GetFields()
    const {
  const uint32_t limit = num_static_fields_ + num_instance_fields_;
  return { DataIterator<Field>(dex_file_, 0u, num_static_fields_, limit, ptr_pos_),
           DataIterator<Field>(dex_file_, limit, num_static_fields_, limit, ptr_pos_) };
}

inline IterationRange<ClassAccessor::DataIterator<ClassAccessor::Method>>
    ClassAccessor::GetMethods() const {
  // Skip over the fields.
  Field field(dex_file_);
  const size_t skip_count = num_static_fields_ + num_instance_fields_;
  const uint8_t* ptr_pos = VisitMembers(skip_count, VoidFunctor(), ptr_pos_, &field);
  // Return the iterator pair for all the methods.
  const uint32_t limit = num_direct_methods_ + num_virtual_methods_;
  return { DataIterator<Method>(dex_file_, 0u, num_direct_methods_, limit, ptr_pos),
           DataIterator<Method>(dex_file_, limit, num_direct_methods_, limit, ptr_pos) };
}

}  // namespace art

#endif  // ART_LIBDEXFILE_DEX_CLASS_ACCESSOR_INL_H_
