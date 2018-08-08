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

#include "android-base/stringprintf.h"
#include "android-base/strings.h"

#include "base/mem_map.h"
#include "base/os.h"
#include "base/unix_file/fd_file.h"
#include "dex/art_dex_file_loader.h"
#include "dex/class_accessor-inl.h"
#include "dex/dex_file-inl.h"
#include "dex/hidden_api_access_flags.h"

namespace art {

static int original_argc;
static char** original_argv;

static std::string CommandLine() {
  std::vector<std::string> command;
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
  UsageError("    --dex=<filename>: dex file which belongs to boot class path,");
  UsageError("                      the file will be overwritten");
  UsageError("");
  UsageError("    --light-greylist=<filename>:");
  UsageError("    --dark-greylist=<filename>:");
  UsageError("    --blacklist=<filename>:");
  UsageError("        text files with signatures of methods/fields to be annotated");
  UsageError("");
  UsageError("  Command \"list\": dump lists of public and private API");
  UsageError("    --boot-dex=<filename>: dex file which belongs to boot class path");
  UsageError("    --stub-classpath=<filenames>: colon-separated list of dex/apk files");
  UsageError("        which form API stubs of boot class path. Multiple classpaths can");
  UsageError("        be specified");
  UsageError("");
  UsageError("    --out-public=<filename>: output file for a list of all public APIs");
  UsageError("    --out-private=<filename>: output file for a list of all private APIs");
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

  std::string GetSuperclassDescriptor() const {
    return HasSuperclass() ? dex_file_.StringByTypeIdx(GetSuperclassIndex()) : "";
  }

  std::set<std::string> GetInterfaceDescriptors() const {
    std::set<std::string> list;
    const DexFile::TypeList* ifaces = dex_file_.GetInterfacesList(GetClassDef());
    for (uint32_t i = 0; ifaces != nullptr && i < ifaces->Size(); ++i) {
      list.insert(dex_file_.StringByTypeIdx(ifaces->GetTypeItem(i).type_idx_));
    }
    return list;
  }

  inline bool IsPublic() const { return HasAccessFlags(kAccPublic); }

  inline bool Equals(const DexClass& other) const {
    bool equals = strcmp(GetDescriptor(), other.GetDescriptor()) == 0;
    if (equals) {
      // TODO(dbrazdil): Check that methods/fields match as well once b/111116543 is fixed.
      CHECK_EQ(GetAccessFlags(), other.GetAccessFlags());
      CHECK_EQ(GetSuperclassDescriptor(), other.GetSuperclassDescriptor());
      CHECK(GetInterfaceDescriptors() == other.GetInterfaceDescriptors());
    }
    return equals;
  }

 private:
  uint32_t GetAccessFlags() const { return GetClassDef().access_flags_; }
  bool HasAccessFlags(uint32_t mask) const { return (GetAccessFlags() & mask) == mask; }
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

  // Sets hidden bits in access flags and writes them back into the DEX in memory.
  // Note that this will not update the cached data of the class accessor
  // until it iterates over this item again and therefore will fail a CHECK if
  // it is called multiple times on the same DexMember.
  void SetHidden(HiddenApiAccessFlags::ApiList value) const {
    const uint32_t old_flags = item_.GetRawAccessFlags();
    const uint32_t new_flags = HiddenApiAccessFlags::EncodeForDex(old_flags, value);
    CHECK_EQ(UnsignedLeb128Size(new_flags), UnsignedLeb128Size(old_flags));

    // Locate the LEB128-encoded access flags in class data.
    // `ptr` initially points to the next ClassData item. We iterate backwards
    // until we hit the terminating byte of the previous Leb128 value.
    const uint8_t* ptr = item_.GetDataPointer();
    if (IsMethod()) {
      ptr = ReverseSearchUnsignedLeb128(ptr);
      DCHECK_EQ(DecodeUnsignedLeb128WithoutMovingCursor(ptr), GetMethod().GetCodeItemOffset());
    }
    ptr = ReverseSearchUnsignedLeb128(ptr);
    DCHECK_EQ(DecodeUnsignedLeb128WithoutMovingCursor(ptr), old_flags);

    // Overwrite the access flags.
    UpdateUnsignedLeb128(const_cast<uint8_t*>(ptr), new_flags);
  }

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
  inline uint32_t HasAccessFlags(uint32_t mask) const { return (GetAccessFlags() & mask) == mask; }

  inline std::string GetName() const {
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

  inline const DexFile::MethodId& GetMethodId() const {
    DCHECK(IsMethod());
    return item_.GetDexFile().GetMethodId(item_.GetIndex());
  }

  inline const DexFile::FieldId& GetFieldId() const {
    DCHECK(!IsMethod());
    return item_.GetDexFile().GetFieldId(item_.GetIndex());
  }

  const DexClass& klass_;
  const ClassAccessor::BaseItem& item_;
  const bool is_method_;
};

class ClassPath FINAL {
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
        File fd(filename.c_str(), O_RDWR, /* check_usage */ false);
        CHECK_NE(fd.Fd(), -1) << "Unable to open file '" << filename << "': " << strerror(errno);

        // Memory-map the dex file with MAP_SHARED flag so that changes in memory
        // propagate to the underlying file. We run dex file verification as if
        // the dex file was not in boot claass path to check basic assumptions,
        // such as that at most one of public/private/protected flag is set.
        // We do those checks here and skip them when loading the processed file
        // into boot class path.
        std::unique_ptr<const DexFile> dex_file(dex_loader.OpenDex(fd.Release(),
                                                                   /* location */ filename,
                                                                   /* verify */ true,
                                                                   /* verify_checksum */ true,
                                                                   /* mmap_shared */ true,
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
                                       /* location */ filename,
                                       /* verify */ true,
                                       /* verify_checksum */ true,
                                       &error_msg,
                                       &dex_files_);
        CHECK(success) << "Open failed for '" << filename << "' " << error_msg;
      }
    }
  }

