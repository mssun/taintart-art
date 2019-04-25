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

#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <string_view>

#include "android-base/stringprintf.h"
#include "android-base/strings.h"

#include "base/bit_utils.h"
#include "base/hiddenapi_flags.h"
#include "base/mem_map.h"
#include "base/os.h"
#include "base/stl_util.h"
#include "base/string_view_cpp20.h"
#include "base/unix_file/fd_file.h"
#include "dex/art_dex_file_loader.h"
#include "dex/class_accessor-inl.h"
#include "dex/dex_file-inl.h"

namespace art {
namespace hiddenapi {

const char kErrorHelp[] = "\nSee go/hiddenapi-error for help.";

static int original_argc;
static char** original_argv;

static std::string CommandLine() {
  std::vector<std::string> command;
  command.reserve(original_argc);
  for (int i = 0; i < original_argc; ++i) {
    command.push_back(original_argv[i]);
  }
  return android::base::Join(command, ' ');
}

static void UsageErrorV(const char* fmt, va_list ap) {
  std::string error;
  android::base::StringAppendV(&error, fmt, ap);
  LOG(ERROR) << error;
}

static void UsageError(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  UsageErrorV(fmt, ap);
  va_end(ap);
}

NO_RETURN static void Usage(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  UsageErrorV(fmt, ap);
  va_end(ap);

  UsageError("Command: %s", CommandLine().c_str());
  UsageError("Usage: hiddenapi [command_name] [options]...");
  UsageError("");
  UsageError("  Command \"encode\": encode API list membership in boot dex files");
  UsageError("    --input-dex=<filename>: dex file which belongs to boot class path");
  UsageError("    --output-dex=<filename>: file to write encoded dex into");
  UsageError("        input and output dex files are paired in order of appearance");
  UsageError("");
  UsageError("    --api-flags=<filename>:");
  UsageError("        CSV file with signatures of methods/fields and their respective flags");
  UsageError("");
  UsageError("    --no-force-assign-all:");
  UsageError("        Disable check that all dex entries have been assigned a flag");
  UsageError("");
  UsageError("  Command \"list\": dump lists of public and private API");
  UsageError("    --boot-dex=<filename>: dex file which belongs to boot class path");
  UsageError("    --public-stub-classpath=<filenames>:");
  UsageError("    --system-stub-classpath=<filenames>:");
  UsageError("    --test-stub-classpath=<filenames>:");
  UsageError("    --core-platform-stub-classpath=<filenames>:");
  UsageError("        colon-separated list of dex/apk files which form API stubs of boot");
  UsageError("        classpath. Multiple classpaths can be specified");
  UsageError("");
  UsageError("    --out-api-flags=<filename>: output file for a CSV file with API flags");
  UsageError("");

  exit(EXIT_FAILURE);
}

template<typename E>
static bool Contains(const std::vector<E>& vec, const E& elem) {
  return std::find(vec.begin(), vec.end(), elem) != vec.end();
}

class DexClass : public ClassAccessor {
 public:
  explicit DexClass(const ClassAccessor& accessor) : ClassAccessor(accessor) {}

  const uint8_t* GetData() const { return dex_file_.GetClassData(GetClassDef()); }

  const dex::TypeIndex GetSuperclassIndex() const { return GetClassDef().superclass_idx_; }

  bool HasSuperclass() const { return dex_file_.IsTypeIndexValid(GetSuperclassIndex()); }

  std::string_view GetSuperclassDescriptor() const {
    return HasSuperclass() ? dex_file_.StringByTypeIdx(GetSuperclassIndex()) : "";
  }

  std::set<std::string_view> GetInterfaceDescriptors() const {
    std::set<std::string_view> list;
    const dex::TypeList* ifaces = dex_file_.GetInterfacesList(GetClassDef());
    for (uint32_t i = 0; ifaces != nullptr && i < ifaces->Size(); ++i) {
      list.insert(dex_file_.StringByTypeIdx(ifaces->GetTypeItem(i).type_idx_));
    }
    return list;
  }

  inline bool IsPublic() const { return HasAccessFlags(kAccPublic); }
  inline bool IsInterface() const { return HasAccessFlags(kAccInterface); }

  inline bool Equals(const DexClass& other) const {
    bool equals = strcmp(GetDescriptor(), other.GetDescriptor()) == 0;

    if (equals) {
      LOG(FATAL) << "Class duplication: " << GetDescriptor() << " in " << dex_file_.GetLocation()
          << " and " << other.dex_file_.GetLocation();
    }

    return equals;
  }

 private:
  uint32_t GetAccessFlags() const { return GetClassDef().access_flags_; }
  bool HasAccessFlags(uint32_t mask) const { return (GetAccessFlags() & mask) == mask; }

  static std::string JoinStringSet(const std::set<std::string_view>& s) {
    return "{" + ::android::base::Join(std::vector<std::string>(s.begin(), s.end()), ",") + "}";
  }
};

class DexMember {
 public:
  DexMember(const DexClass& klass, const ClassAccessor::Field& item)
      : klass_(klass), item_(item), is_method_(false) {
    DCHECK_EQ(GetFieldId().class_idx_, klass.GetClassIdx());
  }

