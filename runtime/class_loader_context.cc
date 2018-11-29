/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include "class_loader_context.h"

#include <android-base/parseint.h>
#include <android-base/strings.h>

#include "art_field-inl.h"
#include "base/casts.h"
#include "base/dchecked_vector.h"
#include "base/stl_util.h"
#include "class_linker.h"
#include "class_loader_utils.h"
#include "class_root.h"
#include "dex/art_dex_file_loader.h"
#include "dex/dex_file.h"
#include "dex/dex_file_loader.h"
#include "handle_scope-inl.h"
#include "jni/jni_internal.h"
#include "mirror/object_array-alloc-inl.h"
#include "nativehelper/scoped_local_ref.h"
#include "oat_file_assistant.h"
#include "obj_ptr-inl.h"
#include "runtime.h"
#include "scoped_thread_state_change-inl.h"
#include "thread.h"
#include "well_known_classes.h"

namespace art {

static constexpr char kPathClassLoaderString[] = "PCL";
static constexpr char kDelegateLastClassLoaderString[] = "DLC";
static constexpr char kClassLoaderOpeningMark = '[';
static constexpr char kClassLoaderClosingMark = ']';
static constexpr char kClassLoaderSharedLibraryOpeningMark = '{';
static constexpr char kClassLoaderSharedLibraryClosingMark = '}';
static constexpr char kClassLoaderSharedLibrarySeparator = '#';
static constexpr char kClassLoaderSeparator = ';';
static constexpr char kClasspathSeparator = ':';
static constexpr char kDexFileChecksumSeparator = '*';

ClassLoaderContext::ClassLoaderContext()
    : special_shared_library_(false),
      dex_files_open_attempted_(false),
      dex_files_open_result_(false),
      owns_the_dex_files_(true) {}

ClassLoaderContext::ClassLoaderContext(bool owns_the_dex_files)
    : special_shared_library_(false),
      dex_files_open_attempted_(true),
      dex_files_open_result_(true),
      owns_the_dex_files_(owns_the_dex_files) {}

// Utility method to add parent and shared libraries of `info` into
// the `work_list`.
static void AddToWorkList(
    ClassLoaderContext::ClassLoaderInfo* info,
    std::vector<ClassLoaderContext::ClassLoaderInfo*>& work_list) {
  if (info->parent != nullptr) {
    work_list.push_back(info->parent.get());
  }
  for (size_t i = 0; i < info->shared_libraries.size(); ++i) {
    work_list.push_back(info->shared_libraries[i].get());
  }
}

ClassLoaderContext::~ClassLoaderContext() {
  if (!owns_the_dex_files_ && class_loader_chain_ != nullptr) {
    // If the context does not own the dex/oat files release the unique pointers to
    // make sure we do not de-allocate them.
    std::vector<ClassLoaderInfo*> work_list;
    work_list.push_back(class_loader_chain_.get());
    while (!work_list.empty()) {
      ClassLoaderInfo* info = work_list.back();
      work_list.pop_back();
      for (std::unique_ptr<OatFile>& oat_file : info->opened_oat_files) {
        oat_file.release();  // NOLINT b/117926937
      }
      for (std::unique_ptr<const DexFile>& dex_file : info->opened_dex_files) {
        dex_file.release();  // NOLINT b/117926937
      }
      AddToWorkList(info, work_list);
    }
  }
}

std::unique_ptr<ClassLoaderContext> ClassLoaderContext::Default() {
  return Create("");
}

std::unique_ptr<ClassLoaderContext> ClassLoaderContext::Create(const std::string& spec) {
  std::unique_ptr<ClassLoaderContext> result(new ClassLoaderContext());
  if (result->Parse(spec)) {
    return result;
  } else {
    return nullptr;
  }
}

static size_t FindMatchingSharedLibraryCloseMarker(const std::string& spec,
                                                   size_t shared_library_open_index) {
  // Counter of opened shared library marker we've encountered so far.
  uint32_t counter = 1;
  // The index at which we're operating in the loop.
  uint32_t string_index = shared_library_open_index + 1;
  size_t shared_library_close = std::string::npos;
  while (counter != 0) {
    shared_library_close =
        spec.find_first_of(kClassLoaderSharedLibraryClosingMark, string_index);
    size_t shared_library_open =
        spec.find_first_of(kClassLoaderSharedLibraryOpeningMark, string_index);
    if (shared_library_close == std::string::npos) {
      // No matching closing marker. Return an error.
      break;
    }

    if ((shared_library_open == std::string::npos) ||
        (shared_library_close < shared_library_open)) {
      // We have seen a closing marker. Decrement the counter.
      --counter;
      // Move the search index forward.
      string_index = shared_library_close + 1;
    } else {
      // New nested opening marker. Increment the counter and move the search
      // index after the marker.
      ++counter;
      string_index = shared_library_open + 1;
    }
  }
  return shared_library_close;
}

// The expected format is:
// "ClassLoaderType1[ClasspathElem1*Checksum1:ClasspathElem2*Checksum2...]{ClassLoaderType2[...]}".
// The checksum part of the format is expected only if parse_cheksums is true.
std::unique_ptr<ClassLoaderContext::ClassLoaderInfo> ClassLoaderContext::ParseClassLoaderSpec(
    const std::string& class_loader_spec,
    bool parse_checksums) {
  ClassLoaderType class_loader_type = ExtractClassLoaderType(class_loader_spec);
  if (class_loader_type == kInvalidClassLoader) {
    return nullptr;
  }
  const char* class_loader_type_str = GetClassLoaderTypeName(class_loader_type);
  size_t type_str_size = strlen(class_loader_type_str);

  CHECK_EQ(0, class_loader_spec.compare(0, type_str_size, class_loader_type_str));

  // Check the opening and closing markers.
  if (class_loader_spec[type_str_size] != kClassLoaderOpeningMark) {
    return nullptr;
  }
  if ((class_loader_spec[class_loader_spec.length() - 1] != kClassLoaderClosingMark) &&
      (class_loader_spec[class_loader_spec.length() - 1] != kClassLoaderSharedLibraryClosingMark)) {
    return nullptr;
  }

  size_t closing_index = class_loader_spec.find_first_of(kClassLoaderClosingMark);

  // At this point we know the format is ok; continue and extract the classpath.
  // Note that class loaders with an empty class path are allowed.
  std::string classpath = class_loader_spec.substr(type_str_size + 1,
                                                   closing_index - type_str_size - 1);

  std::unique_ptr<ClassLoaderInfo> info(new ClassLoaderInfo(class_loader_type));

  if (!parse_checksums) {
    Split(classpath, kClasspathSeparator, &info->classpath);
  } else {
    std::vector<std::string> classpath_elements;
    Split(classpath, kClasspathSeparator, &classpath_elements);
    for (const std::string& element : classpath_elements) {
      std::vector<std::string> dex_file_with_checksum;
      Split(element, kDexFileChecksumSeparator, &dex_file_with_checksum);
      if (dex_file_with_checksum.size() != 2) {
        return nullptr;
      }
      uint32_t checksum = 0;
      if (!android::base::ParseUint(dex_file_with_checksum[1].c_str(), &checksum)) {
        return nullptr;
      }
      info->classpath.push_back(dex_file_with_checksum[0]);
      info->checksums.push_back(checksum);
    }
  }

  if ((class_loader_spec[class_loader_spec.length() - 1] == kClassLoaderSharedLibraryClosingMark) &&
      (class_loader_spec[class_loader_spec.length() - 2] != kClassLoaderSharedLibraryOpeningMark)) {
    // Non-empty list of shared libraries.
    size_t start_index = class_loader_spec.find_first_of(kClassLoaderSharedLibraryOpeningMark);
    if (start_index == std::string::npos) {
      return nullptr;
    }
    std::string shared_libraries_spec =
        class_loader_spec.substr(start_index + 1, class_loader_spec.length() - start_index - 2);
    std::vector<std::string> shared_libraries;
    size_t cursor = 0;
    while (cursor != shared_libraries_spec.length()) {
      size_t shared_library_separator =
          shared_libraries_spec.find_first_of(kClassLoaderSharedLibrarySeparator, cursor);
      size_t shared_library_open =
          shared_libraries_spec.find_first_of(kClassLoaderSharedLibraryOpeningMark, cursor);
      std::string shared_library_spec;
      if (shared_library_separator == std::string::npos) {
        // Only one shared library, for example:
        // PCL[...]
        shared_library_spec =
            shared_libraries_spec.substr(cursor, shared_libraries_spec.length() - cursor);
        cursor = shared_libraries_spec.length();
      } else if ((shared_library_open == std::string::npos) ||
                 (shared_library_open > shared_library_separator)) {
        // We found a shared library without nested shared libraries, for example:
        // PCL[...]#PCL[...]{...}
        shared_library_spec =
            shared_libraries_spec.substr(cursor, shared_library_separator - cursor);
        cursor = shared_library_separator + 1;
      } else {
        // The shared library contains nested shared libraries. Find the matching closing shared
        // marker for it.
        size_t closing_marker =
            FindMatchingSharedLibraryCloseMarker(shared_libraries_spec, shared_library_open);
        if (closing_marker == std::string::npos) {
          // No matching closing marker, return an error.
          return nullptr;
        }
        shared_library_spec = shared_libraries_spec.substr(cursor, closing_marker + 1 - cursor);
        cursor = closing_marker + 1;
        if (cursor != shared_libraries_spec.length() &&
            shared_libraries_spec[cursor] == kClassLoaderSharedLibrarySeparator) {
          // Pass the shared library separator marker.
          ++cursor;
        }
      }
      std::unique_ptr<ClassLoaderInfo> shared_library(
          ParseInternal(shared_library_spec, parse_checksums));
      if (shared_library == nullptr) {
        return nullptr;
      }
      info->shared_libraries.push_back(std::move(shared_library));
    }
  }

  return info;
}

// Extracts the class loader type from the given spec.
// Return ClassLoaderContext::kInvalidClassLoader if the class loader type is not
// recognized.
ClassLoaderContext::ClassLoaderType
ClassLoaderContext::ExtractClassLoaderType(const std::string& class_loader_spec) {
  const ClassLoaderType kValidTypes[] = {kPathClassLoader, kDelegateLastClassLoader};
  for (const ClassLoaderType& type : kValidTypes) {
    const char* type_str = GetClassLoaderTypeName(type);
    if (class_loader_spec.compare(0, strlen(type_str), type_str) == 0) {
      return type;
    }
  }
  return kInvalidClassLoader;
}

// The format: ClassLoaderType1[ClasspathElem1:ClasspathElem2...];ClassLoaderType2[...]...
// ClassLoaderType is either "PCL" (PathClassLoader) or "DLC" (DelegateLastClassLoader).
// ClasspathElem is the path of dex/jar/apk file.
bool ClassLoaderContext::Parse(const std::string& spec, bool parse_checksums) {
  if (spec.empty()) {
    // By default we load the dex files in a PathClassLoader.
    // So an empty spec is equivalent to an empty PathClassLoader (this happens when running
    // tests)
    class_loader_chain_.reset(new ClassLoaderInfo(kPathClassLoader));
    return true;
  }

  // Stop early if we detect the special shared library, which may be passed as the classpath
  // for dex2oat when we want to skip the shared libraries check.
  if (spec == OatFile::kSpecialSharedLibrary) {
    LOG(INFO) << "The ClassLoaderContext is a special shared library.";
    special_shared_library_ = true;
    return true;
  }

  CHECK(class_loader_chain_ == nullptr);
  class_loader_chain_.reset(ParseInternal(spec, parse_checksums));
  return class_loader_chain_ != nullptr;
}

ClassLoaderContext::ClassLoaderInfo* ClassLoaderContext::ParseInternal(
    const std::string& spec, bool parse_checksums) {
  CHECK(!spec.empty());
  CHECK_NE(spec, OatFile::kSpecialSharedLibrary);
  std::string remaining = spec;
  std::unique_ptr<ClassLoaderInfo> first(nullptr);
  ClassLoaderInfo* previous_iteration = nullptr;
  while (!remaining.empty()) {
    std::string class_loader_spec;
    size_t first_class_loader_separator = remaining.find_first_of(kClassLoaderSeparator);
    size_t first_shared_library_open =
        remaining.find_first_of(kClassLoaderSharedLibraryOpeningMark);
    if (first_class_loader_separator == std::string::npos) {
      // Only one class loader, for example:
      // PCL[...]
      class_loader_spec = remaining;
      remaining = "";
    } else if ((first_shared_library_open == std::string::npos) ||
               (first_shared_library_open > first_class_loader_separator)) {
      // We found a class loader spec without shared libraries, for example:
      // PCL[...];PCL[...]{...}
      class_loader_spec = remaining.substr(0, first_class_loader_separator);
      remaining = remaining.substr(first_class_loader_separator + 1,
                                   remaining.size() - first_class_loader_separator - 1);
    } else {
      // The class loader spec contains shared libraries. Find the matching closing
      // shared library marker for it.

      uint32_t shared_library_close =
          FindMatchingSharedLibraryCloseMarker(remaining, first_shared_library_open);
      if (shared_library_close == std::string::npos) {
        LOG(ERROR) << "Invalid class loader spec: " << class_loader_spec;
        return nullptr;
      }
      class_loader_spec = remaining.substr(0, shared_library_close + 1);

      // Compute the remaining string to analyze.
      if (remaining.size() == shared_library_close + 1) {
        remaining = "";
      } else if ((remaining.size() == shared_library_close + 2) ||
                 (remaining.at(shared_library_close + 1) != kClassLoaderSeparator)) {
        LOG(ERROR) << "Invalid class loader spec: " << class_loader_spec;
        return nullptr;
      } else {
        remaining = remaining.substr(shared_library_close + 2,
                                     remaining.size() - shared_library_close - 2);
      }
    }

    std::unique_ptr<ClassLoaderInfo> info =
        ParseClassLoaderSpec(class_loader_spec, parse_checksums);
    if (info == nullptr) {
      LOG(ERROR) << "Invalid class loader spec: " << class_loader_spec;
      return nullptr;
    }
    if (first == nullptr) {
      first = std::move(info);
      previous_iteration = first.get();
    } else {
      CHECK(previous_iteration != nullptr);
      previous_iteration->parent = std::move(info);
      previous_iteration = previous_iteration->parent.get();
    }
  }
  return first.release();
}

// Opens requested class path files and appends them to opened_dex_files. If the dex files have
// been stripped, this opens them from their oat files (which get added to opened_oat_files).
bool ClassLoaderContext::OpenDexFiles(InstructionSet isa, const std::string& classpath_dir) {
  if (dex_files_open_attempted_) {
    // Do not attempt to re-open the files if we already tried.
    return dex_files_open_result_;
  }

  dex_files_open_attempted_ = true;
  // Assume we can open all dex files. If not, we will set this to false as we go.
  dex_files_open_result_ = true;

  if (special_shared_library_) {
    // Nothing to open if the context is a special shared library.
    return true;
  }

  // Note that we try to open all dex files even if some fail.
  // We may get resource-only apks which we cannot load.
  // TODO(calin): Refine the dex opening interface to be able to tell if an archive contains
  // no dex files. So that we can distinguish the real failures...
  const ArtDexFileLoader dex_file_loader;
  std::vector<ClassLoaderInfo*> work_list;
  CHECK(class_loader_chain_ != nullptr);
  work_list.push_back(class_loader_chain_.get());
  while (!work_list.empty()) {
    ClassLoaderInfo* info = work_list.back();
    work_list.pop_back();
    size_t opened_dex_files_index = info->opened_dex_files.size();
    for (const std::string& cp_elem : info->classpath) {
      // If path is relative, append it to the provided base directory.
      std::string location = cp_elem;
      if (location[0] != '/' && !classpath_dir.empty()) {
        location = classpath_dir + (classpath_dir.back() == '/' ? "" : "/") + location;
      }

      std::string error_msg;
      // When opening the dex files from the context we expect their checksum to match their
      // contents. So pass true to verify_checksum.
      if (!dex_file_loader.Open(location.c_str(),
                                location.c_str(),
                                Runtime::Current()->IsVerificationEnabled(),
                                /*verify_checksum=*/ true,
                                &error_msg,
                                &info->opened_dex_files)) {
        // If we fail to open the dex file because it's been stripped, try to open the dex file
        // from its corresponding oat file.
        // This could happen when we need to recompile a pre-build whose dex code has been stripped.
        // (for example, if the pre-build is only quicken and we want to re-compile it
        // speed-profile).
        // TODO(calin): Use the vdex directly instead of going through the oat file.
        OatFileAssistant oat_file_assistant(location.c_str(), isa, false);
        std::unique_ptr<OatFile> oat_file(oat_file_assistant.GetBestOatFile());
        std::vector<std::unique_ptr<const DexFile>> oat_dex_files;
        if (oat_file != nullptr &&
            OatFileAssistant::LoadDexFiles(*oat_file, location, &oat_dex_files)) {
          info->opened_oat_files.push_back(std::move(oat_file));
          info->opened_dex_files.insert(info->opened_dex_files.end(),
                                        std::make_move_iterator(oat_dex_files.begin()),
                                        std::make_move_iterator(oat_dex_files.end()));
        } else {
          LOG(WARNING) << "Could not open dex files from location: " << location;
          dex_files_open_result_ = false;
        }
      }
    }

    // We finished opening the dex files from the classpath.
    // Now update the classpath and the checksum with the locations of the dex files.
    //
    // We do this because initially the classpath contains the paths of the dex files; and
    // some of them might be multi-dexes. So in order to have a consistent view we replace all the
    // file paths with the actual dex locations being loaded.
    // This will allow the context to VerifyClassLoaderContextMatch which expects or multidex
    // location in the class paths.
    // Note that this will also remove the paths that could not be opened.
    info->original_classpath = std::move(info->classpath);
    info->classpath.clear();
    info->checksums.clear();
    for (size_t k = opened_dex_files_index; k < info->opened_dex_files.size(); k++) {
      std::unique_ptr<const DexFile>& dex = info->opened_dex_files[k];
      info->classpath.push_back(dex->GetLocation());
      info->checksums.push_back(dex->GetLocationChecksum());
    }
    AddToWorkList(info, work_list);
  }

  return dex_files_open_result_;
}

bool ClassLoaderContext::RemoveLocationsFromClassPaths(
    const dchecked_vector<std::string>& locations) {
  CHECK(!dex_files_open_attempted_)
      << "RemoveLocationsFromClasspaths cannot be call after OpenDexFiles";

  if (class_loader_chain_ == nullptr) {
    return false;
  }

  std::set<std::string> canonical_locations;
  for (const std::string& location : locations) {
    canonical_locations.insert(DexFileLoader::GetDexCanonicalLocation(location.c_str()));
  }
  bool removed_locations = false;
  std::vector<ClassLoaderInfo*> work_list;
  work_list.push_back(class_loader_chain_.get());
  while (!work_list.empty()) {
    ClassLoaderInfo* info = work_list.back();
    work_list.pop_back();
    size_t initial_size = info->classpath.size();
    auto kept_it = std::remove_if(
        info->classpath.begin(),
        info->classpath.end(),
        [canonical_locations](const std::string& location) {
            return ContainsElement(canonical_locations,
                                   DexFileLoader::GetDexCanonicalLocation(location.c_str()));
        });
    info->classpath.erase(kept_it, info->classpath.end());
    if (initial_size != info->classpath.size()) {
      removed_locations = true;
    }
    AddToWorkList(info, work_list);
  }
  return removed_locations;
}

std::string ClassLoaderContext::EncodeContextForDex2oat(const std::string& base_dir) const {
  return EncodeContext(base_dir, /*for_dex2oat=*/ true, /*stored_context=*/ nullptr);
}

std::string ClassLoaderContext::EncodeContextForOatFile(const std::string& base_dir,
                                                        ClassLoaderContext* stored_context) const {
  return EncodeContext(base_dir, /*for_dex2oat=*/ false, stored_context);
}

std::string ClassLoaderContext::EncodeContext(const std::string& base_dir,
                                              bool for_dex2oat,
                                              ClassLoaderContext* stored_context) const {
  CheckDexFilesOpened("EncodeContextForOatFile");
  if (special_shared_library_) {
    return OatFile::kSpecialSharedLibrary;
  }

  if (stored_context != nullptr) {
    DCHECK_EQ(GetParentChainSize(), stored_context->GetParentChainSize());
  }

  std::ostringstream out;
  if (class_loader_chain_ == nullptr) {
    // We can get in this situation if the context was created with a class path containing the
    // source dex files which were later removed (happens during run-tests).
    out << GetClassLoaderTypeName(kPathClassLoader)
        << kClassLoaderOpeningMark
        << kClassLoaderClosingMark;
    return out.str();
  }

  EncodeContextInternal(
      *class_loader_chain_,
      base_dir,
      for_dex2oat,
      (stored_context == nullptr ? nullptr : stored_context->class_loader_chain_.get()),
      out);
  return out.str();
}

void ClassLoaderContext::EncodeContextInternal(const ClassLoaderInfo& info,
                                               const std::string& base_dir,
                                               bool for_dex2oat,
                                               ClassLoaderInfo* stored_info,
                                               std::ostringstream& out) const {
  out << GetClassLoaderTypeName(info.type);
  out << kClassLoaderOpeningMark;
  std::set<std::string> seen_locations;
  SafeMap<std::string, std::string> remap;
  if (stored_info != nullptr) {
    for (size_t k = 0; k < info.original_classpath.size(); ++k) {
      // Note that we don't care if the same name appears twice.
      remap.Put(info.original_classpath[k], stored_info->classpath[k]);
    }
  }
  for (size_t k = 0; k < info.opened_dex_files.size(); k++) {
    const std::unique_ptr<const DexFile>& dex_file = info.opened_dex_files[k];
    if (for_dex2oat) {
      // dex2oat only needs the base location. It cannot accept multidex locations.
      // So ensure we only add each file once.
      bool new_insert = seen_locations.insert(
          DexFileLoader::GetBaseLocation(dex_file->GetLocation())).second;
      if (!new_insert) {
        continue;
      }
    }
    std::string location = dex_file->GetLocation();
    // If there is a stored class loader remap, fix up the multidex strings.
    if (!remap.empty()) {
      std::string base_dex_location = DexFileLoader::GetBaseLocation(location);
      auto it = remap.find(base_dex_location);
      CHECK(it != remap.end()) << base_dex_location;
      location = it->second + DexFileLoader::GetMultiDexSuffix(location);
    }
    if (k > 0) {
      out << kClasspathSeparator;
    }
    // Find paths that were relative and convert them back from absolute.
    if (!base_dir.empty() && location.substr(0, base_dir.length()) == base_dir) {
      out << location.substr(base_dir.length() + 1).c_str();
    } else {
      out << location.c_str();
    }
    // dex2oat does not need the checksums.
    if (!for_dex2oat) {
      out << kDexFileChecksumSeparator;
      out << dex_file->GetLocationChecksum();
    }
  }
  out << kClassLoaderClosingMark;

  if (!info.shared_libraries.empty()) {
    out << kClassLoaderSharedLibraryOpeningMark;
    for (uint32_t i = 0; i < info.shared_libraries.size(); ++i) {
      if (i > 0) {
        out << kClassLoaderSharedLibrarySeparator;
      }
      EncodeContextInternal(
          *info.shared_libraries[i].get(),
          base_dir,
          for_dex2oat,
          (stored_info == nullptr ? nullptr : stored_info->shared_libraries[i].get()),
          out);
    }
    out << kClassLoaderSharedLibraryClosingMark;
  }
  if (info.parent != nullptr) {
    out << kClassLoaderSeparator;
    EncodeContextInternal(
        *info.parent.get(),
        base_dir,
        for_dex2oat,
        (stored_info == nullptr ? nullptr : stored_info->parent.get()),
        out);
  }
}

// Returns the WellKnownClass for the given class loader type.
static jclass GetClassLoaderClass(ClassLoaderContext::ClassLoaderType type) {
  switch (type) {
    case ClassLoaderContext::kPathClassLoader:
      return WellKnownClasses::dalvik_system_PathClassLoader;
    case ClassLoaderContext::kDelegateLastClassLoader:
      return WellKnownClasses::dalvik_system_DelegateLastClassLoader;
    case ClassLoaderContext::kInvalidClassLoader: break;  // will fail after the switch.
  }
  LOG(FATAL) << "Invalid class loader type " << type;
  UNREACHABLE();
}

static std::string FlattenClasspath(const std::vector<std::string>& classpath) {
  return android::base::Join(classpath, ':');
}

static ObjPtr<mirror::ClassLoader> CreateClassLoaderInternal(
    Thread* self,
    ScopedObjectAccess& soa,
    const ClassLoaderContext::ClassLoaderInfo& info,
    bool for_shared_library,
    VariableSizedHandleScope& map_scope,
    std::map<std::string, Handle<mirror::ClassLoader>>& canonicalized_libraries,
    bool add_compilation_sources,
    const std::vector<const DexFile*>& compilation_sources)
      REQUIRES_SHARED(Locks::mutator_lock_) {
  if (for_shared_library) {
    // Check if the shared library has already been created.
    auto search = canonicalized_libraries.find(FlattenClasspath(info.classpath));
    if (search != canonicalized_libraries.end()) {
      return search->second.Get();
    }
  }

  StackHandleScope<3> hs(self);
  MutableHandle<mirror::ObjectArray<mirror::ClassLoader>> libraries(
      hs.NewHandle<mirror::ObjectArray<mirror::ClassLoader>>(nullptr));

  if (!info.shared_libraries.empty()) {
    libraries.Assign(mirror::ObjectArray<mirror::ClassLoader>::Alloc(
        self,
        GetClassRoot<mirror::ObjectArray<mirror::ClassLoader>>(),
        info.shared_libraries.size()));
    for (uint32_t i = 0; i < info.shared_libraries.size(); ++i) {
      // We should only add the compilation sources to the first class loader.
      libraries->Set(i,
                     CreateClassLoaderInternal(
                         self,
                         soa,
                         *info.shared_libraries[i].get(),
                         /* for_shared_library= */ true,
                         map_scope,
                         canonicalized_libraries,
                         /* add_compilation_sources= */ false,
                         compilation_sources));
    }
  }

  MutableHandle<mirror::ClassLoader> parent = hs.NewHandle<mirror::ClassLoader>(nullptr);
  if (info.parent != nullptr) {
    // We should only add the compilation sources to the first class loader.
    parent.Assign(CreateClassLoaderInternal(
        self,
        soa,
        *info.parent.get(),
        /* for_shared_library= */ false,
        map_scope,
        canonicalized_libraries,
        /* add_compilation_sources= */ false,
        compilation_sources));
  }
  std::vector<const DexFile*> class_path_files = MakeNonOwningPointerVector(
      info.opened_dex_files);
  if (add_compilation_sources) {
    // For the first class loader, its classpath comes first, followed by compilation sources.
    // This ensures that whenever we need to resolve classes from it the classpath elements
    // come first.
    class_path_files.insert(class_path_files.end(),
                            compilation_sources.begin(),
                            compilation_sources.end());
  }
  Handle<mirror::Class> loader_class = hs.NewHandle<mirror::Class>(
      soa.Decode<mirror::Class>(GetClassLoaderClass(info.type)));
  ObjPtr<mirror::ClassLoader> loader =
      Runtime::Current()->GetClassLinker()->CreateWellKnownClassLoader(
          self,
          class_path_files,
          loader_class,
          parent,
          libraries);
  if (for_shared_library) {
    canonicalized_libraries[FlattenClasspath(info.classpath)] =
        map_scope.NewHandle<mirror::ClassLoader>(loader);
  }
  return loader;
}

jobject ClassLoaderContext::CreateClassLoader(
    const std::vector<const DexFile*>& compilation_sources) const {
  CheckDexFilesOpened("CreateClassLoader");

  Thread* self = Thread::Current();
  ScopedObjectAccess soa(self);

  ClassLinker* const class_linker = Runtime::Current()->GetClassLinker();

  if (class_loader_chain_ == nullptr) {
    CHECK(special_shared_library_);
    return class_linker->CreatePathClassLoader(self, compilation_sources);
  }

  // Create a map of canonicalized shared libraries. As we're holding objects,
  // we're creating a variable size handle scope to put handles in the map.
  VariableSizedHandleScope map_scope(self);
  std::map<std::string, Handle<mirror::ClassLoader>> canonicalized_libraries;

  // Create the class loader.
  ObjPtr<mirror::ClassLoader> loader =
      CreateClassLoaderInternal(self,
                                soa,
                                *class_loader_chain_.get(),
                                /* for_shared_library= */ false,
                                map_scope,
                                canonicalized_libraries,
                                /* add_compilation_sources= */ true,
                                compilation_sources);
  // Make it a global ref and return.
  ScopedLocalRef<jobject> local_ref(
      soa.Env(), soa.Env()->AddLocalReference<jobject>(loader));
  return soa.Env()->NewGlobalRef(local_ref.get());
}

std::vector<const DexFile*> ClassLoaderContext::FlattenOpenedDexFiles() const {
  CheckDexFilesOpened("FlattenOpenedDexFiles");

  std::vector<const DexFile*> result;
  if (class_loader_chain_ == nullptr) {
    return result;
  }
  std::vector<ClassLoaderInfo*> work_list;
  work_list.push_back(class_loader_chain_.get());
  while (!work_list.empty()) {
    ClassLoaderInfo* info = work_list.back();
    work_list.pop_back();
    for (const std::unique_ptr<const DexFile>& dex_file : info->opened_dex_files) {
      result.push_back(dex_file.get());
    }
    AddToWorkList(info, work_list);
  }
  return result;
}

const char* ClassLoaderContext::GetClassLoaderTypeName(ClassLoaderType type) {
  switch (type) {
    case kPathClassLoader: return kPathClassLoaderString;
    case kDelegateLastClassLoader: return kDelegateLastClassLoaderString;
    default:
      LOG(FATAL) << "Invalid class loader type " << type;
      UNREACHABLE();
  }
}

void ClassLoaderContext::CheckDexFilesOpened(const std::string& calling_method) const {
  CHECK(dex_files_open_attempted_)
      << "Dex files were not successfully opened before the call to " << calling_method
      << "attempt=" << dex_files_open_attempted_ << ", result=" << dex_files_open_result_;
}

// Collects the dex files from the give Java dex_file object. Only the dex files with
// at least 1 class are collected. If a null java_dex_file is passed this method does nothing.
static bool CollectDexFilesFromJavaDexFile(ObjPtr<mirror::Object> java_dex_file,
                                           ArtField* const cookie_field,
                                           std::vector<const DexFile*>* out_dex_files)
      REQUIRES_SHARED(Locks::mutator_lock_) {
  if (java_dex_file == nullptr) {
    return true;
  }
  // On the Java side, the dex files are stored in the cookie field.
  mirror::LongArray* long_array = cookie_field->GetObject(java_dex_file)->AsLongArray();
  if (long_array == nullptr) {
    // This should never happen so log a warning.
    LOG(ERROR) << "Unexpected null cookie";
    return false;
  }
  int32_t long_array_size = long_array->GetLength();
  // Index 0 from the long array stores the oat file. The dex files start at index 1.
  for (int32_t j = 1; j < long_array_size; ++j) {
    const DexFile* cp_dex_file =
        reinterpret_cast64<const DexFile*>(long_array->GetWithoutChecks(j));
    if (cp_dex_file != nullptr && cp_dex_file->NumClassDefs() > 0) {
      // TODO(calin): It's unclear why the dex files with no classes are skipped here and when
      // cp_dex_file can be null.
      out_dex_files->push_back(cp_dex_file);
    }
  }
  return true;
}

// Collects all the dex files loaded by the given class loader.
// Returns true for success or false if an unexpected state is discovered (e.g. a null dex cookie,
// a null list of dex elements or a null dex element).
static bool CollectDexFilesFromSupportedClassLoader(ScopedObjectAccessAlreadyRunnable& soa,
                                                    Handle<mirror::ClassLoader> class_loader,
                                                    std::vector<const DexFile*>* out_dex_files)
      REQUIRES_SHARED(Locks::mutator_lock_) {
  CHECK(IsPathOrDexClassLoader(soa, class_loader) || IsDelegateLastClassLoader(soa, class_loader));

  // All supported class loaders inherit from BaseDexClassLoader.
  // We need to get the DexPathList and loop through it.
  ArtField* const cookie_field =
      jni::DecodeArtField(WellKnownClasses::dalvik_system_DexFile_cookie);
  ArtField* const dex_file_field =
      jni::DecodeArtField(WellKnownClasses::dalvik_system_DexPathList__Element_dexFile);
  ObjPtr<mirror::Object> dex_path_list =
      jni::DecodeArtField(WellKnownClasses::dalvik_system_BaseDexClassLoader_pathList)->
          GetObject(class_loader.Get());
  CHECK(cookie_field != nullptr);
  CHECK(dex_file_field != nullptr);
  if (dex_path_list == nullptr) {
    // This may be null if the current class loader is under construction and it does not
    // have its fields setup yet.
    return true;
  }
  // DexPathList has an array dexElements of Elements[] which each contain a dex file.
  ObjPtr<mirror::Object> dex_elements_obj =
      jni::DecodeArtField(WellKnownClasses::dalvik_system_DexPathList_dexElements)->
          GetObject(dex_path_list);
  // Loop through each dalvik.system.DexPathList$Element's dalvik.system.DexFile and look
  // at the mCookie which is a DexFile vector.
  if (dex_elements_obj == nullptr) {
    // TODO(calin): It's unclear if we should just assert here. For now be prepared for the worse
    // and assume we have no elements.
    return true;
  } else {
    StackHandleScope<1> hs(soa.Self());
    Handle<mirror::ObjectArray<mirror::Object>> dex_elements(
        hs.NewHandle(dex_elements_obj->AsObjectArray<mirror::Object>()));
    for (int32_t i = 0; i < dex_elements->GetLength(); ++i) {
      mirror::Object* element = dex_elements->GetWithoutChecks(i);
      if (element == nullptr) {
        // Should never happen, log an error and break.
        // TODO(calin): It's unclear if we should just assert here.
        // This code was propagated to oat_file_manager from the class linker where it would
        // throw a NPE. For now, return false which will mark this class loader as unsupported.
        LOG(ERROR) << "Unexpected null in the dex element list";
        return false;
      }
      ObjPtr<mirror::Object> dex_file = dex_file_field->GetObject(element);
      if (!CollectDexFilesFromJavaDexFile(dex_file, cookie_field, out_dex_files)) {
        return false;
      }
    }
  }

  return true;
}

static bool GetDexFilesFromDexElementsArray(
    ScopedObjectAccessAlreadyRunnable& soa,
    Handle<mirror::ObjectArray<mirror::Object>> dex_elements,
    std::vector<const DexFile*>* out_dex_files) REQUIRES_SHARED(Locks::mutator_lock_) {
  DCHECK(dex_elements != nullptr);

  ArtField* const cookie_field =
      jni::DecodeArtField(WellKnownClasses::dalvik_system_DexFile_cookie);
  ArtField* const dex_file_field =
      jni::DecodeArtField(WellKnownClasses::dalvik_system_DexPathList__Element_dexFile);
  ObjPtr<mirror::Class> const element_class = soa.Decode<mirror::Class>(
      WellKnownClasses::dalvik_system_DexPathList__Element);
  ObjPtr<mirror::Class> const dexfile_class = soa.Decode<mirror::Class>(
      WellKnownClasses::dalvik_system_DexFile);

  for (int32_t i = 0; i < dex_elements->GetLength(); ++i) {
    mirror::Object* element = dex_elements->GetWithoutChecks(i);
    // We can hit a null element here because this is invoked with a partially filled dex_elements
    // array from DexPathList. DexPathList will open each dex sequentially, each time passing the
    // list of dex files which were opened before.
    if (element == nullptr) {
      continue;
    }

    // We support this being dalvik.system.DexPathList$Element and dalvik.system.DexFile.
    // TODO(calin): Code caried over oat_file_manager: supporting both classes seem to be
    // a historical glitch. All the java code opens dex files using an array of Elements.
    ObjPtr<mirror::Object> dex_file;
    if (element_class == element->GetClass()) {
      dex_file = dex_file_field->GetObject(element);
    } else if (dexfile_class == element->GetClass()) {
      dex_file = element;
    } else {
      LOG(ERROR) << "Unsupported element in dex_elements: "
                 << mirror::Class::PrettyClass(element->GetClass());
      return false;
    }

    if (!CollectDexFilesFromJavaDexFile(dex_file, cookie_field, out_dex_files)) {
      return false;
    }
  }
  return true;
}

// Adds the `class_loader` info to the `context`.
// The dex file present in `dex_elements` array (if not null) will be added at the end of
// the classpath.
// This method is recursive (w.r.t. the class loader parent) and will stop once it reaches the
// BootClassLoader. Note that the class loader chain is expected to be short.
bool ClassLoaderContext::CreateInfoFromClassLoader(
      ScopedObjectAccessAlreadyRunnable& soa,
      Handle<mirror::ClassLoader> class_loader,
      Handle<mirror::ObjectArray<mirror::Object>> dex_elements,
      std::unique_ptr<ClassLoaderInfo>* return_info)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  if (ClassLinker::IsBootClassLoader(soa, class_loader.Get())) {
    // Nothing to do for the boot class loader as we don't add its dex files to the context.
    return true;
  }

