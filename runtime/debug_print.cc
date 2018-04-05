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

#include <sstream>

#include "debug_print.h"

#include "class_linker.h"
#include "class_table.h"
#include "class_loader_utils.h"
#include "dex/utf.h"
#include "gc/heap.h"
#include "gc/space/space-inl.h"
#include "mirror/class.h"
#include "mirror/class_loader.h"
#include "runtime.h"
#include "scoped_thread_state_change-inl.h"
#include "thread-current-inl.h"
#include "well_known_classes.h"

namespace art {

std::string DescribeSpace(ObjPtr<mirror::Class> klass) {
  std::ostringstream oss;
  gc::Heap* heap = Runtime::Current()->GetHeap();
  gc::space::ContinuousSpace* cs =
      heap->FindContinuousSpaceFromObject(klass.Ptr(), /* fail_ok */ true);
  if (cs != nullptr) {
    if (cs->IsImageSpace()) {
      oss << "image;" << cs->GetName() << ";" << cs->AsImageSpace()->GetImageFilename();
    } else {
      oss << "continuous;" << cs->GetName();
    }
  } else {
    gc::space::DiscontinuousSpace* ds =
        heap->FindDiscontinuousSpaceFromObject(klass, /* fail_ok */ true);
    if (ds != nullptr) {
      oss << "discontinuous;" << ds->GetName();
    } else {
      oss << "invalid";
    }
  }
  return oss.str();
}

std::string DescribeLoaders(ObjPtr<mirror::ClassLoader> loader, const char* class_descriptor) {
  std::ostringstream oss;
  uint32_t hash = ComputeModifiedUtf8Hash(class_descriptor);
  ObjPtr<mirror::Class> path_class_loader =
      WellKnownClasses::ToClass(WellKnownClasses::dalvik_system_PathClassLoader);
  ObjPtr<mirror::Class> dex_class_loader =
      WellKnownClasses::ToClass(WellKnownClasses::dalvik_system_DexClassLoader);
  ObjPtr<mirror::Class> delegate_last_class_loader =
      WellKnownClasses::ToClass(WellKnownClasses::dalvik_system_DelegateLastClassLoader);

  // Print the class loader chain.
  bool found_class = false;
  const char* loader_separator = "";
  if (loader == nullptr) {
    oss << "BootClassLoader";  // This would be unexpected.
  }
  for (; loader != nullptr; loader = loader->GetParent()) {
    oss << loader_separator << loader->GetClass()->PrettyDescriptor();
    loader_separator = ";";
    // If we didn't find the class yet, try to find it in the current class loader.
    if (!found_class) {
      ClassTable* table = Runtime::Current()->GetClassLinker()->ClassTableForClassLoader(loader);
      ObjPtr<mirror::Class> klass =
          (table != nullptr) ? table->Lookup(class_descriptor, hash) : nullptr;
      if (klass != nullptr) {
        found_class = true;
        oss << "[hit:" << DescribeSpace(klass) << "]";
      }
    }

    // For PathClassLoader, DexClassLoader or DelegateLastClassLoader
    // also dump the dex file locations.
    if (loader->GetClass() == path_class_loader ||
        loader->GetClass() == dex_class_loader ||
        loader->GetClass() == delegate_last_class_loader) {
      oss << "(";
      ScopedObjectAccessUnchecked soa(Thread::Current());
      StackHandleScope<1> hs(soa.Self());
      Handle<mirror::ClassLoader> handle(hs.NewHandle(loader));
      const char* path_separator = "";
      VisitClassLoaderDexFiles(soa,
                               handle,
                               [&](const DexFile* dex_file) {
                                 oss << path_separator << dex_file->GetLocation();
                                 path_separator = ":";
                                 return true;  // Continue with the next DexFile.
                               });
      oss << ")";
    }
  }

  return oss.str();
}

}  // namespace art