  DexMember(const DexClass& klass, const ClassAccessor::Method& item)
      : klass_(klass), item_(item), is_method_(true) {
    DCHECK_EQ(GetMethodId().class_idx_, klass.GetClassIdx());
  }

  inline const DexClass& GetDeclaringClass() const { return klass_; }

  inline bool IsMethod() const { return is_method_; }
  inline bool IsVirtualMethod() const { return IsMethod() && !GetMethod().IsStaticOrDirect(); }
  inline bool IsConstructor() const { return IsMethod() && HasAccessFlags(kAccConstructor); }

  inline bool IsPublicOrProtected() const {
    return HasAccessFlags(kAccPublic) || HasAccessFlags(kAccProtected);
  }

  // Constructs a string with a unique signature of this class member.
  std::string GetApiEntry() const {
    std::stringstream ss;
    ss << klass_.GetDescriptor() << "->" << GetName() << (IsMethod() ? "" : ":")
       << GetSignature();
    return ss.str();
  }

  inline bool operator==(const DexMember& other) const {
    // These need to match if they should resolve to one another.
    bool equals = IsMethod() == other.IsMethod() &&
                  GetName() == other.GetName() &&
                  GetSignature() == other.GetSignature();

    // Sanity checks if they do match.
    if (equals) {
      CHECK_EQ(IsVirtualMethod(), other.IsVirtualMethod());
    }

    return equals;
  }

 private:
  inline uint32_t GetAccessFlags() const { return item_.GetAccessFlags(); }
  inline bool HasAccessFlags(uint32_t mask) const { return (GetAccessFlags() & mask) == mask; }

  inline std::string_view GetName() const {
    return IsMethod() ? item_.GetDexFile().GetMethodName(GetMethodId())
                      : item_.GetDexFile().GetFieldName(GetFieldId());
  }

  inline std::string GetSignature() const {
    return IsMethod() ? item_.GetDexFile().GetMethodSignature(GetMethodId()).ToString()
                      : item_.GetDexFile().GetFieldTypeDescriptor(GetFieldId());
  }

  inline const ClassAccessor::Method& GetMethod() const {
    DCHECK(IsMethod());
    return down_cast<const ClassAccessor::Method&>(item_);
  }

  inline const dex::MethodId& GetMethodId() const {
    DCHECK(IsMethod());
    return item_.GetDexFile().GetMethodId(item_.GetIndex());
  }

  inline const dex::FieldId& GetFieldId() const {
    DCHECK(!IsMethod());
    return item_.GetDexFile().GetFieldId(item_.GetIndex());
  }

  const DexClass& klass_;
  const ClassAccessor::BaseItem& item_;
  const bool is_method_;
};

class ClassPath final {
 public:
  ClassPath(const std::vector<std::string>& dex_paths, bool open_writable) {
    OpenDexFiles(dex_paths, open_writable);
  }

  template<typename Fn>
  void ForEachDexClass(Fn fn) {
    for (auto& dex_file : dex_files_) {
      for (ClassAccessor accessor : dex_file->GetClasses()) {
        fn(DexClass(accessor));
      }
    }
  }

  template<typename Fn>
  void ForEachDexMember(Fn fn) {
    ForEachDexClass([&fn](const DexClass& klass) {
      for (const ClassAccessor::Field& field : klass.GetFields()) {
        fn(DexMember(klass, field));
      }
      for (const ClassAccessor::Method& method : klass.GetMethods()) {
        fn(DexMember(klass, method));
      }
    });
  }

  std::vector<const DexFile*> GetDexFiles() const {
    return MakeNonOwningPointerVector(dex_files_);
  }

  void UpdateDexChecksums() {
    for (auto& dex_file : dex_files_) {
      // Obtain a writeable pointer to the dex header.
      DexFile::Header* header = const_cast<DexFile::Header*>(&dex_file->GetHeader());
      // Recalculate checksum and overwrite the value in the header.
      header->checksum_ = dex_file->CalculateChecksum();
    }
  }

 private:
  void OpenDexFiles(const std::vector<std::string>& dex_paths, bool open_writable) {
    ArtDexFileLoader dex_loader;
    std::string error_msg;

    if (open_writable) {
      for (const std::string& filename : dex_paths) {
        File fd(filename.c_str(), O_RDWR, /* check_usage= */ false);
        CHECK_NE(fd.Fd(), -1) << "Unable to open file '" << filename << "': " << strerror(errno);

        // Memory-map the dex file with MAP_SHARED flag so that changes in memory
        // propagate to the underlying file. We run dex file verification as if
        // the dex file was not in boot claass path to check basic assumptions,
        // such as that at most one of public/private/protected flag is set.
        // We do those checks here and skip them when loading the processed file
        // into boot class path.
        std::unique_ptr<const DexFile> dex_file(dex_loader.OpenDex(fd.Release(),
                                                                   /* location= */ filename,
                                                                   /* verify= */ true,
                                                                   /* verify_checksum= */ true,
                                                                   /* mmap_shared= */ true,
                                                                   &error_msg));
        CHECK(dex_file.get() != nullptr) << "Open failed for '" << filename << "' " << error_msg;
        CHECK(dex_file->IsStandardDexFile()) << "Expected a standard dex file '" << filename << "'";
        CHECK(dex_file->EnableWrite())
            << "Failed to enable write permission for '" << filename << "'";
        dex_files_.push_back(std::move(dex_file));
      }
    } else {
      for (const std::string& filename : dex_paths) {
        bool success = dex_loader.Open(filename.c_str(),
                                       /* location= */ filename,
                                       /* verify= */ true,
                                       /* verify_checksum= */ true,
                                       &error_msg,
                                       &dex_files_);
        CHECK(success) << "Open failed for '" << filename << "' " << error_msg;
      }
    }
  }