  ClassLoaderContext::ClassLoaderType type;
  if (IsPathOrDexClassLoader(soa, class_loader)) {
    type = kPathClassLoader;
  } else if (IsDelegateLastClassLoader(soa, class_loader)) {
    type = kDelegateLastClassLoader;
  } else {
    LOG(WARNING) << "Unsupported class loader";
    return false;
  }

  // Inspect the class loader for its dex files.
  std::vector<const DexFile*> dex_files_loaded;
  CollectDexFilesFromSupportedClassLoader(soa, class_loader, &dex_files_loaded);

  // If we have a dex_elements array extract its dex elements now.
  // This is used in two situations:
  //   1) when a new ClassLoader is created DexPathList will open each dex file sequentially
  //      passing the list of already open dex files each time. This ensures that we see the
  //      correct context even if the ClassLoader under construction is not fully build.
  //   2) when apk splits are loaded on the fly, the framework will load their dex files by
  //      appending them to the current class loader. When the new code paths are loaded in
  //      BaseDexClassLoader, the paths already present in the class loader will be passed
  //      in the dex_elements array.
  if (dex_elements != nullptr) {
    GetDexFilesFromDexElementsArray(soa, dex_elements, &dex_files_loaded);
  }

  std::unique_ptr<ClassLoaderInfo> info(new ClassLoaderContext::ClassLoaderInfo(type));
  for (const DexFile* dex_file : dex_files_loaded) {
    info->classpath.push_back(dex_file->GetLocation());
    info->checksums.push_back(dex_file->GetLocationChecksum());
    info->opened_dex_files.emplace_back(dex_file);
  }

