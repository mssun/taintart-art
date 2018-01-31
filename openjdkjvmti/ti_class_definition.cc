/* Copyright (C) 2016 The Android Open Source Project
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

#include "ti_class_definition.h"

#include "base/array_slice.h"
#include "class_linker-inl.h"
#include "dex/dex_file.h"
#include "fixed_up_dex_file.h"
#include "handle.h"
#include "handle_scope-inl.h"
#include "mirror/class-inl.h"
#include "mirror/class_ext.h"
#include "mirror/object-inl.h"
#include "reflection.h"
#include "thread.h"

namespace openjdkjvmti {

bool ArtClassDefinition::IsModified() const {
  // RedefineClasses calls always are 'modified' since they need to change the current_dex_file of
  // the class.
  if (redefined_) {
    return true;
  }

  // Check to see if any change has taken place.
  if (current_dex_file_.data() == dex_data_.data()) {
    // no change at all.
    return false;
  }

  // Check if the dex file we want to set is the same as the current one.
  // Unfortunately we need to do this check even if no modifications have been done since it could
  // be that agents were removed in the mean-time so we still have a different dex file. The dex
  // checksum means this is likely to be fairly fast.
  return current_dex_file_.size() != dex_data_.size() ||
      memcmp(current_dex_file_.data(), dex_data_.data(), current_dex_file_.size()) != 0;
}

jvmtiError ArtClassDefinition::InitCommon(art::Thread* self, jclass klass) {
  art::ScopedObjectAccess soa(self);
  art::ObjPtr<art::mirror::Class> m_klass(soa.Decode<art::mirror::Class>(klass));
  if (m_klass.IsNull()) {
    return ERR(INVALID_CLASS);
  }
  initialized_ = true;
  klass_ = klass;
  loader_ = soa.AddLocalReference<jobject>(m_klass->GetClassLoader());
  std::string descriptor_store;
  std::string descriptor(m_klass->GetDescriptor(&descriptor_store));
  name_ = descriptor.substr(1, descriptor.size() - 2);
  // Android doesn't really have protection domains.
  protection_domain_ = nullptr;
  return OK;
}

static void DequickenDexFile(const art::DexFile* dex_file,
                             const char* descriptor,
                             /*out*/std::vector<unsigned char>* dex_data)
    REQUIRES_SHARED(art::Locks::mutator_lock_) {
  std::unique_ptr<FixedUpDexFile> fixed_dex_file(FixedUpDexFile::Create(*dex_file, descriptor));
  dex_data->resize(fixed_dex_file->Size());
  memcpy(dex_data->data(), fixed_dex_file->Begin(), fixed_dex_file->Size());
}

// Gets the data surrounding the given class.
static void GetDexDataForRetransformation(art::Handle<art::mirror::Class> klass,
                                          /*out*/std::vector<unsigned char>* dex_data)
    REQUIRES_SHARED(art::Locks::mutator_lock_) {
  art::StackHandleScope<3> hs(art::Thread::Current());
  art::Handle<art::mirror::ClassExt> ext(hs.NewHandle(klass->GetExtData()));
  const art::DexFile* dex_file = nullptr;
  if (!ext.IsNull()) {
    art::Handle<art::mirror::Object> orig_dex(hs.NewHandle(ext->GetOriginalDexFile()));
    if (!orig_dex.IsNull()) {
      if (orig_dex->IsArrayInstance()) {
        DCHECK(orig_dex->GetClass()->GetComponentType()->IsPrimitiveByte());
        art::Handle<art::mirror::ByteArray> orig_dex_bytes(
            hs.NewHandle(art::down_cast<art::mirror::ByteArray*>(orig_dex->AsArray())));
        dex_data->resize(orig_dex_bytes->GetLength());
        memcpy(dex_data->data(), orig_dex_bytes->GetData(), dex_data->size());
        return;
      } else if (orig_dex->IsDexCache()) {
        dex_file = orig_dex->AsDexCache()->GetDexFile();
      } else {
        DCHECK(orig_dex->GetClass()->DescriptorEquals("Ljava/lang/Long;"))
            << "Expected java/lang/Long but found object of type "
            << orig_dex->GetClass()->PrettyClass();
        art::ObjPtr<art::mirror::Class> prim_long_class(
            art::Runtime::Current()->GetClassLinker()->GetClassRoot(
                art::ClassLinker::kPrimitiveLong));
        art::JValue val;
        if (!art::UnboxPrimitiveForResult(orig_dex.Get(), prim_long_class, &val)) {
          // This should never happen.
          LOG(FATAL) << "Unable to unbox a primitive long value!";
        }
        dex_file = reinterpret_cast<const art::DexFile*>(static_cast<uintptr_t>(val.GetJ()));
      }
    }
  }
  if (dex_file == nullptr) {
    dex_file = &klass->GetDexFile();
  }
  std::string storage;
  DequickenDexFile(dex_file, klass->GetDescriptor(&storage), dex_data);
}