  // Opened dex files. Note that these are opened as `const` but may be written into.
  std::vector<std::unique_ptr<const DexFile>> dex_files_;
};

class HierarchyClass final {
 public:
  HierarchyClass() {}

  void AddDexClass(const DexClass& klass) {
    CHECK(dex_classes_.empty() || klass.Equals(dex_classes_.front()));
    dex_classes_.push_back(klass);
  }

  void AddExtends(HierarchyClass& parent) {
    CHECK(!Contains(extends_, &parent));
    CHECK(!Contains(parent.extended_by_, this));
    extends_.push_back(&parent);
    parent.extended_by_.push_back(this);
  }

  const DexClass& GetOneDexClass() const {
    CHECK(!dex_classes_.empty());
    return dex_classes_.front();
  }

  // See comment on Hierarchy::ForEachResolvableMember.
  template<typename Fn>
  bool ForEachResolvableMember(const DexMember& other, Fn fn) {
    std::vector<HierarchyClass*> visited;
    return ForEachResolvableMember_Impl(other, fn, true, true, visited);
  }

  // Returns true if this class contains at least one member matching `other`.
  bool HasMatchingMember(const DexMember& other) {
    return ForEachMatchingMember(other, [](const DexMember&) { return true; });
  }

  // Recursively iterates over all subclasses of this class and invokes `fn`
  // on each one. If `fn` returns false for a particular subclass, exploring its
  // subclasses is skipped.
  template<typename Fn>
  void ForEachSubClass(Fn fn) {
    for (HierarchyClass* subclass : extended_by_) {
      if (fn(subclass)) {
        subclass->ForEachSubClass(fn);
      }
    }
  }

 private:
  template<typename Fn>
  bool ForEachResolvableMember_Impl(const DexMember& other,
                                    Fn fn,
                                    bool allow_explore_up,
                                    bool allow_explore_down,
                                    std::vector<HierarchyClass*> visited) {
    if (std::find(visited.begin(), visited.end(), this) == visited.end()) {
      visited.push_back(this);
    } else {
      return false;
    }

    // First try to find a member matching `other` in this class.
    bool found = ForEachMatchingMember(other, fn);

    // If not found, see if it is inherited from parents. Note that this will not
    // revisit parents already in `visited`.
    if (!found && allow_explore_up) {
      for (HierarchyClass* superclass : extends_) {
        found |= superclass->ForEachResolvableMember_Impl(
            other,
            fn,
            /* allow_explore_up */ true,
            /* allow_explore_down */ false,
            visited);
      }
    }

    // If this is a virtual method, continue exploring into subclasses so as to visit
    // all overriding methods. Allow subclasses to explore their superclasses if this
    // is an interface. This is needed to find implementations of this interface's
    // methods inherited from superclasses (b/122551864).
    if (allow_explore_down && other.IsVirtualMethod()) {
      for (HierarchyClass* subclass : extended_by_) {
        subclass->ForEachResolvableMember_Impl(
            other,
            fn,
            /* allow_explore_up */ GetOneDexClass().IsInterface(),
            /* allow_explore_down */ true,
            visited);
      }
    }

    return found;
  }

  template<typename Fn>
  bool ForEachMatchingMember(const DexMember& other, Fn fn) {
    bool found = false;
    auto compare_member = [&](const DexMember& member) {
      // TODO(dbrazdil): Check whether class of `other` can access `member`.
      if (member == other) {
        found = true;
        fn(member);
      }
    };
    for (const DexClass& dex_class : dex_classes_) {
      for (const ClassAccessor::Field& field : dex_class.GetFields()) {
        compare_member(DexMember(dex_class, field));
      }
      for (const ClassAccessor::Method& method : dex_class.GetMethods()) {
        compare_member(DexMember(dex_class, method));
      }
    }
    return found;
  }

  // DexClass entries of this class found across all the provided dex files.
  std::vector<DexClass> dex_classes_;

  // Classes which this class inherits, or interfaces which it implements.
  std::vector<HierarchyClass*> extends_;

  // Classes which inherit from this class.
  std::vector<HierarchyClass*> extended_by_;
};

class Hierarchy final {
 public:
  explicit Hierarchy(ClassPath& classpath) : classpath_(classpath) {
    BuildClassHierarchy();
  }