  // Note that dex_elements array is null here. The elements are considered to be part of the
  // current class loader and are not passed to the parents.
  ScopedNullHandle<mirror::ObjectArray<mirror::Object>> null_dex_elements;

  // Add the shared libraries.
  StackHandleScope<3> hs(Thread::Current());
  ArtField* field =
      jni::DecodeArtField(WellKnownClasses::dalvik_system_BaseDexClassLoader_sharedLibraryLoaders);
  ObjPtr<mirror::Object> raw_shared_libraries = field->GetObject(class_loader.Get());
  if (raw_shared_libraries != nullptr) {
    Handle<mirror::ObjectArray<mirror::ClassLoader>> shared_libraries =
        hs.NewHandle(raw_shared_libraries->AsObjectArray<mirror::ClassLoader>());
    MutableHandle<mirror::ClassLoader> temp_loader = hs.NewHandle<mirror::ClassLoader>(nullptr);
    for (int32_t i = 0; i < shared_libraries->GetLength(); ++i) {
      temp_loader.Assign(shared_libraries->Get(i));
      std::unique_ptr<ClassLoaderInfo> result;
      if (!CreateInfoFromClassLoader(soa, temp_loader, null_dex_elements, &result)) {
        return false;
      }
      info->shared_libraries.push_back(std::move(result));
    }
  }