static bool DexNeedsDequickening(art::Handle<art::mirror::Class> klass,
                                 /*out*/ bool* from_class_ext)
    REQUIRES_SHARED(art::Locks::mutator_lock_) {
  art::ObjPtr<art::mirror::ClassExt> ext(klass->GetExtData());
  if (ext.IsNull()) {
    // We don't seem to have ever been redefined so be conservative and say we need de-quickening.
    *from_class_ext = false;
    return true;
  }
  art::ObjPtr<art::mirror::Object> orig_dex(ext->GetOriginalDexFile());
  if (orig_dex.IsNull()) {
    // We don't seem to have ever been redefined so be conservative and say we need de-quickening.
    *from_class_ext = false;
    return true;
  } else if (!orig_dex->IsArrayInstance()) {
    // We were redefined but the original is held in a dex-cache or dex file. This means that the
    // original dex file is the one from the disk, which might be quickened.
    DCHECK(orig_dex->IsDexCache() || orig_dex->GetClass()->DescriptorEquals("Ljava/lang/Long;"));
    *from_class_ext = true;
    return true;
  } else {
    // An array instance means the original-dex-file is from a redefineClasses which cannot have any
    // quickening, so it's fine to use directly.
    DCHECK(orig_dex->GetClass()->GetComponentType()->IsPrimitiveByte());
    *from_class_ext = true;
    return false;
  }
}

static const art::DexFile* GetQuickenedDexFile(art::Handle<art::mirror::Class> klass)
    REQUIRES_SHARED(art::Locks::mutator_lock_) {
  art::ObjPtr<art::mirror::ClassExt> ext(klass->GetExtData());
  if (ext.IsNull() || ext->GetOriginalDexFile() == nullptr) {
    return &klass->GetDexFile();
  }

  art::ObjPtr<art::mirror::Object> orig_dex(ext->GetOriginalDexFile());
  DCHECK(!orig_dex->IsArrayInstance());
  if (orig_dex->IsDexCache()) {
    return orig_dex->AsDexCache()->GetDexFile();
  }

  DCHECK(orig_dex->GetClass()->DescriptorEquals("Ljava/lang/Long;"))
      << "Expected java/lang/Long but found object of type "
      << orig_dex->GetClass()->PrettyClass();
  art::ObjPtr<art::mirror::Class> prim_long_class(
      art::Runtime::Current()->GetClassLinker()->GetClassRoot(
          art::ClassLinker::kPrimitiveLong));
  art::JValue val;
  if (!art::UnboxPrimitiveForResult(orig_dex.Ptr(), prim_long_class, &val)) {
    LOG(FATAL) << "Unable to unwrap a long value!";
  }
  return reinterpret_cast<const art::DexFile*>(static_cast<uintptr_t>(val.GetJ()));
}