  // Perform an operation for each member of the hierarchy which could potentially
  // be the result of method/field resolution of `other`.
  // The function `fn` should accept a DexMember reference and return true if
  // the member was changed. This drives a performance optimization which only
  // visits overriding members the first time the overridden member is visited.
  // Returns true if at least one resolvable member was found.
  template<typename Fn>
  bool ForEachResolvableMember(const DexMember& other, Fn fn) {
    HierarchyClass* klass = FindClass(other.GetDeclaringClass().GetDescriptor());
    return (klass != nullptr) && klass->ForEachResolvableMember(other, fn);
  }

  // Returns true if `member`, which belongs to this classpath, is visible to
  // code in child class loaders.
  bool IsMemberVisible(const DexMember& member) {
    if (!member.IsPublicOrProtected()) {
      // Member is private or package-private. Cannot be visible.
      return false;
    } else if (member.GetDeclaringClass().IsPublic()) {
      // Member is public or protected, and class is public. It must be visible.
      return true;
    } else if (member.IsConstructor()) {
      // Member is public or protected constructor and class is not public.
      // Must be hidden because it cannot be implicitly exposed by a subclass.
      return false;
    } else {
      // Member is public or protected method, but class is not public. Check if
      // it is exposed through a public subclass.
      // Example code (`foo` exposed by ClassB):
      //   class ClassA { public void foo() { ... } }
      //   public class ClassB extends ClassA {}
      HierarchyClass* klass = FindClass(member.GetDeclaringClass().GetDescriptor());
      CHECK(klass != nullptr);
      bool visible = false;
      klass->ForEachSubClass([&visible, &member](HierarchyClass* subclass) {
        if (subclass->HasMatchingMember(member)) {
          // There is a member which matches `member` in `subclass`, either
          // a virtual method overriding `member` or a field overshadowing
          // `member`. In either case, `member` remains hidden.
          CHECK(member.IsVirtualMethod() || !member.IsMethod());
          return false;  // do not explore deeper
        } else if (subclass->GetOneDexClass().IsPublic()) {
          // `subclass` inherits and exposes `member`.
          visible = true;
          return false;  // do not explore deeper
        } else {
          // `subclass` inherits `member` but does not expose it.
          return true;   // explore deeper
        }
      });
      return visible;
    }
  }

 private:
  HierarchyClass* FindClass(const std::string_view& descriptor) {
    auto it = classes_.find(descriptor);
    if (it == classes_.end()) {
      return nullptr;
    } else {
      return &it->second;
    }
  }

  void BuildClassHierarchy() {
    // Create one HierarchyClass entry in `classes_` per class descriptor
    // and add all DexClass objects with the same descriptor to that entry.
    classpath_.ForEachDexClass([this](const DexClass& klass) {
      classes_[klass.GetDescriptor()].AddDexClass(klass);
    });

    // Connect each HierarchyClass to its successors and predecessors.
    for (auto& entry : classes_) {
      HierarchyClass& klass = entry.second;
      const DexClass& dex_klass = klass.GetOneDexClass();

      if (!dex_klass.HasSuperclass()) {
        CHECK(dex_klass.GetInterfaceDescriptors().empty())
            << "java/lang/Object should not implement any interfaces";
        continue;
      }

      HierarchyClass* superclass = FindClass(dex_klass.GetSuperclassDescriptor());
      CHECK(superclass != nullptr)
          << "Superclass " << dex_klass.GetSuperclassDescriptor()
          << " of class " << dex_klass.GetDescriptor() << " from dex file \""
          << dex_klass.GetDexFile().GetLocation() << "\" was not found. "
          << "Either the superclass is missing or it appears later in the classpath spec.";
      klass.AddExtends(*superclass);

      for (const std::string_view& iface_desc : dex_klass.GetInterfaceDescriptors()) {
        HierarchyClass* iface = FindClass(iface_desc);
        CHECK(iface != nullptr);
        klass.AddExtends(*iface);
      }
    }
  }

  ClassPath& classpath_;
  std::map<std::string_view, HierarchyClass> classes_;
};

// Builder of dex section containing hiddenapi flags.
class HiddenapiClassDataBuilder final {
 public:
  explicit HiddenapiClassDataBuilder(const DexFile& dex_file)
      : num_classdefs_(dex_file.NumClassDefs()),
        next_class_def_idx_(0u),
        class_def_has_non_zero_flags_(false),
        dex_file_has_non_zero_flags_(false),
        data_(sizeof(uint32_t) * (num_classdefs_ + 1), 0u) {
    *GetSizeField() = GetCurrentDataSize();
  }

  // Notify the builder that new flags for the next class def
  // will be written now. The builder records the current offset
  // into the header.
  void BeginClassDef(uint32_t idx) {
    CHECK_EQ(next_class_def_idx_, idx);
    CHECK_LT(idx, num_classdefs_);
    GetOffsetArray()[idx] = GetCurrentDataSize();
    class_def_has_non_zero_flags_ = false;
  }