  // We created the ClassLoaderInfo for the current loader. Move on to its parent.
  Handle<mirror::ClassLoader> parent = hs.NewHandle(class_loader->GetParent());
  std::unique_ptr<ClassLoaderInfo> parent_info;
  if (!CreateInfoFromClassLoader(soa, parent, null_dex_elements, &parent_info)) {
    return false;
  }
  info->parent = std::move(parent_info);
  *return_info = std::move(info);
  return true;
}

std::unique_ptr<ClassLoaderContext> ClassLoaderContext::CreateContextForClassLoader(
    jobject class_loader,
    jobjectArray dex_elements) {
  CHECK(class_loader != nullptr);

  ScopedObjectAccess soa(Thread::Current());
  StackHandleScope<2> hs(soa.Self());
  Handle<mirror::ClassLoader> h_class_loader =
      hs.NewHandle(soa.Decode<mirror::ClassLoader>(class_loader));
  Handle<mirror::ObjectArray<mirror::Object>> h_dex_elements =
      hs.NewHandle(soa.Decode<mirror::ObjectArray<mirror::Object>>(dex_elements));

  std::unique_ptr<ClassLoaderInfo> info;
  if (!CreateInfoFromClassLoader(soa, h_class_loader, h_dex_elements, &info)) {
    return nullptr;
  }
  std::unique_ptr<ClassLoaderContext> result(new ClassLoaderContext(/*owns_the_dex_files=*/ false));
  result->class_loader_chain_ = std::move(info);
  return result;
}