  // Opened dex files. Note that these are opened as `const` but may be written into.
  std::vector<std::unique_ptr<const DexFile>> dex_files_;
};

class HierarchyClass FINAL {
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
    return ForEachResolvableMember_Impl(other, fn) != ResolutionResult::kNotFound;
  }

  // Returns true if this class contains at least one member matching `other`.
  bool HasMatchingMember(const DexMember& other) {
    return ForEachMatchingMember(
        other, [](const DexMember&) { return true; }) != ResolutionResult::kNotFound;
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
  // Result of resolution which takes into account whether the member was found
  // for the first time or not. This is just a performance optimization to prevent
  // re-visiting previously visited members.
  // Note that order matters. When accumulating results, we always pick the maximum.
  enum class ResolutionResult {
    kNotFound,
    kFoundOld,
    kFoundNew,
  };

  inline ResolutionResult Accumulate(ResolutionResult a, ResolutionResult b) {
    return static_cast<ResolutionResult>(
        std::max(static_cast<unsigned>(a), static_cast<unsigned>(b)));
  }

  template<typename Fn>
  ResolutionResult ForEachResolvableMember_Impl(const DexMember& other, Fn fn) {
    // First try to find a member matching `other` in this class.
    ResolutionResult foundInClass = ForEachMatchingMember(other, fn);

    switch (foundInClass) {
      case ResolutionResult::kFoundOld:
        // A matching member was found and previously explored. All subclasses
        // must have been explored too.
        break;

      case ResolutionResult::kFoundNew:
        // A matching member was found and this was the first time it was visited.
        // If it is a virtual method, visit all methods overriding/implementing it too.
        if (other.IsVirtualMethod()) {
          for (HierarchyClass* subclass : extended_by_) {
            subclass->ForEachOverridingMember(other, fn);
          }
        }
        break;

      case ResolutionResult::kNotFound:
        // A matching member was not found in this class. Explore the superclasses
        // and implemented interfaces.
        for (HierarchyClass* superclass : extends_) {
          foundInClass = Accumulate(
              foundInClass, superclass->ForEachResolvableMember_Impl(other, fn));
        }
        break;
    }

    return foundInClass;
  }

  template<typename Fn>
  ResolutionResult ForEachMatchingMember(const DexMember& other, Fn fn) {
    ResolutionResult found = ResolutionResult::kNotFound;
    auto compare_member = [&](const DexMember& member) {
      if (member == other) {
        found = Accumulate(found, fn(member) ? ResolutionResult::kFoundNew
                                             : ResolutionResult::kFoundOld);
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

  template<typename Fn>
  void ForEachOverridingMember(const DexMember& other, Fn fn) {
    CHECK(other.IsVirtualMethod());
    ResolutionResult found = ForEachMatchingMember(other, fn);
    if (found == ResolutionResult::kFoundOld) {
      // No need to explore further.
      return;
    } else {
      for (HierarchyClass* subclass : extended_by_) {
        subclass->ForEachOverridingMember(other, fn);
      }
    }
  }

  // DexClass entries of this class found across all the provided dex files.
  std::vector<DexClass> dex_classes_;

  // Classes which this class inherits, or interfaces which it implements.
  std::vector<HierarchyClass*> extends_;

  // Classes which inherit from this class.
  std::vector<HierarchyClass*> extended_by_;
};

class Hierarchy FINAL {
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
  HierarchyClass* FindClass(const std::string& descriptor) {
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
      CHECK(superclass != nullptr);
      klass.AddExtends(*superclass);

      for (const std::string& iface_desc : dex_klass.GetInterfaceDescriptors()) {
        HierarchyClass* iface = FindClass(iface_desc);
        CHECK(iface != nullptr);
        klass.AddExtends(*iface);
      }
    }
  }

  ClassPath& classpath_;
  std::map<std::string, HierarchyClass> classes_;
};

class HiddenApi FINAL {
 public:
  HiddenApi() {}

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
      const StringPiece command(argv[0]);
      if (command == "encode") {
        for (int i = 1; i < argc; ++i) {
          const StringPiece option(argv[i]);
          if (option.starts_with("--dex=")) {
            boot_dex_paths_.push_back(option.substr(strlen("--dex=")).ToString());
          } else if (option.starts_with("--light-greylist=")) {
            light_greylist_path_ = option.substr(strlen("--light-greylist=")).ToString();
          } else if (option.starts_with("--dark-greylist=")) {
            dark_greylist_path_ = option.substr(strlen("--dark-greylist=")).ToString();
          } else if (option.starts_with("--blacklist=")) {
            blacklist_path_ = option.substr(strlen("--blacklist=")).ToString();
          } else {
            Usage("Unknown argument '%s'", option.data());
          }
        }
        return Command::kEncode;
      } else if (command == "list") {
        for (int i = 1; i < argc; ++i) {
          const StringPiece option(argv[i]);
          if (option.starts_with("--boot-dex=")) {
            boot_dex_paths_.push_back(option.substr(strlen("--boot-dex=")).ToString());
          } else if (option.starts_with("--stub-classpath=")) {
            stub_classpaths_.push_back(android::base::Split(
                option.substr(strlen("--stub-classpath=")).ToString(), ":"));
          } else if (option.starts_with("--out-public=")) {
            out_public_path_ = option.substr(strlen("--out-public=")).ToString();
          } else if (option.starts_with("--out-private=")) {
            out_private_path_ = option.substr(strlen("--out-private=")).ToString();
          } else {
            Usage("Unknown argument '%s'", option.data());
          }
        }
        return Command::kList;
      } else {
        Usage("Unknown command '%s'", command.data());
      }
    } else {
      Usage("No command specified");
    }
  }

  void EncodeAccessFlags() {
    if (boot_dex_paths_.empty()) {
      Usage("No boot DEX files specified");
    }

    // Load dex signatures.
    std::map<std::string, HiddenApiAccessFlags::ApiList> api_list;
    OpenApiFile(light_greylist_path_, api_list, HiddenApiAccessFlags::kLightGreylist);
    OpenApiFile(dark_greylist_path_, api_list, HiddenApiAccessFlags::kDarkGreylist);
    OpenApiFile(blacklist_path_, api_list, HiddenApiAccessFlags::kBlacklist);

    // Open all dex files.
    ClassPath boot_classpath(boot_dex_paths_, /* open_writable */ true);

    // Set access flags of all members.
    boot_classpath.ForEachDexMember([&api_list](const DexMember& boot_member) {
      auto it = api_list.find(boot_member.GetApiEntry());
      boot_member.SetHidden(it == api_list.end() ? HiddenApiAccessFlags::kWhitelist : it->second);
    });

    boot_classpath.UpdateDexChecksums();
  }

  void OpenApiFile(const std::string& path,
                   std::map<std::string, HiddenApiAccessFlags::ApiList>& api_list,
                   HiddenApiAccessFlags::ApiList membership) {
    if (path.empty()) {
      return;
    }

    std::ifstream api_file(path, std::ifstream::in);
    CHECK(!api_file.fail()) << "Unable to open file '" << path << "' " << strerror(errno);

    for (std::string line; std::getline(api_file, line);) {
      CHECK(api_list.find(line) == api_list.end())
          << "Duplicate entry: " << line << " (" << api_list[line] << " and " << membership << ")";
      api_list.emplace(line, membership);
    }
    api_file.close();
  }

  void ListApi() {
    if (boot_dex_paths_.empty()) {
      Usage("No boot DEX files specified");
    } else if (stub_classpaths_.empty()) {
      Usage("No stub DEX files specified");
    } else if (out_public_path_.empty()) {
      Usage("No public API output path specified");
    } else if (out_private_path_.empty()) {
      Usage("No private API output path specified");
    }

    // Complete list of boot class path members. The associated boolean states
    // whether it is public (true) or private (false).
    std::map<std::string, bool> boot_members;

    // Deduplicate errors before printing them.
    std::set<std::string> unresolved;

    // Open all dex files.
    ClassPath boot_classpath(boot_dex_paths_, /* open_writable */ false);
    Hierarchy boot_hierarchy(boot_classpath);

    // Mark all boot dex members private.
    boot_classpath.ForEachDexMember([&boot_members](const DexMember& boot_member) {
      boot_members[boot_member.GetApiEntry()] = false;
    });

    // Resolve each SDK dex member against the framework and mark it white.
    for (const std::vector<std::string>& stub_classpath_dex : stub_classpaths_) {
      ClassPath stub_classpath(stub_classpath_dex, /* open_writable */ false);
      Hierarchy stub_hierarchy(stub_classpath);
      stub_classpath.ForEachDexMember(
          [&stub_hierarchy, &boot_hierarchy, &boot_members, &unresolved](
              const DexMember& stub_member) {
            if (!stub_hierarchy.IsMemberVisible(stub_member)) {
              // Typically fake constructors and inner-class `this` fields.
              return;
            }
            bool resolved = boot_hierarchy.ForEachResolvableMember(
                stub_member,
                [&boot_members](const DexMember& boot_member) {
                  std::string entry = boot_member.GetApiEntry();
                  auto it = boot_members.find(entry);
                  CHECK(it != boot_members.end());
                  if (it->second) {
                    return false;  // has been marked before
                  } else {
                    it->second = true;
                    return true;  // marked for the first time
                  }
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
    std::ofstream file_public(out_public_path_.c_str());
    std::ofstream file_private(out_private_path_.c_str());
    for (const std::pair<std::string, bool> entry : boot_members) {
      if (entry.second) {
        file_public << entry.first << std::endl;
      } else {
        file_private << entry.first << std::endl;
      }
    }
    file_public.close();
    file_private.close();
  }

  // Paths to DEX files which should be processed.
  std::vector<std::string> boot_dex_paths_;

  // Set of public API stub classpaths. Each classpath is formed by a list
  // of DEX/APK files in the order they appear on the classpath.
  std::vector<std::vector<std::string>> stub_classpaths_;

  // Paths to text files which contain the lists of API members.
  std::string light_greylist_path_;
  std::string dark_greylist_path_;
  std::string blacklist_path_;

  // Paths to text files to which we will output list of all API members.
  std::string out_public_path_;
  std::string out_private_path_;
};

}  // namespace art

int main(int argc, char** argv) {
  android::base::InitLogging(argv);
  art::MemMap::Init();
  art::HiddenApi().Run(argc, argv);
  return EXIT_SUCCESS;
}