  // Notify the builder that all flags for this class def have been
  // written. The builder updates the total size of the data struct
  // and may set offset for class def in header to zero if no data
  // has been written.
  void EndClassDef(uint32_t idx) {
    CHECK_EQ(next_class_def_idx_, idx);
    CHECK_LT(idx, num_classdefs_);

    ++next_class_def_idx_;

    if (!class_def_has_non_zero_flags_) {
      // No need to store flags for this class. Remove the written flags
      // and set offset in header to zero.
      data_.resize(GetOffsetArray()[idx]);
      GetOffsetArray()[idx] = 0u;
    }

    dex_file_has_non_zero_flags_ |= class_def_has_non_zero_flags_;

    if (idx == num_classdefs_ - 1) {
      if (dex_file_has_non_zero_flags_) {
        // This was the last class def and we have generated non-zero hiddenapi
        // flags. Update total size in the header.
        *GetSizeField() = GetCurrentDataSize();
      } else {
        // This was the last class def and we have not generated any non-zero
        // hiddenapi flags. Clear all the data.
        data_.clear();
      }
    }
  }

  // Append flags at the end of the data struct. This should be called
  // between BeginClassDef and EndClassDef in the order of appearance of
  // fields/methods in the class data stream.
  void WriteFlags(const ApiList& flags) {
    uint32_t dex_flags = flags.GetDexFlags();
    EncodeUnsignedLeb128(&data_, dex_flags);
    class_def_has_non_zero_flags_ |= (dex_flags != 0u);
  }

  // Return backing data, assuming that all flags have been written.
  const std::vector<uint8_t>& GetData() const {
    CHECK_EQ(next_class_def_idx_, num_classdefs_) << "Incomplete data";
    return data_;
  }

 private:
  // Returns pointer to the size field in the header of this dex section.
  uint32_t* GetSizeField() {
    // Assume malloc() aligns allocated memory to at least uint32_t.
    CHECK(IsAligned<sizeof(uint32_t)>(data_.data()));
    return reinterpret_cast<uint32_t*>(data_.data());
  }

  // Returns pointer to array of offsets (indexed by class def indices) in the
  // header of this dex section.
  uint32_t* GetOffsetArray() { return &GetSizeField()[1]; }
  uint32_t GetCurrentDataSize() const { return data_.size(); }

  // Number of class defs in this dex file.
  const uint32_t num_classdefs_;

  // Next expected class def index.
  uint32_t next_class_def_idx_;

  // Whether non-zero flags have been encountered for this class def.
  bool class_def_has_non_zero_flags_;

  // Whether any non-zero flags have been encountered for this dex file.
  bool dex_file_has_non_zero_flags_;

  // Vector containing the data of the built data structure.
  std::vector<uint8_t> data_;
};

// Edits a dex file, inserting a new HiddenapiClassData section.
class DexFileEditor final {
 public:
  DexFileEditor(const DexFile& old_dex, const std::vector<uint8_t>& hiddenapi_class_data)
      : old_dex_(old_dex),
        hiddenapi_class_data_(hiddenapi_class_data),
        loaded_dex_header_(nullptr),
        loaded_dex_maplist_(nullptr) {}

  // Copies dex file into a backing data vector, appends the given HiddenapiClassData
  // and updates the MapList.
  void Encode() {
    // We do not support non-standard dex encodings, e.g. compact dex.
    CHECK(old_dex_.IsStandardDexFile());

    // If there are no data to append, copy the old dex file and return.
    if (hiddenapi_class_data_.empty()) {
      AllocateMemory(old_dex_.Size());
      Append(old_dex_.Begin(), old_dex_.Size(), /* update_header= */ false);
      return;
    }

    // Find the old MapList, find its size.
    const dex::MapList* old_map = old_dex_.GetMapList();
    CHECK_LT(old_map->size_, std::numeric_limits<uint32_t>::max());

    // Compute the size of the new dex file. We append the HiddenapiClassData,
    // one MapItem and possibly some padding to align the new MapList.
    CHECK(IsAligned<kMapListAlignment>(old_dex_.Size()))
        << "End of input dex file is not 4-byte aligned, possibly because its MapList is not "
        << "at the end of the file.";
    size_t size_delta =
        RoundUp(hiddenapi_class_data_.size(), kMapListAlignment) + sizeof(dex::MapItem);
    size_t new_size = old_dex_.Size() + size_delta;
    AllocateMemory(new_size);

    // Copy the old dex file into the backing data vector. Load the copied
    // dex file to obtain pointers to its header and MapList.
    Append(old_dex_.Begin(), old_dex_.Size(), /* update_header= */ false);
    ReloadDex(/* verify= */ false);

    // Truncate the new dex file before the old MapList. This assumes that
    // the MapList is the last entry in the dex file. This is currently true
    // for our tooling.
    // TODO: Implement the general case by zero-ing the old MapList (turning
    // it into padding.
    RemoveOldMapList();

    // Append HiddenapiClassData.
    size_t payload_offset = AppendHiddenapiClassData();

    // Wrute new MapList with an entry for HiddenapiClassData.
    CreateMapListWithNewItem(payload_offset);

    // Check that the pre-computed size matches the actual size.
    CHECK_EQ(offset_, new_size);

    // Reload to all data structures.
    ReloadDex(/* verify= */ false);

    // Update the dex checksum.
    UpdateChecksum();

    // Run DexFileVerifier on the new dex file as a CHECK.
    ReloadDex(/* verify= */ true);
  }