static bool IsAbsoluteLocation(const std::string& location) {
  return !location.empty() && location[0] == '/';
}

ClassLoaderContext::VerificationResult ClassLoaderContext::VerifyClassLoaderContextMatch(
    const std::string& context_spec,
    bool verify_names,
    bool verify_checksums) const {
  if (verify_names || verify_checksums) {
    DCHECK(dex_files_open_attempted_);
    DCHECK(dex_files_open_result_);
  }

  ClassLoaderContext expected_context;
  if (!expected_context.Parse(context_spec, verify_checksums)) {
    LOG(WARNING) << "Invalid class loader context: " << context_spec;
    return VerificationResult::kMismatch;
  }

  // Special shared library contexts always match. They essentially instruct the runtime
  // to ignore the class path check because the oat file is known to be loaded in different
  // contexts. OatFileManager will further verify if the oat file can be loaded based on the
  // collision check.
  if (expected_context.special_shared_library_) {
    // Special case where we are the only entry in the class path.
    if (class_loader_chain_ != nullptr &&
        class_loader_chain_->parent == nullptr &&
        class_loader_chain_->classpath.size() == 0) {
      return VerificationResult::kVerifies;
    }
    return VerificationResult::kForcedToSkipChecks;
  } else if (special_shared_library_) {
    return VerificationResult::kForcedToSkipChecks;
  }

  ClassLoaderInfo* info = class_loader_chain_.get();
  ClassLoaderInfo* expected = expected_context.class_loader_chain_.get();
  CHECK(info != nullptr);
  CHECK(expected != nullptr);
  if (!ClassLoaderInfoMatch(*info, *expected, context_spec, verify_names, verify_checksums)) {
    return VerificationResult::kMismatch;
  }
  return VerificationResult::kVerifies;
}