template<typename GetOriginalDexFile>
void ArtClassDefinition::InitWithDex(GetOriginalDexFile get_original,
                                     const art::DexFile* quick_dex) {
  art::Thread* self = art::Thread::Current();
  DCHECK(quick_dex != nullptr);
  get_original(/*out*/&dex_data_memory_);
  dex_data_ = art::ArrayRef<const unsigned char>(dex_data_memory_);
  if (from_class_ext_) {
    // We got initial from class_ext so the current one must have undergone redefinition so no
    // cdex or quickening stuff.
    // We can only do this if it's not a first load.
    DCHECK(klass_ != nullptr);
    const art::DexFile& cur_dex = self->DecodeJObject(klass_)->AsClass()->GetDexFile();
    current_dex_file_ = art::ArrayRef<const unsigned char>(cur_dex.Begin(), cur_dex.Size());
  } else {
    // No redefinition must have ever happened so the (dequickened) cur_dex is the same as the
    // initial dex_data. We need to copy it into another buffer to keep it around if we have a
    // real redefinition.
    current_dex_memory_.resize(dex_data_.size());
    memcpy(current_dex_memory_.data(), dex_data_.data(), current_dex_memory_.size());
    current_dex_file_ = art::ArrayRef<const unsigned char>(current_dex_memory_);
  }
}

jvmtiError ArtClassDefinition::Init(art::Thread* self, jclass klass) {
  jvmtiError res = InitCommon(self, klass);
  if (res != OK) {
    return res;
  }
  art::ScopedObjectAccess soa(self);
  art::StackHandleScope<1> hs(self);
  art::Handle<art::mirror::Class> m_klass(hs.NewHandle(self->DecodeJObject(klass)->AsClass()));
  if (!DexNeedsDequickening(m_klass, &from_class_ext_)) {
    // We don't need to do any dequickening. We want to copy the data just so we don't need to deal
    // with the GC moving it around.
    art::ObjPtr<art::mirror::ByteArray> orig_dex(
        m_klass->GetExtData()->GetOriginalDexFile()->AsByteArray());
    dex_data_memory_.resize(orig_dex->GetLength());
    memcpy(dex_data_memory_.data(), orig_dex->GetData(), dex_data_memory_.size());
    dex_data_ = art::ArrayRef<const unsigned char>(dex_data_memory_);

    // Since we are here we must not have any quickened instructions since we were redefined.
    const art::DexFile& cur_dex = m_klass->GetDexFile();
    DCHECK(from_class_ext_);
    current_dex_file_ = art::ArrayRef<const unsigned char>(cur_dex.Begin(), cur_dex.Size());
    return OK;
  }

  // We need to dequicken stuff. This is often super slow (10's of ms). Instead we will do it
  // dynamically.
  const art::DexFile* quick_dex = GetQuickenedDexFile(m_klass);
  auto get_original = [&](/*out*/std::vector<unsigned char>* dex_data)
      REQUIRES_SHARED(art::Locks::mutator_lock_) {
    GetDexDataForRetransformation(m_klass, dex_data);
  };
  InitWithDex(get_original, quick_dex);
  return OK;
}

jvmtiError ArtClassDefinition::Init(art::Thread* self, const jvmtiClassDefinition& def) {
  jvmtiError res = InitCommon(self, def.klass);
  if (res != OK) {
    return res;
  }
  // We are being directly redefined.
  redefined_ = true;
  current_dex_file_ = art::ArrayRef<const unsigned char>(def.class_bytes, def.class_byte_count);
  dex_data_ = art::ArrayRef<const unsigned char>(def.class_bytes, def.class_byte_count);
  return OK;
}

void ArtClassDefinition::InitFirstLoad(const char* descriptor,
                                       art::Handle<art::mirror::ClassLoader> klass_loader,
                                       const art::DexFile& dex_file) {
  art::Thread* self = art::Thread::Current();
  art::ScopedObjectAccess soa(self);
  initialized_ = true;
  // No Class
  klass_ = nullptr;
  loader_ = soa.AddLocalReference<jobject>(klass_loader.Get());
  std::string descriptor_str(descriptor);
  name_ = descriptor_str.substr(1, descriptor_str.size() - 2);
  // Android doesn't really have protection domains.
  protection_domain_ = nullptr;
  auto get_original = [&](/*out*/std::vector<unsigned char>* dex_data)
      REQUIRES_SHARED(art::Locks::mutator_lock_) {
    DequickenDexFile(&dex_file, descriptor, dex_data);
  };
  InitWithDex(get_original, &dex_file);
}

}  // namespace openjdkjvmti