  // Writes the edited dex file into a file.
  void WriteTo(const std::string& path) {
    CHECK(!data_.empty());
    std::ofstream ofs(path.c_str(), std::ofstream::out | std::ofstream::binary);
    ofs.write(reinterpret_cast<const char*>(data_.data()), data_.size());
    ofs.flush();
    CHECK(ofs.good());
    ofs.close();
  }

 private:
  static constexpr size_t kMapListAlignment = 4u;
  static constexpr size_t kHiddenapiClassDataAlignment = 4u;

  void ReloadDex(bool verify) {
    std::string error_msg;
    DexFileLoader loader;
    loaded_dex_ = loader.Open(
        data_.data(),
        data_.size(),
        "test_location",
        old_dex_.GetLocationChecksum(),
        /* oat_dex_file= */ nullptr,
        /* verify= */ verify,
        /* verify_checksum= */ verify,
        &error_msg);
    if (loaded_dex_.get() == nullptr) {
      LOG(FATAL) << "Failed to load edited dex file: " << error_msg;
      UNREACHABLE();
    }

    // Load the location of header and map list before we start editing the file.
    loaded_dex_header_ = const_cast<DexFile::Header*>(&loaded_dex_->GetHeader());
    loaded_dex_maplist_ = const_cast<dex::MapList*>(loaded_dex_->GetMapList());
  }

  DexFile::Header& GetHeader() const {
    CHECK(loaded_dex_header_ != nullptr);
    return *loaded_dex_header_;
  }

  dex::MapList& GetMapList() const {
    CHECK(loaded_dex_maplist_ != nullptr);
    return *loaded_dex_maplist_;
  }

  void AllocateMemory(size_t total_size) {
    data_.clear();
    data_.resize(total_size);
    CHECK(IsAligned<kMapListAlignment>(data_.data()));
    CHECK(IsAligned<kHiddenapiClassDataAlignment>(data_.data()));
    offset_ = 0;
  }

  uint8_t* GetCurrentDataPtr() {
    return data_.data() + offset_;
  }

  void UpdateDataSize(off_t delta, bool update_header) {
    offset_ += delta;
    if (update_header) {
      DexFile::Header& header = GetHeader();
      header.file_size_ += delta;
      header.data_size_ += delta;
    }
  }

  template<typename T>
  T* Append(const T* src, size_t len, bool update_header = true) {
    CHECK_LE(offset_ + len, data_.size());
    uint8_t* dst = GetCurrentDataPtr();
    memcpy(dst, src, len);
    UpdateDataSize(len, update_header);
    return reinterpret_cast<T*>(dst);
  }

  void InsertPadding(size_t alignment) {
    size_t len = RoundUp(offset_, alignment) - offset_;
    std::vector<uint8_t> padding(len, 0);
    Append(padding.data(), padding.size());
  }

  void RemoveOldMapList() {
    size_t map_size = GetMapList().Size();
    uint8_t* map_start = reinterpret_cast<uint8_t*>(&GetMapList());
    CHECK_EQ(map_start + map_size, GetCurrentDataPtr()) << "MapList not at the end of dex file";
    UpdateDataSize(-static_cast<off_t>(map_size), /* update_header= */ true);
    CHECK_EQ(map_start, GetCurrentDataPtr());
    loaded_dex_maplist_ = nullptr;  // do not use this map list any more
  }

  void CreateMapListWithNewItem(size_t payload_offset) {
    InsertPadding(/* alignment= */ kMapListAlignment);

    size_t new_map_offset = offset_;
    dex::MapList* map = Append(old_dex_.GetMapList(), old_dex_.GetMapList()->Size());

    // Check last map entry is a pointer to itself.
    dex::MapItem& old_item = map->list_[map->size_ - 1];
    CHECK(old_item.type_ == DexFile::kDexTypeMapList);
    CHECK_EQ(old_item.size_, 1u);
    CHECK_EQ(old_item.offset_, GetHeader().map_off_);

    // Create a new MapItem entry with new MapList details.
    dex::MapItem new_item;
    new_item.type_ = old_item.type_;
    new_item.unused_ = 0u;  // initialize to ensure dex output is deterministic (b/119308882)
    new_item.size_ = old_item.size_;
    new_item.offset_ = new_map_offset;

    // Update pointer in the header.
    GetHeader().map_off_ = new_map_offset;

    // Append a new MapItem and return its pointer.
    map->size_++;
    Append(&new_item, sizeof(dex::MapItem));

    // Change penultimate entry to point to metadata.
    old_item.type_ = DexFile::kDexTypeHiddenapiClassData;
    old_item.size_ = 1u;  // there is only one section
    old_item.offset_ = payload_offset;
  }

  size_t AppendHiddenapiClassData() {
    size_t payload_offset = offset_;
    CHECK_EQ(kMapListAlignment, kHiddenapiClassDataAlignment);
    CHECK(IsAligned<kHiddenapiClassDataAlignment>(payload_offset))
        << "Should not need to align the section, previous data was already aligned";
    Append(hiddenapi_class_data_.data(), hiddenapi_class_data_.size());
    return payload_offset;
  }

  void UpdateChecksum() {
    GetHeader().checksum_ = loaded_dex_->CalculateChecksum();
  }

  const DexFile& old_dex_;
  const std::vector<uint8_t>& hiddenapi_class_data_;