bool ClassLoaderContext::ClassLoaderInfoMatch(
    const ClassLoaderInfo& info,
    const ClassLoaderInfo& expected_info,
    const std::string& context_spec,
    bool verify_names,
    bool verify_checksums) const {
  if (info.type != expected_info.type) {
    LOG(WARNING) << "ClassLoaderContext type mismatch"
        << ". expected=" << GetClassLoaderTypeName(expected_info.type)
        << ", found=" << GetClassLoaderTypeName(info.type)
        << " (" << context_spec << " | " << EncodeContextForOatFile("") << ")";
    return false;
  }
  if (info.classpath.size() != expected_info.classpath.size()) {
    LOG(WARNING) << "ClassLoaderContext classpath size mismatch"
          << ". expected=" << expected_info.classpath.size()
          << ", found=" << info.classpath.size()
          << " (" << context_spec << " | " << EncodeContextForOatFile("") << ")";
    return false;
  }

  if (verify_checksums) {
    DCHECK_EQ(info.classpath.size(), info.checksums.size());
    DCHECK_EQ(expected_info.classpath.size(), expected_info.checksums.size());
  }

  if (verify_names) {
    for (size_t k = 0; k < info.classpath.size(); k++) {
      // Compute the dex location that must be compared.
      // We shouldn't do a naive comparison `info.classpath[k] == expected_info.classpath[k]`
      // because even if they refer to the same file, one could be encoded as a relative location
      // and the other as an absolute one.
      bool is_dex_name_absolute = IsAbsoluteLocation(info.classpath[k]);
      bool is_expected_dex_name_absolute = IsAbsoluteLocation(expected_info.classpath[k]);
      std::string dex_name;
      std::string expected_dex_name;

      if (is_dex_name_absolute == is_expected_dex_name_absolute) {
        // If both locations are absolute or relative then compare them as they are.
        // This is usually the case for: shared libraries and secondary dex files.
        dex_name = info.classpath[k];
        expected_dex_name = expected_info.classpath[k];
      } else if (is_dex_name_absolute) {
        // The runtime name is absolute but the compiled name (the expected one) is relative.
        // This is the case for split apks which depend on base or on other splits.
        dex_name = info.classpath[k];
        expected_dex_name = OatFile::ResolveRelativeEncodedDexLocation(
            info.classpath[k].c_str(), expected_info.classpath[k]);
      } else if (is_expected_dex_name_absolute) {
        // The runtime name is relative but the compiled name is absolute.
        // There is no expected use case that would end up here as dex files are always loaded
        // with their absolute location. However, be tolerant and do the best effort (in case
        // there are unexpected new use case...).
        dex_name = OatFile::ResolveRelativeEncodedDexLocation(
            expected_info.classpath[k].c_str(), info.classpath[k]);
        expected_dex_name = expected_info.classpath[k];
      } else {
        // Both locations are relative. In this case there's not much we can be sure about
        // except that the names are the same. The checksum will ensure that the files are
        // are same. This should not happen outside testing and manual invocations.
        dex_name = info.classpath[k];
        expected_dex_name = expected_info.classpath[k];
      }

      // Compare the locations.
      if (dex_name != expected_dex_name) {
        LOG(WARNING) << "ClassLoaderContext classpath element mismatch"
            << ". expected=" << expected_info.classpath[k]
            << ", found=" << info.classpath[k]
            << " (" << context_spec << " | " << EncodeContextForOatFile("") << ")";
        return false;
      }

      // Compare the checksums.
      if (info.checksums[k] != expected_info.checksums[k]) {
        LOG(WARNING) << "ClassLoaderContext classpath element checksum mismatch"
                     << ". expected=" << expected_info.checksums[k]
                     << ", found=" << info.checksums[k]
                     << " (" << context_spec << " | " << EncodeContextForOatFile("") << ")";
        return false;
      }
    }
  }

  if (info.shared_libraries.size() != expected_info.shared_libraries.size()) {
    LOG(WARNING) << "ClassLoaderContext shared library size mismatch. "
          << "Expected=" << expected_info.shared_libraries.size()
          << ", found=" << info.shared_libraries.size()
          << " (" << context_spec << " | " << EncodeContextForOatFile("") << ")";
    return false;
  }
  for (size_t i = 0; i < info.shared_libraries.size(); ++i) {
    if (!ClassLoaderInfoMatch(*info.shared_libraries[i].get(),
                              *expected_info.shared_libraries[i].get(),
                              context_spec,
                              verify_names,
                              verify_checksums)) {
      return false;
    }
  }
  if (info.parent.get() == nullptr) {
    if (expected_info.parent.get() != nullptr) {
      LOG(WARNING) << "ClassLoaderContext parent mismatch. "
            << " (" << context_spec << " | " << EncodeContextForOatFile("") << ")";
      return false;
    }
    return true;
  } else if (expected_info.parent.get() == nullptr) {
    LOG(WARNING) << "ClassLoaderContext parent mismatch. "
          << " (" << context_spec << " | " << EncodeContextForOatFile("") << ")";
    return false;
  } else {
    return ClassLoaderInfoMatch(*info.parent.get(),
                                *expected_info.parent.get(),
                                context_spec,
                                verify_names,
                                verify_checksums);
  }
}

}  // namespace art