  std::vector<uint8_t> data_;
  size_t offset_;

  std::unique_ptr<const DexFile> loaded_dex_;
  DexFile::Header* loaded_dex_header_;
  dex::MapList* loaded_dex_maplist_;
};

class HiddenApi final {
 public:
  HiddenApi() : force_assign_all_(true) {}

  void Run(int argc, char** argv) {
    switch (ParseArgs(argc, argv)) {
    case Command::kEncode:
      EncodeAccessFlags();
      break;
    case Command::kList:
      ListApi();
      break;
    }
  }

 private:
  enum class Command {
    kEncode,
    kList,
  };

  Command ParseArgs(int argc, char** argv) {
    // Skip over the binary's path.
    argv++;
    argc--;

    if (argc > 0) {
      const char* raw_command = argv[0];
      const std::string_view command(raw_command);
      if (command == "encode") {
        for (int i = 1; i < argc; ++i) {
          const char* raw_option = argv[i];
          const std::string_view option(raw_option);
          if (StartsWith(option, "--input-dex=")) {
            boot_dex_paths_.push_back(std::string(option.substr(strlen("--input-dex="))));
          } else if (StartsWith(option, "--output-dex=")) {
            output_dex_paths_.push_back(std::string(option.substr(strlen("--output-dex="))));
          } else if (StartsWith(option, "--api-flags=")) {
            api_flags_path_ = std::string(option.substr(strlen("--api-flags=")));
          } else if (option == "--no-force-assign-all") {
            force_assign_all_ = false;
          } else {
            Usage("Unknown argument '%s'", raw_option);
          }
        }
        return Command::kEncode;
      } else if (command == "list") {
        for (int i = 1; i < argc; ++i) {
          const char* raw_option = argv[i];
          const std::string_view option(raw_option);
          if (StartsWith(option, "--boot-dex=")) {
            boot_dex_paths_.push_back(std::string(option.substr(strlen("--boot-dex="))));
          } else if (StartsWith(option, "--public-stub-classpath=")) {
            stub_classpaths_.push_back(std::make_pair(
                std::string(option.substr(strlen("--public-stub-classpath="))),
                ApiStubs::Kind::kPublicApi));
          } else if (StartsWith(option, "--system-stub-classpath=")) {
            stub_classpaths_.push_back(std::make_pair(
                std::string(option.substr(strlen("--system-stub-classpath="))),
                ApiStubs::Kind::kSystemApi));
          } else if (StartsWith(option, "--test-stub-classpath=")) {
            stub_classpaths_.push_back(std::make_pair(
                std::string(option.substr(strlen("--test-stub-classpath="))),
                ApiStubs::Kind::kTestApi));
          } else if (StartsWith(option, "--core-platform-stub-classpath=")) {
            stub_classpaths_.push_back(std::make_pair(
                std::string(option.substr(strlen("--core-platform-stub-classpath="))),
                ApiStubs::Kind::kCorePlatformApi));
          } else if (StartsWith(option, "--out-api-flags=")) {
            api_flags_path_ = std::string(option.substr(strlen("--out-api-flags=")));
          } else {
            Usage("Unknown argument '%s'", raw_option);
          }
        }
        return Command::kList;
      } else {
        Usage("Unknown command '%s'", raw_command);
      }
    } else {
      Usage("No command specified");
    }
  }

  void EncodeAccessFlags() {
    if (boot_dex_paths_.empty()) {
      Usage("No input DEX files specified");
    } else if (output_dex_paths_.size() != boot_dex_paths_.size()) {
      Usage("Number of input DEX files does not match number of output DEX files");
    }

    // Load dex signatures.
    std::map<std::string, ApiList> api_list = OpenApiFile(api_flags_path_);

    // Iterate over input dex files and insert HiddenapiClassData sections.
    for (size_t i = 0; i < boot_dex_paths_.size(); ++i) {
      const std::string& input_path = boot_dex_paths_[i];
      const std::string& output_path = output_dex_paths_[i];

      ClassPath boot_classpath({ input_path }, /* open_writable= */ false);
      std::vector<const DexFile*> input_dex_files = boot_classpath.GetDexFiles();
      CHECK_EQ(input_dex_files.size(), 1u);
      const DexFile& input_dex = *input_dex_files[0];

      HiddenapiClassDataBuilder builder(input_dex);
      boot_classpath.ForEachDexClass([&](const DexClass& boot_class) {
        builder.BeginClassDef(boot_class.GetClassDefIndex());
        if (boot_class.GetData() != nullptr) {
          auto fn_shared = [&](const DexMember& boot_member) {
            auto it = api_list.find(boot_member.GetApiEntry());
            bool api_list_found = (it != api_list.end());
            CHECK(!force_assign_all_ || api_list_found)
                << "Could not find hiddenapi flags for dex entry: " << boot_member.GetApiEntry();
            builder.WriteFlags(api_list_found ? it->second : ApiList::Whitelist());
          };
          auto fn_field = [&](const ClassAccessor::Field& boot_field) {
            fn_shared(DexMember(boot_class, boot_field));
          };
          auto fn_method = [&](const ClassAccessor::Method& boot_method) {
            fn_shared(DexMember(boot_class, boot_method));
          };
          boot_class.VisitFieldsAndMethods(fn_field, fn_field, fn_method, fn_method);
        }
        builder.EndClassDef(boot_class.GetClassDefIndex());
      });

      DexFileEditor dex_editor(input_dex, builder.GetData());
      dex_editor.Encode();
      dex_editor.WriteTo(output_path);
    }
  }

  std::map<std::string, ApiList> OpenApiFile(const std::string& path) {
    CHECK(!path.empty());
    std::ifstream api_file(path, std::ifstream::in);
    CHECK(!api_file.fail()) << "Unable to open file '" << path << "' " << strerror(errno);

    std::map<std::string, ApiList> api_flag_map;

    size_t line_number = 1;
    for (std::string line; std::getline(api_file, line); line_number++) {
      // Every line contains a comma separated list with the signature as the
      // first element and the api flags as the rest
      std::vector<std::string> values = android::base::Split(line, ",");
      CHECK_GT(values.size(), 1u) << path << ":" << line_number
          << ": No flags found: " << line << kErrorHelp;

      const std::string& signature = values[0];

      CHECK(api_flag_map.find(signature) == api_flag_map.end()) << path << ":" << line_number
          << ": Duplicate entry: " << signature << kErrorHelp;

      ApiList membership;

      bool success = ApiList::FromNames(values.begin() + 1, values.end(), &membership);
      CHECK(success) << path << ":" << line_number
          << ": Some flags were not recognized: " << line << kErrorHelp;
      CHECK(membership.IsValid()) << path << ":" << line_number
          << ": Invalid combination of flags: " << line << kErrorHelp;

      api_flag_map.emplace(signature, membership);
    }

    api_file.close();
    return api_flag_map;
  }

  void ListApi() {
    if (boot_dex_paths_.empty()) {
      Usage("No boot DEX files specified");
    } else if (stub_classpaths_.empty()) {
      Usage("No stub DEX files specified");
    } else if (api_flags_path_.empty()) {
      Usage("No output path specified");
    }

    // Complete list of boot class path members. The associated boolean states
    // whether it is public (true) or private (false).
    std::map<std::string, std::set<std::string_view>> boot_members;

    // Deduplicate errors before printing them.
    std::set<std::string> unresolved;

    // Open all dex files.
    ClassPath boot_classpath(boot_dex_paths_, /* open_writable= */ false);
    Hierarchy boot_hierarchy(boot_classpath);

    // Mark all boot dex members private.
    boot_classpath.ForEachDexMember([&](const DexMember& boot_member) {
      boot_members[boot_member.GetApiEntry()] = {};
    });

    // Resolve each SDK dex member against the framework and mark it white.
    for (const auto& cp_entry : stub_classpaths_) {
      ClassPath stub_classpath(android::base::Split(cp_entry.first, ":"),
                               /* open_writable= */ false);
      Hierarchy stub_hierarchy(stub_classpath);
      const ApiStubs::Kind stub_api = cp_entry.second;

      stub_classpath.ForEachDexMember(
          [&](const DexMember& stub_member) {
            if (!stub_hierarchy.IsMemberVisible(stub_member)) {
              // Typically fake constructors and inner-class `this` fields.
              return;
            }
            bool resolved = boot_hierarchy.ForEachResolvableMember(
                stub_member,
                [&](const DexMember& boot_member) {
                  std::string entry = boot_member.GetApiEntry();
                  auto it = boot_members.find(entry);
                  CHECK(it != boot_members.end());
                  it->second.insert(ApiStubs::ToString(stub_api));
                });
            if (!resolved) {
              unresolved.insert(stub_member.GetApiEntry());
            }
          });
    }

    // Print errors.
    for (const std::string& str : unresolved) {
      LOG(WARNING) << "unresolved: " << str;
    }

    // Write into public/private API files.
    std::ofstream file_flags(api_flags_path_.c_str());
    for (const auto& entry : boot_members) {
      if (entry.second.empty()) {
        file_flags << entry.first << std::endl;
      } else {
        file_flags << entry.first << ",";
        file_flags << android::base::Join(entry.second, ",") << std::endl;
      }
    }
    file_flags.close();
  }

  // Whether to check that all dex entries have been assigned flags.
  // Defaults to true.
  bool force_assign_all_;

  // Paths to DEX files which should be processed.
  std::vector<std::string> boot_dex_paths_;

  // Output paths where modified DEX files should be written.
  std::vector<std::string> output_dex_paths_;

  // Set of public API stub classpaths. Each classpath is formed by a list
  // of DEX/APK files in the order they appear on the classpath.
  std::vector<std::pair<std::string, ApiStubs::Kind>> stub_classpaths_;

  // Path to CSV file containing the list of API members and their flags.
  // This could be both an input and output path.
  std::string api_flags_path_;
};

}  // namespace hiddenapi
}  // namespace art

int main(int argc, char** argv) {
  art::hiddenapi::original_argc = argc;
  art::hiddenapi::original_argv = argv;
  android::base::InitLogging(argv);
  art::MemMap::Init();
  art::hiddenapi::HiddenApi().Run(argc, argv);
  return EXIT_SUCCESS;
}
