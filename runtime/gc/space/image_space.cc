/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include "image_space.h"

#include <sys/statvfs.h>
#include <sys/types.h>
#include <unistd.h>

#include <random>

#include "android-base/stringprintf.h"
#include "android-base/strings.h"

#include "arch/instruction_set.h"
#include "art_field-inl.h"
#include "art_method-inl.h"
#include "base/array_ref.h"
#include "base/bit_memory_region.h"
#include "base/callee_save_type.h"
#include "base/enums.h"
#include "base/file_utils.h"
#include "base/macros.h"
#include "base/os.h"
#include "base/scoped_flock.h"
#include "base/stl_util.h"
#include "base/systrace.h"
#include "base/time_utils.h"
#include "base/utils.h"
#include "class_root.h"
#include "dex/art_dex_file_loader.h"
#include "dex/dex_file_loader.h"
#include "exec_utils.h"
#include "gc/accounting/space_bitmap-inl.h"
#include "gc/task_processor.h"
#include "image-inl.h"
#include "image_space_fs.h"
#include "intern_table-inl.h"
#include "mirror/class-inl.h"
#include "mirror/executable-inl.h"
#include "mirror/object-inl.h"
#include "mirror/object-refvisitor-inl.h"
#include "oat_file.h"
#include "runtime.h"
#include "space-inl.h"

namespace art {
namespace gc {
namespace space {

using android::base::StringAppendF;
using android::base::StringPrintf;

Atomic<uint32_t> ImageSpace::bitmap_index_(0);

ImageSpace::ImageSpace(const std::string& image_filename,
                       const char* image_location,
                       MemMap&& mem_map,
                       std::unique_ptr<accounting::ContinuousSpaceBitmap> live_bitmap,
                       uint8_t* end)
    : MemMapSpace(image_filename,
                  std::move(mem_map),
                  mem_map.Begin(),
                  end,
                  end,
                  kGcRetentionPolicyNeverCollect),
      live_bitmap_(std::move(live_bitmap)),
      oat_file_non_owned_(nullptr),
      image_location_(image_location) {
  DCHECK(live_bitmap_ != nullptr);
}

static int32_t ChooseRelocationOffsetDelta(int32_t min_delta, int32_t max_delta) {
  CHECK_ALIGNED(min_delta, kPageSize);
  CHECK_ALIGNED(max_delta, kPageSize);
  CHECK_LT(min_delta, max_delta);

  int32_t r = GetRandomNumber<int32_t>(min_delta, max_delta);
  if (r % 2 == 0) {
    r = RoundUp(r, kPageSize);
  } else {
    r = RoundDown(r, kPageSize);
  }
  CHECK_LE(min_delta, r);
  CHECK_GE(max_delta, r);
  CHECK_ALIGNED(r, kPageSize);
  return r;
}

static int32_t ChooseRelocationOffsetDelta() {
  return ChooseRelocationOffsetDelta(ART_BASE_ADDRESS_MIN_DELTA, ART_BASE_ADDRESS_MAX_DELTA);
}

static bool GenerateImage(const std::string& image_filename,
                          InstructionSet image_isa,
                          std::string* error_msg) {
  Runtime* runtime = Runtime::Current();
  const std::vector<std::string>& boot_class_path = runtime->GetBootClassPath();
  if (boot_class_path.empty()) {
    *error_msg = "Failed to generate image because no boot class path specified";
    return false;
  }
  // We should clean up so we are more likely to have room for the image.
  if (Runtime::Current()->IsZygote()) {
    LOG(INFO) << "Pruning dalvik-cache since we are generating an image and will need to recompile";
    PruneDalvikCache(image_isa);
  }

  std::vector<std::string> arg_vector;

  std::string dex2oat(Runtime::Current()->GetCompilerExecutable());
  arg_vector.push_back(dex2oat);

  char* dex2oat_bcp = getenv("DEX2OATBOOTCLASSPATH");
  std::vector<std::string> dex2oat_bcp_vector;
  if (dex2oat_bcp != nullptr) {
    arg_vector.push_back("--runtime-arg");
    arg_vector.push_back(StringPrintf("-Xbootclasspath:%s", dex2oat_bcp));
    Split(dex2oat_bcp, ':', &dex2oat_bcp_vector);
  }

  std::string image_option_string("--image=");
  image_option_string += image_filename;
  arg_vector.push_back(image_option_string);

  if (!dex2oat_bcp_vector.empty()) {
    for (size_t i = 0u; i < dex2oat_bcp_vector.size(); i++) {
      arg_vector.push_back(std::string("--dex-file=") + dex2oat_bcp_vector[i]);
      arg_vector.push_back(std::string("--dex-location=") + dex2oat_bcp_vector[i]);
    }
  } else {
    const std::vector<std::string>& boot_class_path_locations =
        runtime->GetBootClassPathLocations();
    DCHECK_EQ(boot_class_path.size(), boot_class_path_locations.size());
    for (size_t i = 0u; i < boot_class_path.size(); i++) {
      arg_vector.push_back(std::string("--dex-file=") + boot_class_path[i]);
      arg_vector.push_back(std::string("--dex-location=") + boot_class_path_locations[i]);
    }
  }

  std::string oat_file_option_string("--oat-file=");
  oat_file_option_string += ImageHeader::GetOatLocationFromImageLocation(image_filename);
  arg_vector.push_back(oat_file_option_string);

  // Note: we do not generate a fully debuggable boot image so we do not pass the
  // compiler flag --debuggable here.

  Runtime::Current()->AddCurrentRuntimeFeaturesAsDex2OatArguments(&arg_vector);
  CHECK_EQ(image_isa, kRuntimeISA)
      << "We should always be generating an image for the current isa.";

  int32_t base_offset = ChooseRelocationOffsetDelta();
  LOG(INFO) << "Using an offset of 0x" << std::hex << base_offset << " from default "
            << "art base address of 0x" << std::hex << ART_BASE_ADDRESS;
  arg_vector.push_back(StringPrintf("--base=0x%x", ART_BASE_ADDRESS + base_offset));

  if (!kIsTargetBuild) {
    arg_vector.push_back("--host");
  }

  const std::vector<std::string>& compiler_options = Runtime::Current()->GetImageCompilerOptions();
  for (size_t i = 0; i < compiler_options.size(); ++i) {
    arg_vector.push_back(compiler_options[i].c_str());
  }

  std::string command_line(android::base::Join(arg_vector, ' '));
  LOG(INFO) << "GenerateImage: " << command_line;
  return Exec(arg_vector, error_msg);
}

static bool FindImageFilenameImpl(const char* image_location,
                                  const InstructionSet image_isa,
                                  bool* has_system,
                                  std::string* system_filename,
                                  bool* dalvik_cache_exists,
                                  std::string* dalvik_cache,
                                  bool* is_global_cache,
                                  bool* has_cache,
                                  std::string* cache_filename) {
  DCHECK(dalvik_cache != nullptr);

  *has_system = false;
  *has_cache = false;
  // image_location = /system/framework/boot.art
  // system_image_location = /system/framework/<image_isa>/boot.art
  std::string system_image_filename(GetSystemImageFilename(image_location, image_isa));
  if (OS::FileExists(system_image_filename.c_str())) {
    *system_filename = system_image_filename;
    *has_system = true;
  }

  bool have_android_data = false;
  *dalvik_cache_exists = false;
  GetDalvikCache(GetInstructionSetString(image_isa),
                 /*create_if_absent=*/ true,
                 dalvik_cache,
                 &have_android_data,
                 dalvik_cache_exists,
                 is_global_cache);

  if (*dalvik_cache_exists) {
    DCHECK(have_android_data);
    // Always set output location even if it does not exist,
    // so that the caller knows where to create the image.
    //
    // image_location = /system/framework/boot.art
    // *image_filename = /data/dalvik-cache/<image_isa>/system@framework@boot.art
    std::string error_msg;
    if (!GetDalvikCacheFilename(image_location,
                                dalvik_cache->c_str(),
                                cache_filename,
                                &error_msg)) {
      LOG(WARNING) << error_msg;
      return *has_system;
    }
    *has_cache = OS::FileExists(cache_filename->c_str());
  }
  return *has_system || *has_cache;
}

bool ImageSpace::FindImageFilename(const char* image_location,
                                   const InstructionSet image_isa,
                                   std::string* system_filename,
                                   bool* has_system,
                                   std::string* cache_filename,
                                   bool* dalvik_cache_exists,
                                   bool* has_cache,
                                   bool* is_global_cache) {
  std::string dalvik_cache_unused;
  return FindImageFilenameImpl(image_location,
                               image_isa,
                               has_system,
                               system_filename,
                               dalvik_cache_exists,
                               &dalvik_cache_unused,
                               is_global_cache,
                               has_cache,
                               cache_filename);
}

static bool ReadSpecificImageHeader(const char* filename, ImageHeader* image_header) {
    std::unique_ptr<File> image_file(OS::OpenFileForReading(filename));
    if (image_file.get() == nullptr) {
      return false;
    }
    const bool success = image_file->ReadFully(image_header, sizeof(ImageHeader));
    if (!success || !image_header->IsValid()) {
      return false;
    }
    return true;
}

static std::unique_ptr<ImageHeader> ReadSpecificImageHeader(const char* filename,
                                                            std::string* error_msg) {
  std::unique_ptr<ImageHeader> hdr(new ImageHeader);
  if (!ReadSpecificImageHeader(filename, hdr.get())) {
    *error_msg = StringPrintf("Unable to read image header for %s", filename);
    return nullptr;
  }
  return hdr;
}

std::unique_ptr<ImageHeader> ImageSpace::ReadImageHeader(const char* image_location,
                                                         const InstructionSet image_isa,
                                                         ImageSpaceLoadingOrder order,
                                                         std::string* error_msg) {
  std::string system_filename;
  bool has_system = false;
  std::string cache_filename;
  bool has_cache = false;
  bool dalvik_cache_exists = false;
  bool is_global_cache = false;
  if (FindImageFilename(image_location,
                        image_isa,
                        &system_filename,
                        &has_system,
                        &cache_filename,
                        &dalvik_cache_exists,
                        &has_cache,
                        &is_global_cache)) {
    if (order == ImageSpaceLoadingOrder::kSystemFirst) {
      if (has_system) {
        return ReadSpecificImageHeader(system_filename.c_str(), error_msg);
      }
      if (has_cache) {
        return ReadSpecificImageHeader(cache_filename.c_str(), error_msg);
      }
    } else {
      if (has_cache) {
        return ReadSpecificImageHeader(cache_filename.c_str(), error_msg);
      }
      if (has_system) {
        return ReadSpecificImageHeader(system_filename.c_str(), error_msg);
      }
    }
  }

  *error_msg = StringPrintf("Unable to find image file for %s", image_location);
  return nullptr;
}

static bool CanWriteToDalvikCache(const InstructionSet isa) {
  const std::string dalvik_cache = GetDalvikCache(GetInstructionSetString(isa));
  if (access(dalvik_cache.c_str(), O_RDWR) == 0) {
    return true;
  } else if (errno != EACCES) {
    PLOG(WARNING) << "CanWriteToDalvikCache returned error other than EACCES";
  }
  return false;
}

static bool ImageCreationAllowed(bool is_global_cache,
                                 const InstructionSet isa,
                                 bool is_zygote,
                                 std::string* error_msg) {
  // Anyone can write into a "local" cache.
  if (!is_global_cache) {
    return true;
  }

  // Only the zygote running as root is allowed to create the global boot image.
  // If the zygote is running as non-root (and cannot write to the dalvik-cache),
  // then image creation is not allowed..
  if (is_zygote) {
    return CanWriteToDalvikCache(isa);
  }

  *error_msg = "Only the zygote can create the global boot image.";
  return false;
}

void ImageSpace::VerifyImageAllocations() {
  uint8_t* current = Begin() + RoundUp(sizeof(ImageHeader), kObjectAlignment);
  while (current < End()) {
    CHECK_ALIGNED(current, kObjectAlignment);
    auto* obj = reinterpret_cast<mirror::Object*>(current);
    CHECK(obj->GetClass() != nullptr) << "Image object at address " << obj << " has null class";
    CHECK(live_bitmap_->Test(obj)) << obj->PrettyTypeOf();
    if (kUseBakerReadBarrier) {
      obj->AssertReadBarrierState();
    }
    current += RoundUp(obj->SizeOf(), kObjectAlignment);
  }
}

// Helper class for relocating from one range of memory to another.
class RelocationRange {
 public:
  RelocationRange() = default;
  RelocationRange(const RelocationRange&) = default;
  RelocationRange(uintptr_t source, uintptr_t dest, uintptr_t length)
      : source_(source),
        dest_(dest),
        length_(length) {}

  bool InSource(uintptr_t address) const {
    return address - source_ < length_;
  }

  bool InDest(const void* dest) const {
    return InDest(reinterpret_cast<uintptr_t>(dest));
  }

  bool InDest(uintptr_t address) const {
    return address - dest_ < length_;
  }

  // Translate a source address to the destination space.
  uintptr_t ToDest(uintptr_t address) const {
    DCHECK(InSource(address));
    return address + Delta();
  }

  // Returns the delta between the dest from the source.
  uintptr_t Delta() const {
    return dest_ - source_;
  }

  uintptr_t Source() const {
    return source_;
  }

  uintptr_t Dest() const {
    return dest_;
  }

  uintptr_t Length() const {
    return length_;
  }

 private:
  const uintptr_t source_;
  const uintptr_t dest_;
  const uintptr_t length_;
};

std::ostream& operator<<(std::ostream& os, const RelocationRange& reloc) {
  return os << "(" << reinterpret_cast<const void*>(reloc.Source()) << "-"
            << reinterpret_cast<const void*>(reloc.Source() + reloc.Length()) << ")->("
            << reinterpret_cast<const void*>(reloc.Dest()) << "-"
            << reinterpret_cast<const void*>(reloc.Dest() + reloc.Length()) << ")";
}

template <PointerSize kPointerSize, typename HeapVisitor, typename NativeVisitor>
class ImageSpace::PatchObjectVisitor final {
 public:
  explicit PatchObjectVisitor(HeapVisitor heap_visitor, NativeVisitor native_visitor)
      : heap_visitor_(heap_visitor), native_visitor_(native_visitor) {}

  void VisitClass(mirror::Class* klass) REQUIRES_SHARED(Locks::mutator_lock_) {
    // A mirror::Class object consists of
    //  - instance fields inherited from j.l.Object,
    //  - instance fields inherited from j.l.Class,
    //  - embedded tables (vtable, interface method table),
    //  - static fields of the class itself.
    // The reference fields are at the start of each field section (this is how the
    // ClassLinker orders fields; except when that would create a gap between superclass
    // fields and the first reference of the subclass due to alignment, it can be filled
    // with smaller fields - but that's not the case for j.l.Object and j.l.Class).

    DCHECK_ALIGNED(klass, kObjectAlignment);
    static_assert(IsAligned<kHeapReferenceSize>(kObjectAlignment), "Object alignment check.");
    // First, patch the `klass->klass_`, known to be a reference to the j.l.Class.class.
    // This should be the only reference field in j.l.Object and we assert that below.
    PatchReferenceField</*kMayBeNull=*/ false>(klass, mirror::Object::ClassOffset());
    // Then patch the reference instance fields described by j.l.Class.class.
    // Use the sizeof(Object) to determine where these reference fields start;
    // this is the same as `class_class->GetFirstReferenceInstanceFieldOffset()`
    // after patching but the j.l.Class may not have been patched yet.
    mirror::Class* class_class = klass->GetClass<kVerifyNone, kWithoutReadBarrier>();
    size_t num_reference_instance_fields = class_class->NumReferenceInstanceFields<kVerifyNone>();
    DCHECK_NE(num_reference_instance_fields, 0u);
    static_assert(IsAligned<kHeapReferenceSize>(sizeof(mirror::Object)), "Size alignment check.");
    MemberOffset instance_field_offset(sizeof(mirror::Object));
    for (size_t i = 0; i != num_reference_instance_fields; ++i) {
      PatchReferenceField(klass, instance_field_offset);
      static_assert(sizeof(mirror::HeapReference<mirror::Object>) == kHeapReferenceSize,
                    "Heap reference sizes equality check.");
      instance_field_offset =
          MemberOffset(instance_field_offset.Uint32Value() + kHeapReferenceSize);
    }
    // Now that we have patched the `super_class_`, if this is the j.l.Class.class,
    // we can get a reference to j.l.Object.class and assert that it has only one
    // reference instance field (the `klass_` patched above).
    if (kIsDebugBuild && klass == class_class) {
      ObjPtr<mirror::Class> object_class =
          klass->GetSuperClass<kVerifyNone, kWithoutReadBarrier>();
      CHECK_EQ(object_class->NumReferenceInstanceFields<kVerifyNone>(), 1u);
    }
    // Then patch static fields.
    size_t num_reference_static_fields = klass->NumReferenceStaticFields<kVerifyNone>();
    if (num_reference_static_fields != 0u) {
      MemberOffset static_field_offset =
          klass->GetFirstReferenceStaticFieldOffset<kVerifyNone>(kPointerSize);
      for (size_t i = 0; i != num_reference_static_fields; ++i) {
        PatchReferenceField(klass, static_field_offset);
        static_assert(sizeof(mirror::HeapReference<mirror::Object>) == kHeapReferenceSize,
                      "Heap reference sizes equality check.");
        static_field_offset =
            MemberOffset(static_field_offset.Uint32Value() + kHeapReferenceSize);
      }
    }
    // Then patch native pointers.
    klass->FixupNativePointers<kVerifyNone>(klass, kPointerSize, *this);
  }

  template <typename T>
  T* operator()(T* ptr, void** dest_addr ATTRIBUTE_UNUSED) const {
    return (ptr != nullptr) ? native_visitor_(ptr) : nullptr;
  }

  void VisitPointerArray(ObjPtr<mirror::PointerArray> pointer_array)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    // Fully patch the pointer array, including the `klass_` field.
    PatchReferenceField</*kMayBeNull=*/ false>(pointer_array, mirror::Object::ClassOffset());

    int32_t length = pointer_array->GetLength<kVerifyNone>();
    for (int32_t i = 0; i != length; ++i) {
      ArtMethod** method_entry = reinterpret_cast<ArtMethod**>(
          pointer_array->ElementAddress<kVerifyNone>(i, kPointerSize));
      PatchNativePointer</*kMayBeNull=*/ false>(method_entry);
    }
  }

  void VisitObject(mirror::Object* object) REQUIRES_SHARED(Locks::mutator_lock_) {
    // Visit all reference fields.
    object->VisitReferences</*kVisitNativeRoots=*/ false,
                            kVerifyNone,
                            kWithoutReadBarrier>(*this, *this);
    // This function should not be called for classes.
    DCHECK(!object->IsClass<kVerifyNone>());
  }

  // Visitor for VisitReferences().
  ALWAYS_INLINE void operator()(ObjPtr<mirror::Object> object,
                                MemberOffset field_offset,
                                bool is_static)
      const REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK(!is_static);
    PatchReferenceField(object, field_offset);
  }
  // Visitor for VisitReferences(), java.lang.ref.Reference case.
  ALWAYS_INLINE void operator()(ObjPtr<mirror::Class> klass, ObjPtr<mirror::Reference> ref) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK(klass->IsTypeOfReferenceClass());
    this->operator()(ref, mirror::Reference::ReferentOffset(), /*is_static=*/ false);
  }
  // Ignore class native roots; not called from VisitReferences() for kVisitNativeRoots == false.
  void VisitRootIfNonNull(mirror::CompressedReference<mirror::Object>* root ATTRIBUTE_UNUSED)
      const {}
  void VisitRoot(mirror::CompressedReference<mirror::Object>* root ATTRIBUTE_UNUSED) const {}

  void VisitDexCacheArrays(ObjPtr<mirror::DexCache> dex_cache)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    FixupDexCacheArray<mirror::StringDexCacheType>(dex_cache,
                                                   mirror::DexCache::StringsOffset(),
                                                   dex_cache->NumStrings<kVerifyNone>());
    FixupDexCacheArray<mirror::TypeDexCacheType>(dex_cache,
                                                 mirror::DexCache::ResolvedTypesOffset(),
                                                 dex_cache->NumResolvedTypes<kVerifyNone>());
    FixupDexCacheArray<mirror::MethodDexCacheType>(dex_cache,
                                                   mirror::DexCache::ResolvedMethodsOffset(),
                                                   dex_cache->NumResolvedMethods<kVerifyNone>());
    FixupDexCacheArray<mirror::FieldDexCacheType>(dex_cache,
                                                  mirror::DexCache::ResolvedFieldsOffset(),
                                                  dex_cache->NumResolvedFields<kVerifyNone>());
    FixupDexCacheArray<mirror::MethodTypeDexCacheType>(
        dex_cache,
        mirror::DexCache::ResolvedMethodTypesOffset(),
        dex_cache->NumResolvedMethodTypes<kVerifyNone>());
    FixupDexCacheArray<GcRoot<mirror::CallSite>>(
        dex_cache,
        mirror::DexCache::ResolvedCallSitesOffset(),
        dex_cache->NumResolvedCallSites<kVerifyNone>());
    FixupDexCacheArray<GcRoot<mirror::String>>(
        dex_cache,
        mirror::DexCache::PreResolvedStringsOffset(),
        dex_cache->NumPreResolvedStrings<kVerifyNone>());
  }

  template <bool kMayBeNull = true, typename T>
  ALWAYS_INLINE void PatchGcRoot(/*inout*/GcRoot<T>* root) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    static_assert(sizeof(GcRoot<mirror::Class*>) == sizeof(uint32_t), "GcRoot size check");
    T* old_value = root->template Read<kWithoutReadBarrier>();
    DCHECK(kMayBeNull || old_value != nullptr);
    if (!kMayBeNull || old_value != nullptr) {
      *root = GcRoot<T>(heap_visitor_(old_value));
    }
  }

  template <bool kMayBeNull = true, typename T>
  ALWAYS_INLINE void PatchNativePointer(/*inout*/T** entry) const {
    if (kPointerSize == PointerSize::k64) {
      uint64_t* raw_entry = reinterpret_cast<uint64_t*>(entry);
      T* old_value = reinterpret_cast64<T*>(*raw_entry);
      DCHECK(kMayBeNull || old_value != nullptr);
      if (!kMayBeNull || old_value != nullptr) {
        T* new_value = native_visitor_(old_value);
        *raw_entry = reinterpret_cast64<uint64_t>(new_value);
      }
    } else {
      uint32_t* raw_entry = reinterpret_cast<uint32_t*>(entry);
      T* old_value = reinterpret_cast32<T*>(*raw_entry);
      DCHECK(kMayBeNull || old_value != nullptr);
      if (!kMayBeNull || old_value != nullptr) {
        T* new_value = native_visitor_(old_value);
        *raw_entry = reinterpret_cast32<uint32_t>(new_value);
      }
    }
  }

  template <bool kMayBeNull = true>
  ALWAYS_INLINE void PatchReferenceField(ObjPtr<mirror::Object> object, MemberOffset offset) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    ObjPtr<mirror::Object> old_value =
        object->GetFieldObject<mirror::Object, kVerifyNone, kWithoutReadBarrier>(offset);
    DCHECK(kMayBeNull || old_value != nullptr);
    if (!kMayBeNull || old_value != nullptr) {
      ObjPtr<mirror::Object> new_value = heap_visitor_(old_value.Ptr());
      object->SetFieldObjectWithoutWriteBarrier</*kTransactionActive=*/ false,
                                                /*kCheckTransaction=*/ true,
                                                kVerifyNone>(offset, new_value);
    }
  }

  template <typename T>
  void FixupDexCacheArrayEntry(std::atomic<mirror::DexCachePair<T>>* array, uint32_t index)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    static_assert(sizeof(std::atomic<mirror::DexCachePair<T>>) == sizeof(mirror::DexCachePair<T>),
                  "Size check for removing std::atomic<>.");
    PatchGcRoot(&(reinterpret_cast<mirror::DexCachePair<T>*>(array)[index].object));
  }

  template <typename T>
  void FixupDexCacheArrayEntry(std::atomic<mirror::NativeDexCachePair<T>>* array, uint32_t index)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    static_assert(sizeof(std::atomic<mirror::NativeDexCachePair<T>>) ==
                      sizeof(mirror::NativeDexCachePair<T>),
                  "Size check for removing std::atomic<>.");
    mirror::NativeDexCachePair<T> pair =
        mirror::DexCache::GetNativePairPtrSize(array, index, kPointerSize);
    if (pair.object != nullptr) {
      pair.object = native_visitor_(pair.object);
      mirror::DexCache::SetNativePairPtrSize(array, index, pair, kPointerSize);
    }
  }

  void FixupDexCacheArrayEntry(GcRoot<mirror::CallSite>* array, uint32_t index)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    PatchGcRoot(&array[index]);
  }

  void FixupDexCacheArrayEntry(GcRoot<mirror::String>* array, uint32_t index)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    PatchGcRoot(&array[index]);
  }

  template <typename EntryType>
  void FixupDexCacheArray(ObjPtr<mirror::DexCache> dex_cache,
                          MemberOffset array_offset,
                          uint32_t size) REQUIRES_SHARED(Locks::mutator_lock_) {
    EntryType* old_array =
        reinterpret_cast64<EntryType*>(dex_cache->GetField64<kVerifyNone>(array_offset));
    DCHECK_EQ(old_array != nullptr, size != 0u);
    if (old_array != nullptr) {
      EntryType* new_array = native_visitor_(old_array);
      dex_cache->SetField64<kVerifyNone>(array_offset, reinterpret_cast64<uint64_t>(new_array));
      for (uint32_t i = 0; i != size; ++i) {
        FixupDexCacheArrayEntry(new_array, i);
      }
    }
  }

 private:
  // Heap objects visitor.
  HeapVisitor heap_visitor_;

  // Native objects visitor.
  NativeVisitor native_visitor_;
};

template <typename ReferenceVisitor>
class ImageSpace::ClassTableVisitor final {
 public:
  explicit ClassTableVisitor(const ReferenceVisitor& reference_visitor)
      : reference_visitor_(reference_visitor) {}

  void VisitRoot(mirror::CompressedReference<mirror::Object>* root) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK(root->AsMirrorPtr() != nullptr);
    root->Assign(reference_visitor_(root->AsMirrorPtr()));
  }

 private:
  ReferenceVisitor reference_visitor_;
};

// Helper class encapsulating loading, so we can access private ImageSpace members (this is a
// nested class), but not declare functions in the header.
class ImageSpace::Loader {
 public:
  static std::unique_ptr<ImageSpace> InitAppImage(const char* image_filename,
                                                  const char* image_location,
                                                  const OatFile* oat_file,
                                                  /*inout*/MemMap* image_reservation,
                                                  /*out*/std::string* error_msg)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    TimingLogger logger(__PRETTY_FUNCTION__, /*precise=*/ true, VLOG_IS_ON(image));

    std::unique_ptr<ImageSpace> space = Init(image_filename,
                                             image_location,
                                             oat_file,
                                             &logger,
                                             image_reservation,
                                             error_msg);
    if (space != nullptr) {
      uint32_t expected_reservation_size =
          RoundUp(space->GetImageHeader().GetImageSize(), kPageSize);
      if (!CheckImageReservationSize(*space, expected_reservation_size, error_msg) ||
          !CheckImageComponentCount(*space, /*expected_component_count=*/ 1u, error_msg)) {
        return nullptr;
      }

      TimingLogger::ScopedTiming timing("RelocateImage", &logger);
      ImageHeader* image_header = reinterpret_cast<ImageHeader*>(space->GetMemMap()->Begin());
      const PointerSize pointer_size = image_header->GetPointerSize();
      bool result;
      if (pointer_size == PointerSize::k64) {
        result = RelocateInPlace<PointerSize::k64>(*image_header,
                                                   space->GetMemMap()->Begin(),
                                                   space->GetLiveBitmap(),
                                                   oat_file,
                                                   error_msg);
      } else {
        result = RelocateInPlace<PointerSize::k32>(*image_header,
                                                   space->GetMemMap()->Begin(),
                                                   space->GetLiveBitmap(),
                                                   oat_file,
                                                   error_msg);
      }
      if (!result) {
        return nullptr;
      }
      Runtime* runtime = Runtime::Current();
      CHECK_EQ(runtime->GetResolutionMethod(),
               image_header->GetImageMethod(ImageHeader::kResolutionMethod));
      CHECK_EQ(runtime->GetImtConflictMethod(),
               image_header->GetImageMethod(ImageHeader::kImtConflictMethod));
      CHECK_EQ(runtime->GetImtUnimplementedMethod(),
               image_header->GetImageMethod(ImageHeader::kImtUnimplementedMethod));
      CHECK_EQ(runtime->GetCalleeSaveMethod(CalleeSaveType::kSaveAllCalleeSaves),
               image_header->GetImageMethod(ImageHeader::kSaveAllCalleeSavesMethod));
      CHECK_EQ(runtime->GetCalleeSaveMethod(CalleeSaveType::kSaveRefsOnly),
               image_header->GetImageMethod(ImageHeader::kSaveRefsOnlyMethod));
      CHECK_EQ(runtime->GetCalleeSaveMethod(CalleeSaveType::kSaveRefsAndArgs),
               image_header->GetImageMethod(ImageHeader::kSaveRefsAndArgsMethod));
      CHECK_EQ(runtime->GetCalleeSaveMethod(CalleeSaveType::kSaveEverything),
               image_header->GetImageMethod(ImageHeader::kSaveEverythingMethod));
      CHECK_EQ(runtime->GetCalleeSaveMethod(CalleeSaveType::kSaveEverythingForClinit),
               image_header->GetImageMethod(ImageHeader::kSaveEverythingMethodForClinit));
      CHECK_EQ(runtime->GetCalleeSaveMethod(CalleeSaveType::kSaveEverythingForSuspendCheck),
               image_header->GetImageMethod(ImageHeader::kSaveEverythingMethodForSuspendCheck));

      VLOG(image) << "ImageSpace::Loader::InitAppImage exiting " << *space.get();
    }
    if (VLOG_IS_ON(image)) {
      logger.Dump(LOG_STREAM(INFO));
    }
    return space;
  }

  static std::unique_ptr<ImageSpace> Init(const char* image_filename,
                                          const char* image_location,
                                          const OatFile* oat_file,
                                          TimingLogger* logger,
                                          /*inout*/MemMap* image_reservation,
                                          /*out*/std::string* error_msg)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    CHECK(image_filename != nullptr);
    CHECK(image_location != nullptr);

    VLOG(image) << "ImageSpace::Init entering image_filename=" << image_filename;

    std::unique_ptr<File> file;
    {
      TimingLogger::ScopedTiming timing("OpenImageFile", logger);
      file.reset(OS::OpenFileForReading(image_filename));
      if (file == nullptr) {
        *error_msg = StringPrintf("Failed to open '%s'", image_filename);
        return nullptr;
      }
    }
    ImageHeader temp_image_header;
    ImageHeader* image_header = &temp_image_header;
    {
      TimingLogger::ScopedTiming timing("ReadImageHeader", logger);
      bool success = file->ReadFully(image_header, sizeof(*image_header));
      if (!success || !image_header->IsValid()) {
        *error_msg = StringPrintf("Invalid image header in '%s'", image_filename);
        return nullptr;
      }
    }
    // Check that the file is larger or equal to the header size + data size.
    const uint64_t image_file_size = static_cast<uint64_t>(file->GetLength());
    if (image_file_size < sizeof(ImageHeader) + image_header->GetDataSize()) {
      *error_msg = StringPrintf(
          "Image file truncated: %" PRIu64 " vs. %" PRIu64 ".",
           image_file_size,
           static_cast<uint64_t>(sizeof(ImageHeader) + image_header->GetDataSize()));
      return nullptr;
    }

    if (oat_file != nullptr) {
      // If we have an oat file (i.e. for app image), check the oat file checksum.
      // Otherwise, we open the oat file after the image and check the checksum there.
      const uint32_t oat_checksum = oat_file->GetOatHeader().GetChecksum();
      const uint32_t image_oat_checksum = image_header->GetOatChecksum();
      if (oat_checksum != image_oat_checksum) {
        *error_msg = StringPrintf("Oat checksum 0x%x does not match the image one 0x%x in image %s",
                                  oat_checksum,
                                  image_oat_checksum,
                                  image_filename);
        return nullptr;
      }
    }

    if (VLOG_IS_ON(startup)) {
      LOG(INFO) << "Dumping image sections";
      for (size_t i = 0; i < ImageHeader::kSectionCount; ++i) {
        const auto section_idx = static_cast<ImageHeader::ImageSections>(i);
        auto& section = image_header->GetImageSection(section_idx);
        LOG(INFO) << section_idx << " start="
            << reinterpret_cast<void*>(image_header->GetImageBegin() + section.Offset()) << " "
            << section;
      }
    }

    const auto& bitmap_section = image_header->GetImageBitmapSection();
    // The location we want to map from is the first aligned page after the end of the stored
    // (possibly compressed) data.
    const size_t image_bitmap_offset = RoundUp(sizeof(ImageHeader) + image_header->GetDataSize(),
                                               kPageSize);
    const size_t end_of_bitmap = image_bitmap_offset + bitmap_section.Size();
    if (end_of_bitmap != image_file_size) {
      *error_msg = StringPrintf(
          "Image file size does not equal end of bitmap: size=%" PRIu64 " vs. %zu.",
          image_file_size,
          end_of_bitmap);
      return nullptr;
    }

    // GetImageBegin is the preferred address to map the image. If we manage to map the
    // image at the image begin, the amount of fixup work required is minimized.
    // If it is pic we will retry with error_msg for the2 failure case. Pass a null error_msg to
    // avoid reading proc maps for a mapping failure and slowing everything down.
    // For the boot image, we have already reserved the memory and we load the image
    // into the `image_reservation`.
    MemMap map = LoadImageFile(
        image_filename,
        image_location,
        *image_header,
        file->Fd(),
        logger,
        image_reservation,
        error_msg);
    if (!map.IsValid()) {
      DCHECK(!error_msg->empty());
      return nullptr;
    }
    DCHECK_EQ(0, memcmp(image_header, map.Begin(), sizeof(ImageHeader)));

    MemMap image_bitmap_map = MemMap::MapFile(bitmap_section.Size(),
                                              PROT_READ,
                                              MAP_PRIVATE,
                                              file->Fd(),
                                              image_bitmap_offset,
                                              /*low_4gb=*/ false,
                                              image_filename,
                                              error_msg);
    if (!image_bitmap_map.IsValid()) {
      *error_msg = StringPrintf("Failed to map image bitmap: %s", error_msg->c_str());
      return nullptr;
    }
    // Loaded the map, use the image header from the file now in case we patch it with
    // RelocateInPlace.
    image_header = reinterpret_cast<ImageHeader*>(map.Begin());
    const uint32_t bitmap_index = ImageSpace::bitmap_index_.fetch_add(1);
    std::string bitmap_name(StringPrintf("imagespace %s live-bitmap %u",
                                         image_filename,
                                         bitmap_index));
    // Bitmap only needs to cover until the end of the mirror objects section.
    const ImageSection& image_objects = image_header->GetObjectsSection();
    // We only want the mirror object, not the ArtFields and ArtMethods.
    uint8_t* const image_end = map.Begin() + image_objects.End();
    std::unique_ptr<accounting::ContinuousSpaceBitmap> bitmap;
    {
      TimingLogger::ScopedTiming timing("CreateImageBitmap", logger);
      bitmap.reset(
          accounting::ContinuousSpaceBitmap::CreateFromMemMap(
              bitmap_name,
              std::move(image_bitmap_map),
              reinterpret_cast<uint8_t*>(map.Begin()),
              // Make sure the bitmap is aligned to card size instead of just bitmap word size.
              RoundUp(image_objects.End(), gc::accounting::CardTable::kCardSize)));
      if (bitmap == nullptr) {
        *error_msg = StringPrintf("Could not create bitmap '%s'", bitmap_name.c_str());
        return nullptr;
      }
    }
    // We only want the mirror object, not the ArtFields and ArtMethods.
    std::unique_ptr<ImageSpace> space(new ImageSpace(image_filename,
                                                     image_location,
                                                     std::move(map),
                                                     std::move(bitmap),
                                                     image_end));
    space->oat_file_non_owned_ = oat_file;
    return space;
  }

  static bool CheckImageComponentCount(const ImageSpace& space,
                                       uint32_t expected_component_count,
                                       /*out*/std::string* error_msg) {
    const ImageHeader& header = space.GetImageHeader();
    if (header.GetComponentCount() != expected_component_count) {
      *error_msg = StringPrintf("Unexpected component count in %s, received %u, expected %u",
                                space.GetImageFilename().c_str(),
                                header.GetComponentCount(),
                                expected_component_count);
      return false;
    }
    return true;
  }

  static bool CheckImageReservationSize(const ImageSpace& space,
                                        uint32_t expected_reservation_size,
                                        /*out*/std::string* error_msg) {
    const ImageHeader& header = space.GetImageHeader();
    if (header.GetImageReservationSize() != expected_reservation_size) {
      *error_msg = StringPrintf("Unexpected reservation size in %s, received %u, expected %u",
                                space.GetImageFilename().c_str(),
                                header.GetImageReservationSize(),
                                expected_reservation_size);
      return false;
    }
    return true;
  }

 private:
  static MemMap LoadImageFile(const char* image_filename,
                              const char* image_location,
                              const ImageHeader& image_header,
                              int fd,
                              TimingLogger* logger,
                              /*inout*/MemMap* image_reservation,
                              /*out*/std::string* error_msg)
        REQUIRES_SHARED(Locks::mutator_lock_) {
    TimingLogger::ScopedTiming timing("MapImageFile", logger);
    std::string temp_error_msg;
    const bool is_compressed = image_header.HasCompressedBlock();
    if (!is_compressed) {
      uint8_t* address = (image_reservation != nullptr) ? image_reservation->Begin() : nullptr;
      return MemMap::MapFileAtAddress(address,
                                      image_header.GetImageSize(),
                                      PROT_READ | PROT_WRITE,
                                      MAP_PRIVATE,
                                      fd,
                                      /*start=*/ 0,
                                      /*low_4gb=*/ true,
                                      image_filename,
                                      /*reuse=*/ false,
                                      image_reservation,
                                      error_msg);
    }

    // Reserve output and decompress into it.
    MemMap map = MemMap::MapAnonymous(image_location,
                                      image_header.GetImageSize(),
                                      PROT_READ | PROT_WRITE,
                                      /*low_4gb=*/ true,
                                      image_reservation,
                                      error_msg);
    if (map.IsValid()) {
      const size_t stored_size = image_header.GetDataSize();
      MemMap temp_map = MemMap::MapFile(sizeof(ImageHeader) + stored_size,
                                        PROT_READ,
                                        MAP_PRIVATE,
                                        fd,
                                        /*start=*/ 0,
                                        /*low_4gb=*/ false,
                                        image_filename,
                                        error_msg);
      if (!temp_map.IsValid()) {
        DCHECK(error_msg == nullptr || !error_msg->empty());
        return MemMap::Invalid();
      }
      memcpy(map.Begin(), &image_header, sizeof(ImageHeader));

      Runtime::ScopedThreadPoolUsage stpu;
      ThreadPool* const pool = stpu.GetThreadPool();
      const uint64_t start = NanoTime();
      Thread* const self = Thread::Current();
      static constexpr size_t kMinBlocks = 2u;
      const bool use_parallel = pool != nullptr && image_header.GetBlockCount() >= kMinBlocks;
      for (const ImageHeader::Block& block : image_header.GetBlocks(temp_map.Begin())) {
        auto function = [&](Thread*) {
          const uint64_t start2 = NanoTime();
          ScopedTrace trace("LZ4 decompress block");
          bool result = block.Decompress(/*out_ptr=*/map.Begin(),
                                         /*in_ptr=*/temp_map.Begin(),
                                         error_msg);
          if (!result && error_msg != nullptr) {
            *error_msg = "Failed to decompress image block " + *error_msg;
          }
          VLOG(image) << "Decompress block " << block.GetDataSize() << " -> "
                      << block.GetImageSize() << " in " << PrettyDuration(NanoTime() - start2);
        };
        if (use_parallel) {
          pool->AddTask(self, new FunctionTask(std::move(function)));
        } else {
          function(self);
        }
      }
      if (use_parallel) {
        ScopedTrace trace("Waiting for workers");
        // Go to native since we don't want to suspend while holding the mutator lock.
        ScopedThreadSuspension sts(Thread::Current(), kNative);
        pool->Wait(self, true, false);
      }
      const uint64_t time = NanoTime() - start;
      // Add one 1 ns to prevent possible divide by 0.
      VLOG(image) << "Decompressing image took " << PrettyDuration(time) << " ("
                  << PrettySize(static_cast<uint64_t>(map.Size()) * MsToNs(1000) / (time + 1))
                  << "/s)";
    }

    return map;
  }

  class EmptyRange {
   public:
    ALWAYS_INLINE bool InSource(uintptr_t) const { return false; }
    ALWAYS_INLINE bool InDest(uintptr_t) const { return false; }
    ALWAYS_INLINE uintptr_t ToDest(uintptr_t) const { UNREACHABLE(); }
  };

  template <typename Range0, typename Range1 = EmptyRange, typename Range2 = EmptyRange>
  class ForwardAddress {
   public:
    ForwardAddress(const Range0& range0 = Range0(),
                   const Range1& range1 = Range1(),
                   const Range2& range2 = Range2())
        : range0_(range0), range1_(range1), range2_(range2) {}

    // Return the relocated address of a heap object.
    // Null checks must be performed in the caller (for performance reasons).
    template <typename T>
    ALWAYS_INLINE T* operator()(T* src) const {
      DCHECK(src != nullptr);
      const uintptr_t uint_src = reinterpret_cast<uintptr_t>(src);
      if (range2_.InSource(uint_src)) {
        return reinterpret_cast<T*>(range2_.ToDest(uint_src));
      }
      if (range1_.InSource(uint_src)) {
        return reinterpret_cast<T*>(range1_.ToDest(uint_src));
      }
      CHECK(range0_.InSource(uint_src))
          << reinterpret_cast<const void*>(src) << " not in "
          << reinterpret_cast<const void*>(range0_.Source()) << "-"
          << reinterpret_cast<const void*>(range0_.Source() + range0_.Length());
      return reinterpret_cast<T*>(range0_.ToDest(uint_src));
    }

   private:
    const Range0 range0_;
    const Range1 range1_;
    const Range2 range2_;
  };

  template <typename Forward>
  class FixupRootVisitor {
   public:
    template<typename... Args>
    explicit FixupRootVisitor(Args... args) : forward_(args...) {}

    ALWAYS_INLINE void VisitRootIfNonNull(mirror::CompressedReference<mirror::Object>* root) const
        REQUIRES_SHARED(Locks::mutator_lock_) {
      if (!root->IsNull()) {
        VisitRoot(root);
      }
    }

    ALWAYS_INLINE void VisitRoot(mirror::CompressedReference<mirror::Object>* root) const
        REQUIRES_SHARED(Locks::mutator_lock_) {
      mirror::Object* ref = root->AsMirrorPtr();
      mirror::Object* new_ref = forward_(ref);
      if (ref != new_ref) {
        root->Assign(new_ref);
      }
    }

   private:
    Forward forward_;
  };

  template <typename Forward>
  class FixupObjectVisitor {
   public:
    explicit FixupObjectVisitor(gc::accounting::ContinuousSpaceBitmap* visited,
                                const Forward& forward)
        : visited_(visited), forward_(forward) {}

    // Fix up separately since we also need to fix up method entrypoints.
    ALWAYS_INLINE void VisitRootIfNonNull(
        mirror::CompressedReference<mirror::Object>* root ATTRIBUTE_UNUSED) const {}

    ALWAYS_INLINE void VisitRoot(mirror::CompressedReference<mirror::Object>* root ATTRIBUTE_UNUSED)
        const {}

    ALWAYS_INLINE void operator()(ObjPtr<mirror::Object> obj,
                                  MemberOffset offset,
                                  bool is_static ATTRIBUTE_UNUSED) const
        NO_THREAD_SAFETY_ANALYSIS {
      // Space is not yet added to the heap, don't do a read barrier.
      mirror::Object* ref = obj->GetFieldObject<mirror::Object, kVerifyNone, kWithoutReadBarrier>(
          offset);
      if (ref != nullptr) {
        // Use SetFieldObjectWithoutWriteBarrier to avoid card marking since we are writing to the
        // image.
        obj->SetFieldObjectWithoutWriteBarrier<false, true, kVerifyNone>(offset, forward_(ref));
      }
    }

    // java.lang.ref.Reference visitor.
    void operator()(ObjPtr<mirror::Class> klass ATTRIBUTE_UNUSED,
                    ObjPtr<mirror::Reference> ref) const
        REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(Locks::heap_bitmap_lock_) {
      mirror::Object* obj = ref->GetReferent<kWithoutReadBarrier>();
      if (obj != nullptr) {
        ref->SetFieldObjectWithoutWriteBarrier<false, true, kVerifyNone>(
            mirror::Reference::ReferentOffset(),
            forward_(obj));
      }
    }

    void operator()(mirror::Object* obj) const
        NO_THREAD_SAFETY_ANALYSIS {
      if (!visited_->Set(obj)) {
        // Not already visited.
        obj->VisitReferences</*visit native roots*/false, kVerifyNone, kWithoutReadBarrier>(
            *this,
            *this);
        CHECK(!obj->IsClass());
      }
    }

   private:
    gc::accounting::ContinuousSpaceBitmap* const visited_;
    Forward forward_;
  };

  // Relocate an image space mapped at target_base which possibly used to be at a different base
  // address. In place means modifying a single ImageSpace in place rather than relocating from
  // one ImageSpace to another.
  template <PointerSize kPointerSize>
  static bool RelocateInPlace(ImageHeader& image_header,
                              uint8_t* target_base,
                              accounting::ContinuousSpaceBitmap* bitmap,
                              const OatFile* app_oat_file,
                              std::string* error_msg) {
    DCHECK(error_msg != nullptr);
    // Set up sections.
    uint32_t boot_image_begin = 0;
    uint32_t boot_image_end = 0;
    uint32_t boot_oat_begin = 0;
    uint32_t boot_oat_end = 0;
    gc::Heap* const heap = Runtime::Current()->GetHeap();
    heap->GetBootImagesSize(&boot_image_begin, &boot_image_end, &boot_oat_begin, &boot_oat_end);
    if (boot_image_begin == boot_image_end) {
      *error_msg = "Can not relocate app image without boot image space";
      return false;
    }
    if (boot_oat_begin == boot_oat_end) {
      *error_msg = "Can not relocate app image without boot oat file";
      return false;
    }
    const uint32_t boot_image_size = boot_oat_end - boot_image_begin;
    const uint32_t image_header_boot_image_size = image_header.GetBootImageSize();
    if (boot_image_size != image_header_boot_image_size) {
      *error_msg = StringPrintf("Boot image size %" PRIu64 " does not match expected size %"
                                    PRIu64,
                                static_cast<uint64_t>(boot_image_size),
                                static_cast<uint64_t>(image_header_boot_image_size));
      return false;
    }
    const ImageSection& objects_section = image_header.GetObjectsSection();
    // Where the app image objects are mapped to.
    uint8_t* objects_location = target_base + objects_section.Offset();
    TimingLogger logger(__FUNCTION__, true, false);
    RelocationRange boot_image(image_header.GetBootImageBegin(),
                               boot_image_begin,
                               boot_image_size);
    // Metadata is everything after the objects section, use exclusion to be safe.
    RelocationRange app_image_metadata(
        reinterpret_cast<uintptr_t>(image_header.GetImageBegin()) + objects_section.End(),
        reinterpret_cast<uintptr_t>(target_base) + objects_section.End(),
        image_header.GetImageSize() - objects_section.End());
    // App image heap objects, may be mapped in the heap.
    RelocationRange app_image_objects(
        reinterpret_cast<uintptr_t>(image_header.GetImageBegin()) + objects_section.Offset(),
        reinterpret_cast<uintptr_t>(objects_location),
        objects_section.Size());
    // Use the oat data section since this is where the OatFile::Begin is.
    RelocationRange app_oat(reinterpret_cast<uintptr_t>(image_header.GetOatDataBegin()),
                            // Not necessarily in low 4GB.
                            reinterpret_cast<uintptr_t>(app_oat_file->Begin()),
                            image_header.GetOatDataEnd() - image_header.GetOatDataBegin());
    VLOG(image) << "App image metadata " << app_image_metadata;
    VLOG(image) << "App image objects " << app_image_objects;
    VLOG(image) << "App oat " << app_oat;
    VLOG(image) << "Boot image " << boot_image;
    // True if we need to fixup any heap pointers.
    const bool fixup_image = boot_image.Delta() != 0 || app_image_metadata.Delta() != 0 ||
        app_image_objects.Delta() != 0;
    if (!fixup_image) {
      // Nothing to fix up.
      return true;
    }
    ScopedDebugDisallowReadBarriers sddrb(Thread::Current());

    using ForwardObject = ForwardAddress<RelocationRange, RelocationRange>;
    ForwardObject forward_object(boot_image, app_image_objects);
    ForwardObject forward_metadata(boot_image, app_image_metadata);
    using ForwardCode = ForwardAddress<RelocationRange, RelocationRange>;
    ForwardCode forward_code(boot_image, app_oat);
    PatchObjectVisitor<kPointerSize, ForwardObject, ForwardCode> patch_object_visitor(
        forward_object,
        forward_metadata);
    if (fixup_image) {
      // Two pass approach, fix up all classes first, then fix up non class-objects.
      // The visited bitmap is used to ensure that pointer arrays are not forwarded twice.
      std::unique_ptr<gc::accounting::ContinuousSpaceBitmap> visited_bitmap(
          gc::accounting::ContinuousSpaceBitmap::Create("Relocate bitmap",
                                                        target_base,
                                                        image_header.GetImageSize()));
      {
        TimingLogger::ScopedTiming timing("Fixup classes", &logger);
        const auto& class_table_section = image_header.GetClassTableSection();
        if (class_table_section.Size() > 0u) {
          ScopedObjectAccess soa(Thread::Current());
          ClassTableVisitor class_table_visitor(forward_object);
          size_t read_count = 0u;
          const uint8_t* data = target_base + class_table_section.Offset();
          // We avoid making a copy of the data since we want modifications to be propagated to the
          // memory map.
          ClassTable::ClassSet temp_set(data, /*make_copy_of_data=*/ false, &read_count);
          for (ClassTable::TableSlot& slot : temp_set) {
            slot.VisitRoot(class_table_visitor);
            ObjPtr<mirror::Class> klass = slot.Read<kWithoutReadBarrier>();
            if (!app_image_objects.InDest(klass.Ptr())) {
              continue;
            }
            const bool already_marked = visited_bitmap->Set(klass.Ptr());
            CHECK(!already_marked) << "App image class already visited";
            patch_object_visitor.VisitClass(klass.Ptr());
            // Then patch the non-embedded vtable and iftable.
            ObjPtr<mirror::PointerArray> vtable =
                klass->GetVTable<kVerifyNone, kWithoutReadBarrier>();
            if (vtable != nullptr &&
                app_image_objects.InDest(vtable.Ptr()) &&
                !visited_bitmap->Set(vtable.Ptr())) {
              patch_object_visitor.VisitPointerArray(vtable);
            }
            ObjPtr<mirror::IfTable> iftable = klass->GetIfTable<kVerifyNone, kWithoutReadBarrier>();
            if (iftable != nullptr && app_image_objects.InDest(iftable.Ptr())) {
              // Avoid processing the fields of iftable since we will process them later anyways
              // below.
              int32_t ifcount = klass->GetIfTableCount<kVerifyNone>();
              for (int32_t i = 0; i != ifcount; ++i) {
                ObjPtr<mirror::PointerArray> unpatched_ifarray =
                    iftable->GetMethodArrayOrNull<kVerifyNone, kWithoutReadBarrier>(i);
                if (unpatched_ifarray != nullptr) {
                  // The iftable has not been patched, so we need to explicitly adjust the pointer.
                  ObjPtr<mirror::PointerArray> ifarray = forward_object(unpatched_ifarray.Ptr());
                  if (app_image_objects.InDest(ifarray.Ptr()) &&
                      !visited_bitmap->Set(ifarray.Ptr())) {
                    patch_object_visitor.VisitPointerArray(ifarray);
                  }
                }
              }
            }
          }
        }
      }

      // Fixup objects may read fields in the boot image, use the mutator lock here for sanity.
      // Though its probably not required.
      TimingLogger::ScopedTiming timing("Fixup objects", &logger);
      ScopedObjectAccess soa(Thread::Current());
      // Need to update the image to be at the target base.
      uintptr_t objects_begin = reinterpret_cast<uintptr_t>(target_base + objects_section.Offset());
      uintptr_t objects_end = reinterpret_cast<uintptr_t>(target_base + objects_section.End());
      FixupObjectVisitor<ForwardObject> fixup_object_visitor(visited_bitmap.get(), forward_object);
      bitmap->VisitMarkedRange(objects_begin, objects_end, fixup_object_visitor);
      // Fixup image roots.
      CHECK(app_image_objects.InSource(reinterpret_cast<uintptr_t>(
          image_header.GetImageRoots<kWithoutReadBarrier>().Ptr())));
      image_header.RelocateImageObjects(app_image_objects.Delta());
      CHECK_EQ(image_header.GetImageBegin(), target_base);
      // Fix up dex cache DexFile pointers.
      ObjPtr<mirror::ObjectArray<mirror::DexCache>> dex_caches =
          image_header.GetImageRoot<kWithoutReadBarrier>(ImageHeader::kDexCaches)
              ->AsObjectArray<mirror::DexCache, kVerifyNone>();
      for (int32_t i = 0, count = dex_caches->GetLength(); i < count; ++i) {
        ObjPtr<mirror::DexCache> dex_cache = dex_caches->Get<kVerifyNone, kWithoutReadBarrier>(i);
        CHECK(dex_cache != nullptr);
        patch_object_visitor.VisitDexCacheArrays(dex_cache);
      }
    }
    {
      // Only touches objects in the app image, no need for mutator lock.
      TimingLogger::ScopedTiming timing("Fixup methods", &logger);
      image_header.VisitPackedArtMethods([&](ArtMethod& method) NO_THREAD_SAFETY_ANALYSIS {
        // TODO: Consider a separate visitor for runtime vs normal methods.
        if (UNLIKELY(method.IsRuntimeMethod())) {
          ImtConflictTable* table = method.GetImtConflictTable(kPointerSize);
          if (table != nullptr) {
            ImtConflictTable* new_table = forward_metadata(table);
            if (table != new_table) {
              method.SetImtConflictTable(new_table, kPointerSize);
            }
          }
          const void* old_code = method.GetEntryPointFromQuickCompiledCodePtrSize(kPointerSize);
          const void* new_code = forward_code(old_code);
          if (old_code != new_code) {
            method.SetEntryPointFromQuickCompiledCodePtrSize(new_code, kPointerSize);
          }
        } else {
          patch_object_visitor.PatchGcRoot(&method.DeclaringClassRoot());
          method.UpdateEntrypoints(forward_code, kPointerSize);
        }
      }, target_base, kPointerSize);
    }
    if (fixup_image) {
      {
        // Only touches objects in the app image, no need for mutator lock.
        TimingLogger::ScopedTiming timing("Fixup fields", &logger);
        image_header.VisitPackedArtFields([&](ArtField& field) NO_THREAD_SAFETY_ANALYSIS {
          patch_object_visitor.template PatchGcRoot</*kMayBeNull=*/ false>(
              &field.DeclaringClassRoot());
        }, target_base);
      }
      {
        TimingLogger::ScopedTiming timing("Fixup imt", &logger);
        image_header.VisitPackedImTables(forward_metadata, target_base, kPointerSize);
      }
      {
        TimingLogger::ScopedTiming timing("Fixup conflict tables", &logger);
        image_header.VisitPackedImtConflictTables(forward_metadata, target_base, kPointerSize);
      }
      // In the app image case, the image methods are actually in the boot image.
      image_header.RelocateImageMethods(boot_image.Delta());
      // Fix up the intern table.
      const auto& intern_table_section = image_header.GetInternedStringsSection();
      if (intern_table_section.Size() > 0u) {
        TimingLogger::ScopedTiming timing("Fixup intern table", &logger);
        ScopedObjectAccess soa(Thread::Current());
        // Fixup the pointers in the newly written intern table to contain image addresses.
        InternTable temp_intern_table;
        // Note that we require that ReadFromMemory does not make an internal copy of the elements
        // so that the VisitRoots() will update the memory directly rather than the copies.
        temp_intern_table.AddTableFromMemory(target_base + intern_table_section.Offset(),
                                             [&](InternTable::UnorderedSet& strings)
            REQUIRES_SHARED(Locks::mutator_lock_) {
          for (GcRoot<mirror::String>& root : strings) {
            root = GcRoot<mirror::String>(forward_object(root.Read<kWithoutReadBarrier>()));
          }
        }, /*is_boot_image=*/ false);
      }
    }
    if (VLOG_IS_ON(image)) {
      logger.Dump(LOG_STREAM(INFO));
    }
    return true;
  }
};

class ImageSpace::BootImageLoader {
 public:
  BootImageLoader(const std::vector<std::string>& boot_class_path,
                  const std::vector<std::string>& boot_class_path_locations,
                  const std::string& image_location,
                  InstructionSet image_isa,
                  bool relocate,
                  bool executable,
                  bool is_zygote)
      : boot_class_path_(boot_class_path),
        boot_class_path_locations_(boot_class_path_locations),
        image_location_(image_location),
        image_isa_(image_isa),
        relocate_(relocate),
        executable_(executable),
        is_zygote_(is_zygote),
        has_system_(false),
        has_cache_(false),
        is_global_cache_(true),
        dalvik_cache_exists_(false),
        dalvik_cache_(),
        cache_filename_() {
  }

  bool IsZygote() const { return is_zygote_; }

  void FindImageFiles() {
    std::string system_filename;
    bool found_image = FindImageFilenameImpl(image_location_.c_str(),
                                             image_isa_,
                                             &has_system_,
                                             &system_filename,
                                             &dalvik_cache_exists_,
                                             &dalvik_cache_,
                                             &is_global_cache_,
                                             &has_cache_,
                                             &cache_filename_);
    DCHECK(!dalvik_cache_exists_ || !dalvik_cache_.empty());
    DCHECK_EQ(found_image, has_system_ || has_cache_);
  }

  bool HasSystem() const { return has_system_; }
  bool HasCache() const { return has_cache_; }

  bool DalvikCacheExists() const { return dalvik_cache_exists_; }
  bool IsGlobalCache() const { return is_global_cache_; }

  const std::string& GetDalvikCache() const {
    return dalvik_cache_;
  }

  const std::string& GetCacheFilename() const {
    return cache_filename_;
  }

  bool LoadFromSystem(bool validate_oat_file,
                      size_t extra_reservation_size,
                      /*out*/std::vector<std::unique_ptr<space::ImageSpace>>* boot_image_spaces,
                      /*out*/MemMap* extra_reservation,
                      /*out*/std::string* error_msg) REQUIRES_SHARED(Locks::mutator_lock_) {
    TimingLogger logger(__PRETTY_FUNCTION__, /*precise=*/ true, VLOG_IS_ON(image));
    std::string filename = GetSystemImageFilename(image_location_.c_str(), image_isa_);

    if (!LoadFromFile(filename,
                      validate_oat_file,
                      extra_reservation_size,
                      &logger,
                      boot_image_spaces,
                      extra_reservation,
                      error_msg)) {
      return false;
    }

    if (VLOG_IS_ON(image)) {
      LOG(INFO) << "ImageSpace::BootImageLoader::LoadFromSystem exiting "
          << boot_image_spaces->front();
      logger.Dump(LOG_STREAM(INFO));
    }
    return true;
  }

  bool LoadFromDalvikCache(
      bool validate_oat_file,
      size_t extra_reservation_size,
      /*out*/std::vector<std::unique_ptr<space::ImageSpace>>* boot_image_spaces,
      /*out*/MemMap* extra_reservation,
      /*out*/std::string* error_msg) REQUIRES_SHARED(Locks::mutator_lock_) {
    TimingLogger logger(__PRETTY_FUNCTION__, /*precise=*/ true, VLOG_IS_ON(image));
    DCHECK(DalvikCacheExists());

    if (!LoadFromFile(cache_filename_,
                      validate_oat_file,
                      extra_reservation_size,
                      &logger,
                      boot_image_spaces,
                      extra_reservation,
                      error_msg)) {
      return false;
    }

    if (VLOG_IS_ON(image)) {
      LOG(INFO) << "ImageSpace::BootImageLoader::LoadFromDalvikCache exiting "
          << boot_image_spaces->front();
      logger.Dump(LOG_STREAM(INFO));
    }
    return true;
  }

 private:
  bool LoadFromFile(
      const std::string& filename,
      bool validate_oat_file,
      size_t extra_reservation_size,
      TimingLogger* logger,
      /*out*/std::vector<std::unique_ptr<space::ImageSpace>>* boot_image_spaces,
      /*out*/MemMap* extra_reservation,
      /*out*/std::string* error_msg) REQUIRES_SHARED(Locks::mutator_lock_) {
    ImageHeader system_hdr;
    if (!ReadSpecificImageHeader(filename.c_str(), &system_hdr)) {
      *error_msg = StringPrintf("Cannot read header of %s", filename.c_str());
      return false;
    }
    if (system_hdr.GetComponentCount() == 0u ||
        system_hdr.GetComponentCount() > boot_class_path_.size()) {
      *error_msg = StringPrintf("Unexpected component count in %s, received %u, "
                                    "expected non-zero and <= %zu",
                                filename.c_str(),
                                system_hdr.GetComponentCount(),
                                boot_class_path_.size());
      return false;
    }
    MemMap image_reservation;
    MemMap local_extra_reservation;
    if (!ReserveBootImageMemory(system_hdr.GetImageReservationSize(),
                                reinterpret_cast32<uint32_t>(system_hdr.GetImageBegin()),
                                extra_reservation_size,
                                &image_reservation,
                                &local_extra_reservation,
                                error_msg)) {
      return false;
    }

    ArrayRef<const std::string> provided_locations(boot_class_path_locations_.data(),
                                                   system_hdr.GetComponentCount());
    std::vector<std::string> locations =
        ExpandMultiImageLocations(provided_locations, image_location_);
    std::vector<std::string> filenames =
        ExpandMultiImageLocations(provided_locations, filename);
    DCHECK_EQ(locations.size(), filenames.size());
    std::vector<std::unique_ptr<ImageSpace>> spaces;
    spaces.reserve(locations.size());
    for (std::size_t i = 0u, size = locations.size(); i != size; ++i) {
      spaces.push_back(Load(locations[i], filenames[i], logger, &image_reservation, error_msg));
      const ImageSpace* space = spaces.back().get();
      if (space == nullptr) {
        return false;
      }
      uint32_t expected_component_count = (i == 0u) ? system_hdr.GetComponentCount() : 0u;
      uint32_t expected_reservation_size = (i == 0u) ? system_hdr.GetImageReservationSize() : 0u;
      if (!Loader::CheckImageReservationSize(*space, expected_reservation_size, error_msg) ||
          !Loader::CheckImageComponentCount(*space, expected_component_count, error_msg)) {
        return false;
      }
    }
    for (size_t i = 0u, size = spaces.size(); i != size; ++i) {
      std::string expected_boot_class_path =
          (i == 0u) ? android::base::Join(provided_locations, ':') : std::string();
      if (!OpenOatFile(spaces[i].get(),
                       boot_class_path_[i],
                       expected_boot_class_path,
                       validate_oat_file,
                       logger,
                       &image_reservation,
                       error_msg)) {
        return false;
      }
    }
    if (!CheckReservationExhausted(image_reservation, error_msg)) {
      return false;
    }

    MaybeRelocateSpaces(spaces, logger);
    boot_image_spaces->swap(spaces);
    *extra_reservation = std::move(local_extra_reservation);
    return true;
  }

 private:
  class RelocateVisitor {
   public:
    explicit RelocateVisitor(uint32_t diff) : diff_(diff) {}

    template <typename T>
    ALWAYS_INLINE T* operator()(T* src) const {
      DCHECK(src != nullptr);
      return reinterpret_cast32<T*>(reinterpret_cast32<uint32_t>(src) + diff_);
    }

   private:
    const uint32_t diff_;
  };

  static void** PointerAddress(ArtMethod* method, MemberOffset offset) {
    return reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(method) + offset.Uint32Value());
  }

  template <PointerSize kPointerSize>
  static void DoRelocateSpaces(const std::vector<std::unique_ptr<ImageSpace>>& spaces,
                               uint32_t diff) REQUIRES_SHARED(Locks::mutator_lock_) {
    std::unique_ptr<gc::accounting::ContinuousSpaceBitmap> patched_objects(
        gc::accounting::ContinuousSpaceBitmap::Create(
            "Marked objects",
            spaces.front()->Begin(),
            spaces.back()->End() - spaces.front()->Begin()));
    using PatchRelocateVisitor = PatchObjectVisitor<kPointerSize, RelocateVisitor, RelocateVisitor>;
    RelocateVisitor relocate_visitor(diff);
    PatchRelocateVisitor patch_object_visitor(relocate_visitor, relocate_visitor);

    mirror::Class* dcheck_class_class = nullptr;  // Used only for a DCHECK().
    for (const std::unique_ptr<ImageSpace>& space : spaces) {
      // First patch the image header. The `diff` is OK for patching 32-bit fields but
      // the 64-bit method fields in the ImageHeader may need a negative `delta`.
      reinterpret_cast<ImageHeader*>(space->Begin())->RelocateImage(
          (reinterpret_cast32<uint32_t>(space->Begin()) >= -diff)  // Would `begin+diff` overflow?
              ? -static_cast<int64_t>(-diff) : static_cast<int64_t>(diff));

      // Patch fields and methods.
      const ImageHeader& image_header = space->GetImageHeader();
      image_header.VisitPackedArtFields([&](ArtField& field) REQUIRES_SHARED(Locks::mutator_lock_) {
        patch_object_visitor.template PatchGcRoot</*kMayBeNull=*/ false>(
            &field.DeclaringClassRoot());
      }, space->Begin());
      image_header.VisitPackedArtMethods([&](ArtMethod& method)
          REQUIRES_SHARED(Locks::mutator_lock_) {
        patch_object_visitor.PatchGcRoot(&method.DeclaringClassRoot());
        void** data_address = PointerAddress(&method, ArtMethod::DataOffset(kPointerSize));
        patch_object_visitor.PatchNativePointer(data_address);
        void** entrypoint_address =
            PointerAddress(&method, ArtMethod::EntryPointFromQuickCompiledCodeOffset(kPointerSize));
        patch_object_visitor.PatchNativePointer(entrypoint_address);
      }, space->Begin(), kPointerSize);
      auto method_table_visitor = [&](ArtMethod* method) {
        DCHECK(method != nullptr);
        return relocate_visitor(method);
      };
      image_header.VisitPackedImTables(method_table_visitor, space->Begin(), kPointerSize);
      image_header.VisitPackedImtConflictTables(method_table_visitor, space->Begin(), kPointerSize);

      // Patch the intern table.
      if (image_header.GetInternedStringsSection().Size() != 0u) {
        const uint8_t* data = space->Begin() + image_header.GetInternedStringsSection().Offset();
        size_t read_count;
        InternTable::UnorderedSet temp_set(data, /*make_copy_of_data=*/ false, &read_count);
        for (GcRoot<mirror::String>& slot : temp_set) {
          patch_object_visitor.template PatchGcRoot</*kMayBeNull=*/ false>(&slot);
        }
      }

      // Patch the class table and classes, so that we can traverse class hierarchy to
      // determine the types of other objects when we visit them later.
      if (image_header.GetClassTableSection().Size() != 0u) {
        uint8_t* data = space->Begin() + image_header.GetClassTableSection().Offset();
        size_t read_count;
        ClassTable::ClassSet temp_set(data, /*make_copy_of_data=*/ false, &read_count);
        DCHECK(!temp_set.empty());
        ClassTableVisitor class_table_visitor(relocate_visitor);
        for (ClassTable::TableSlot& slot : temp_set) {
          slot.VisitRoot(class_table_visitor);
          ObjPtr<mirror::Class> klass = slot.Read<kWithoutReadBarrier>();
          DCHECK(klass != nullptr);
          patched_objects->Set(klass.Ptr());
          patch_object_visitor.VisitClass(klass.Ptr());
          if (kIsDebugBuild) {
            mirror::Class* class_class = klass->GetClass<kVerifyNone, kWithoutReadBarrier>();
            if (dcheck_class_class == nullptr) {
              dcheck_class_class = class_class;
            } else {
              CHECK_EQ(class_class, dcheck_class_class);
            }
          }
          // Then patch the non-embedded vtable and iftable.
          ObjPtr<mirror::PointerArray> vtable =
              klass->GetVTable<kVerifyNone, kWithoutReadBarrier>();
          if (vtable != nullptr && !patched_objects->Set(vtable.Ptr())) {
            patch_object_visitor.VisitPointerArray(vtable);
          }
          ObjPtr<mirror::IfTable> iftable = klass->GetIfTable<kVerifyNone, kWithoutReadBarrier>();
          if (iftable != nullptr) {
            int32_t ifcount = klass->GetIfTableCount<kVerifyNone>();
            for (int32_t i = 0; i != ifcount; ++i) {
              ObjPtr<mirror::PointerArray> unpatched_ifarray =
                  iftable->GetMethodArrayOrNull<kVerifyNone, kWithoutReadBarrier>(i);
              if (unpatched_ifarray != nullptr) {
                // The iftable has not been patched, so we need to explicitly adjust the pointer.
                ObjPtr<mirror::PointerArray> ifarray = relocate_visitor(unpatched_ifarray.Ptr());
                if (!patched_objects->Set(ifarray.Ptr())) {
                  patch_object_visitor.VisitPointerArray(ifarray);
                }
              }
            }
          }
        }
      }
    }

    // Patch class roots now, so that we can recognize mirror::Method and mirror::Constructor.
    ObjPtr<mirror::Class> method_class;
    ObjPtr<mirror::Class> constructor_class;
    {
      const ImageSpace* space = spaces.front().get();
      const ImageHeader& image_header = space->GetImageHeader();

      ObjPtr<mirror::ObjectArray<mirror::Object>> image_roots =
          image_header.GetImageRoots<kWithoutReadBarrier>();
      patched_objects->Set(image_roots.Ptr());
      patch_object_visitor.VisitObject(image_roots.Ptr());

      ObjPtr<mirror::ObjectArray<mirror::Class>> class_roots =
          ObjPtr<mirror::ObjectArray<mirror::Class>>::DownCast(
              image_header.GetImageRoot<kWithoutReadBarrier>(ImageHeader::kClassRoots));
      patched_objects->Set(class_roots.Ptr());
      patch_object_visitor.VisitObject(class_roots.Ptr());

      method_class = GetClassRoot<mirror::Method, kWithoutReadBarrier>(class_roots);
      constructor_class = GetClassRoot<mirror::Constructor, kWithoutReadBarrier>(class_roots);
    }

    for (size_t s = 0u, size = spaces.size(); s != size; ++s) {
      const ImageSpace* space = spaces[s].get();
      const ImageHeader& image_header = space->GetImageHeader();

      static_assert(IsAligned<kObjectAlignment>(sizeof(ImageHeader)), "Header alignment check");
      uint32_t objects_end = image_header.GetObjectsSection().Size();
      DCHECK_ALIGNED(objects_end, kObjectAlignment);
      for (uint32_t pos = sizeof(ImageHeader); pos != objects_end; ) {
        mirror::Object* object = reinterpret_cast<mirror::Object*>(space->Begin() + pos);
        if (!patched_objects->Test(object)) {
          // This is the last pass over objects, so we do not need to Set().
          patch_object_visitor.VisitObject(object);
          ObjPtr<mirror::Class> klass = object->GetClass<kVerifyNone, kWithoutReadBarrier>();
          if (klass->IsDexCacheClass<kVerifyNone>()) {
            // Patch dex cache array pointers and elements.
            ObjPtr<mirror::DexCache> dex_cache =
                object->AsDexCache<kVerifyNone, kWithoutReadBarrier>();
            patch_object_visitor.VisitDexCacheArrays(dex_cache);
          } else if (klass == method_class || klass == constructor_class) {
            // Patch the ArtMethod* in the mirror::Executable subobject.
            ObjPtr<mirror::Executable> as_executable =
                ObjPtr<mirror::Executable>::DownCast(object);
            ArtMethod* unpatched_method = as_executable->GetArtMethod<kVerifyNone>();
            ArtMethod* patched_method = relocate_visitor(unpatched_method);
            as_executable->SetArtMethod</*kTransactionActive=*/ false,
                                        /*kCheckTransaction=*/ true,
                                        kVerifyNone>(patched_method);
          }
        }
        pos += RoundUp(object->SizeOf<kVerifyNone>(), kObjectAlignment);
      }
    }
  }

  void MaybeRelocateSpaces(const std::vector<std::unique_ptr<ImageSpace>>& spaces,
                           TimingLogger* logger)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    TimingLogger::ScopedTiming timing("MaybeRelocateSpaces", logger);
    ImageSpace* first_space = spaces.front().get();
    const ImageHeader& first_space_header = first_space->GetImageHeader();
    uint32_t diff =
        static_cast<uint32_t>(first_space->Begin() - first_space_header.GetImageBegin());
    if (!relocate_) {
      DCHECK_EQ(diff, 0u);
      return;
    }

    PointerSize pointer_size = first_space_header.GetPointerSize();
    if (pointer_size == PointerSize::k64) {
      DoRelocateSpaces<PointerSize::k64>(spaces, diff);
    } else {
      DoRelocateSpaces<PointerSize::k32>(spaces, diff);
    }
  }

  std::unique_ptr<ImageSpace> Load(const std::string& image_location,
                                   const std::string& image_filename,
                                   TimingLogger* logger,
                                   /*inout*/MemMap* image_reservation,
                                   /*out*/std::string* error_msg)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    // Should this be a RDWR lock? This is only a defensive measure, as at
    // this point the image should exist.
    // However, only the zygote can write into the global dalvik-cache, so
    // restrict to zygote processes, or any process that isn't using
    // /data/dalvik-cache (which we assume to be allowed to write there).
    const bool rw_lock = is_zygote_ || !is_global_cache_;

    // Note that we must not use the file descriptor associated with
    // ScopedFlock::GetFile to Init the image file. We want the file
    // descriptor (and the associated exclusive lock) to be released when
    // we leave Create.
    ScopedFlock image = LockedFile::Open(image_filename.c_str(),
                                         /*flags=*/ rw_lock ? (O_CREAT | O_RDWR) : O_RDONLY,
                                         /*block=*/ true,
                                         error_msg);

    VLOG(startup) << "Using image file " << image_filename.c_str() << " for image location "
                  << image_location;
    // If we are in /system we can assume the image is good. We can also
    // assume this if we are using a relocated image (i.e. image checksum
    // matches) since this is only different by the offset. We need this to
    // make sure that host tests continue to work.
    // Since we are the boot image, pass null since we load the oat file from the boot image oat
    // file name.
    return Loader::Init(image_filename.c_str(),
                        image_location.c_str(),
                        /*oat_file=*/ nullptr,
                        logger,
                        image_reservation,
                        error_msg);
  }

  bool OpenOatFile(ImageSpace* space,
                   const std::string& dex_filename,
                   const std::string& expected_boot_class_path,
                   bool validate_oat_file,
                   TimingLogger* logger,
                   /*inout*/MemMap* image_reservation,
                   /*out*/std::string* error_msg) {
    // VerifyImageAllocations() will be called later in Runtime::Init()
    // as some class roots like ArtMethod::java_lang_reflect_ArtMethod_
    // and ArtField::java_lang_reflect_ArtField_, which are used from
    // Object::SizeOf() which VerifyImageAllocations() calls, are not
    // set yet at this point.
    DCHECK(image_reservation != nullptr);
    std::unique_ptr<OatFile> oat_file;
    {
      TimingLogger::ScopedTiming timing("OpenOatFile", logger);
      std::string oat_filename =
          ImageHeader::GetOatLocationFromImageLocation(space->GetImageFilename());
      std::string oat_location =
          ImageHeader::GetOatLocationFromImageLocation(space->GetImageLocation());

      oat_file.reset(OatFile::Open(/*zip_fd=*/ -1,
                                   oat_filename,
                                   oat_location,
                                   executable_,
                                   /*low_4gb=*/ false,
                                   /*abs_dex_location=*/ dex_filename.c_str(),
                                   image_reservation,
                                   error_msg));
      if (oat_file == nullptr) {
        *error_msg = StringPrintf("Failed to open oat file '%s' referenced from image %s: %s",
                                  oat_filename.c_str(),
                                  space->GetName(),
                                  error_msg->c_str());
        return false;
      }
      const ImageHeader& image_header = space->GetImageHeader();
      uint32_t oat_checksum = oat_file->GetOatHeader().GetChecksum();
      uint32_t image_oat_checksum = image_header.GetOatChecksum();
      if (oat_checksum != image_oat_checksum) {
        *error_msg = StringPrintf("Failed to match oat file checksum 0x%x to expected oat checksum"
                                  " 0x%x in image %s",
                                  oat_checksum,
                                  image_oat_checksum,
                                  space->GetName());
        return false;
      }
      const char* oat_boot_class_path =
          oat_file->GetOatHeader().GetStoreValueByKey(OatHeader::kBootClassPathKey);
      oat_boot_class_path = (oat_boot_class_path != nullptr) ? oat_boot_class_path : "";
      if (expected_boot_class_path != oat_boot_class_path) {
        *error_msg = StringPrintf("Failed to match oat boot class path %s to expected "
                                  "boot class path %s in image %s",
                                  oat_boot_class_path,
                                  expected_boot_class_path.c_str(),
                                  space->GetName());
        return false;
      }
      ptrdiff_t relocation_diff = space->Begin() - image_header.GetImageBegin();
      CHECK(image_header.GetOatDataBegin() != nullptr);
      uint8_t* oat_data_begin = image_header.GetOatDataBegin() + relocation_diff;
      if (oat_file->Begin() != oat_data_begin) {
        *error_msg = StringPrintf("Oat file '%s' referenced from image %s has unexpected begin"
                                      " %p v. %p",
                                  oat_filename.c_str(),
                                  space->GetName(),
                                  oat_file->Begin(),
                                  oat_data_begin);
        return false;
      }
    }
    if (validate_oat_file) {
      TimingLogger::ScopedTiming timing("ValidateOatFile", logger);
      if (!ImageSpace::ValidateOatFile(*oat_file, error_msg)) {
        DCHECK(!error_msg->empty());
        return false;
      }
    }
    space->oat_file_ = std::move(oat_file);
    space->oat_file_non_owned_ = space->oat_file_.get();
    return true;
  }

  bool ReserveBootImageMemory(uint32_t reservation_size,
                              uint32_t image_start,
                              size_t extra_reservation_size,
                              /*out*/MemMap* image_reservation,
                              /*out*/MemMap* extra_reservation,
                              /*out*/std::string* error_msg) {
    DCHECK_ALIGNED(reservation_size, kPageSize);
    DCHECK_ALIGNED(image_start, kPageSize);
    DCHECK(!image_reservation->IsValid());
    DCHECK_LT(extra_reservation_size, std::numeric_limits<uint32_t>::max() - reservation_size);
    size_t total_size = reservation_size + extra_reservation_size;
    // If relocating, choose a random address for ALSR.
    uint32_t addr = relocate_ ? ART_BASE_ADDRESS + ChooseRelocationOffsetDelta() : image_start;
    *image_reservation =
        MemMap::MapAnonymous("Boot image reservation",
                             reinterpret_cast32<uint8_t*>(addr),
                             total_size,
                             PROT_NONE,
                             /*low_4gb=*/ true,
                             /*reuse=*/ false,
                             /*reservation=*/ nullptr,
                             error_msg);
    if (!image_reservation->IsValid()) {
      return false;
    }
    DCHECK(!extra_reservation->IsValid());
    if (extra_reservation_size != 0u) {
      DCHECK_ALIGNED(extra_reservation_size, kPageSize);
      DCHECK_LT(extra_reservation_size, image_reservation->Size());
      uint8_t* split = image_reservation->End() - extra_reservation_size;
      *extra_reservation = image_reservation->RemapAtEnd(split,
                                                         "Boot image extra reservation",
                                                         PROT_NONE,
                                                         error_msg);
      if (!extra_reservation->IsValid()) {
        return false;
      }
    }

    return true;
  }

  bool CheckReservationExhausted(const MemMap& image_reservation, /*out*/std::string* error_msg) {
    if (image_reservation.IsValid()) {
      *error_msg = StringPrintf("Excessive image reservation after loading boot image: %p-%p",
                                image_reservation.Begin(),
                                image_reservation.End());
      return false;
    }
    return true;
  }

  const std::vector<std::string>& boot_class_path_;
  const std::vector<std::string>& boot_class_path_locations_;
  const std::string& image_location_;
  InstructionSet image_isa_;
  bool relocate_;
  bool executable_;
  bool is_zygote_;
  bool has_system_;
  bool has_cache_;
  bool is_global_cache_;
  bool dalvik_cache_exists_;
  std::string dalvik_cache_;
  std::string cache_filename_;
};

static constexpr uint64_t kLowSpaceValue = 50 * MB;
static constexpr uint64_t kTmpFsSentinelValue = 384 * MB;

// Read the free space of the cache partition and make a decision whether to keep the generated
// image. This is to try to mitigate situations where the system might run out of space later.
static bool CheckSpace(const std::string& cache_filename, std::string* error_msg) {
  // Using statvfs vs statvfs64 because of b/18207376, and it is enough for all practical purposes.
  struct statvfs buf;

  int res = TEMP_FAILURE_RETRY(statvfs(cache_filename.c_str(), &buf));
  if (res != 0) {
    // Could not stat. Conservatively tell the system to delete the image.
    *error_msg = "Could not stat the filesystem, assuming low-memory situation.";
    return false;
  }

  uint64_t fs_overall_size = buf.f_bsize * static_cast<uint64_t>(buf.f_blocks);
  // Zygote is privileged, but other things are not. Use bavail.
  uint64_t fs_free_size = buf.f_bsize * static_cast<uint64_t>(buf.f_bavail);

  // Take the overall size as an indicator for a tmpfs, which is being used for the decryption
  // environment. We do not want to fail quickening the boot image there, as it is beneficial
  // for time-to-UI.
  if (fs_overall_size > kTmpFsSentinelValue) {
    if (fs_free_size < kLowSpaceValue) {
      *error_msg = StringPrintf("Low-memory situation: only %4.2f megabytes available, need at "
                                "least %" PRIu64 ".",
                                static_cast<double>(fs_free_size) / MB,
                                kLowSpaceValue / MB);
      return false;
    }
  }
  return true;
}

bool ImageSpace::LoadBootImage(
    const std::vector<std::string>& boot_class_path,
    const std::vector<std::string>& boot_class_path_locations,
    const std::string& image_location,
    const InstructionSet image_isa,
    ImageSpaceLoadingOrder order,
    bool relocate,
    bool executable,
    bool is_zygote,
    size_t extra_reservation_size,
    /*out*/std::vector<std::unique_ptr<space::ImageSpace>>* boot_image_spaces,
    /*out*/MemMap* extra_reservation) {
  ScopedTrace trace(__FUNCTION__);

  DCHECK(boot_image_spaces != nullptr);
  DCHECK(boot_image_spaces->empty());
  DCHECK_ALIGNED(extra_reservation_size, kPageSize);
  DCHECK(extra_reservation != nullptr);
  DCHECK_NE(image_isa, InstructionSet::kNone);

  if (image_location.empty()) {
    return false;
  }

  BootImageLoader loader(boot_class_path,
                         boot_class_path_locations,
                         image_location,
                         image_isa,
                         relocate,
                         executable,
                         is_zygote);

  // Step 0: Extra zygote work.

  // Step 0.a: If we're the zygote, mark boot.
  if (loader.IsZygote() && CanWriteToDalvikCache(image_isa)) {
    MarkZygoteStart(image_isa, Runtime::Current()->GetZygoteMaxFailedBoots());
  }

  loader.FindImageFiles();

  // Step 0.b: If we're the zygote, check for free space, and prune the cache preemptively,
  //           if necessary. While the runtime may be fine (it is pretty tolerant to
  //           out-of-disk-space situations), other parts of the platform are not.
  //
  //           The advantage of doing this proactively is that the later steps are simplified,
  //           i.e., we do not need to code retries.
  bool low_space = false;
  if (loader.IsZygote() && loader.DalvikCacheExists()) {
    // Extra checks for the zygote. These only apply when loading the first image, explained below.
    const std::string& dalvik_cache = loader.GetDalvikCache();
    DCHECK(!dalvik_cache.empty());
    std::string local_error_msg;
    bool check_space = CheckSpace(dalvik_cache, &local_error_msg);
    if (!check_space) {
      LOG(WARNING) << local_error_msg << " Preemptively pruning the dalvik cache.";
      PruneDalvikCache(image_isa);

      // Re-evaluate the image.
      loader.FindImageFiles();

      // Disable compilation/patching - we do not want to fill up the space again.
      low_space = true;
    }
  }

  // Collect all the errors.
  std::vector<std::string> error_msgs;

  auto try_load_from = [&](auto has_fn, auto load_fn, bool validate_oat_file) {
    if ((loader.*has_fn)()) {
      std::string local_error_msg;
      if ((loader.*load_fn)(validate_oat_file,
                            extra_reservation_size,
                            boot_image_spaces,
                            extra_reservation,
                            &local_error_msg)) {
        return true;
      }
      error_msgs.push_back(local_error_msg);
    }
    return false;
  };

  auto try_load_from_system = [&]() {
    // Validate the oat files if the loading order checks data first. Otherwise assume system
    // integrity.
    return try_load_from(&BootImageLoader::HasSystem,
                         &BootImageLoader::LoadFromSystem,
                         /*validate_oat_file=*/ order != ImageSpaceLoadingOrder::kSystemFirst);
  };
  auto try_load_from_cache = [&]() {
    // Always validate oat files from the dalvik cache.
    return try_load_from(&BootImageLoader::HasCache,
                         &BootImageLoader::LoadFromDalvikCache,
                         /*validate_oat_file=*/ true);
  };

  auto invoke_sequentially = [](auto first, auto second) {
    return first() || second();
  };

  // Step 1+2: Check system and cache images in the asked-for order.
  if (order == ImageSpaceLoadingOrder::kSystemFirst) {
    if (invoke_sequentially(try_load_from_system, try_load_from_cache)) {
      return true;
    }
  } else {
    if (invoke_sequentially(try_load_from_cache, try_load_from_system)) {
      return true;
    }
  }

  // Step 3: We do not have an existing image in /system,
  //         so generate an image into the dalvik cache.
  if (!loader.HasSystem() && loader.DalvikCacheExists()) {
    std::string local_error_msg;
    if (low_space || !Runtime::Current()->IsImageDex2OatEnabled()) {
      local_error_msg = "Image compilation disabled.";
    } else if (ImageCreationAllowed(loader.IsGlobalCache(),
                                    image_isa,
                                    is_zygote,
                                    &local_error_msg)) {
      bool compilation_success =
          GenerateImage(loader.GetCacheFilename(), image_isa, &local_error_msg);
      if (compilation_success) {
        if (loader.LoadFromDalvikCache(/*validate_oat_file=*/ false,
                                       extra_reservation_size,
                                       boot_image_spaces,
                                       extra_reservation,
                                       &local_error_msg)) {
          return true;
        }
      }
    }
    error_msgs.push_back(StringPrintf("Cannot compile image to %s: %s",
                                      loader.GetCacheFilename().c_str(),
                                      local_error_msg.c_str()));
  }

  // We failed. Prune the cache the free up space, create a compound error message
  // and return false.
  if (loader.DalvikCacheExists()) {
    PruneDalvikCache(image_isa);
  }

  std::ostringstream oss;
  bool first = true;
  for (const auto& msg : error_msgs) {
    if (!first) {
      oss << "\n    ";
    }
    oss << msg;
  }

  LOG(ERROR) << "Could not create image space with image file '" << image_location << "'. "
      << "Attempting to fall back to imageless running. Error was: " << oss.str();

  return false;
}

ImageSpace::~ImageSpace() {
  // Everything done by member destructors. Classes forward-declared in header are now defined.
}

std::unique_ptr<ImageSpace> ImageSpace::CreateFromAppImage(const char* image,
                                                           const OatFile* oat_file,
                                                           std::string* error_msg) {
  // Note: The oat file has already been validated.
  return Loader::InitAppImage(image,
                              image,
                              oat_file,
                              /*image_reservation=*/ nullptr,
                              error_msg);
}

const OatFile* ImageSpace::GetOatFile() const {
  return oat_file_non_owned_;
}

std::unique_ptr<const OatFile> ImageSpace::ReleaseOatFile() {
  CHECK(oat_file_ != nullptr);
  return std::move(oat_file_);
}

void ImageSpace::Dump(std::ostream& os) const {
  os << GetType()
      << " begin=" << reinterpret_cast<void*>(Begin())
      << ",end=" << reinterpret_cast<void*>(End())
      << ",size=" << PrettySize(Size())
      << ",name=\"" << GetName() << "\"]";
}

bool ImageSpace::ValidateOatFile(const OatFile& oat_file, std::string* error_msg) {
  const ArtDexFileLoader dex_file_loader;
  for (const OatDexFile* oat_dex_file : oat_file.GetOatDexFiles()) {
    const std::string& dex_file_location = oat_dex_file->GetDexFileLocation();

    // Skip multidex locations - These will be checked when we visit their
    // corresponding primary non-multidex location.
    if (DexFileLoader::IsMultiDexLocation(dex_file_location.c_str())) {
      continue;
    }

    std::vector<uint32_t> checksums;
    if (!dex_file_loader.GetMultiDexChecksums(dex_file_location.c_str(), &checksums, error_msg)) {
      *error_msg = StringPrintf("ValidateOatFile failed to get checksums of dex file '%s' "
                                "referenced by oat file %s: %s",
                                dex_file_location.c_str(),
                                oat_file.GetLocation().c_str(),
                                error_msg->c_str());
      return false;
    }
    CHECK(!checksums.empty());
    if (checksums[0] != oat_dex_file->GetDexFileLocationChecksum()) {
      *error_msg = StringPrintf("ValidateOatFile found checksum mismatch between oat file "
                                "'%s' and dex file '%s' (0x%x != 0x%x)",
                                oat_file.GetLocation().c_str(),
                                dex_file_location.c_str(),
                                oat_dex_file->GetDexFileLocationChecksum(),
                                checksums[0]);
      return false;
    }

    // Verify checksums for any related multidex entries.
    for (size_t i = 1; i < checksums.size(); i++) {
      std::string multi_dex_location = DexFileLoader::GetMultiDexLocation(
          i,
          dex_file_location.c_str());
      const OatDexFile* multi_dex = oat_file.GetOatDexFile(multi_dex_location.c_str(),
                                                           nullptr,
                                                           error_msg);
      if (multi_dex == nullptr) {
        *error_msg = StringPrintf("ValidateOatFile oat file '%s' is missing entry '%s'",
                                  oat_file.GetLocation().c_str(),
                                  multi_dex_location.c_str());
        return false;
      }

      if (checksums[i] != multi_dex->GetDexFileLocationChecksum()) {
        *error_msg = StringPrintf("ValidateOatFile found checksum mismatch between oat file "
                                  "'%s' and dex file '%s' (0x%x != 0x%x)",
                                  oat_file.GetLocation().c_str(),
                                  multi_dex_location.c_str(),
                                  multi_dex->GetDexFileLocationChecksum(),
                                  checksums[i]);
        return false;
      }
    }
  }
  return true;
}

std::string ImageSpace::GetBootClassPathChecksums(ArrayRef<const std::string> boot_class_path,
                                                  const std::string& image_location,
                                                  InstructionSet image_isa,
                                                  ImageSpaceLoadingOrder order,
                                                  /*out*/std::string* error_msg) {
  std::string system_filename;
  bool has_system = false;
  std::string cache_filename;
  bool has_cache = false;
  bool dalvik_cache_exists = false;
  bool is_global_cache = false;
  if (!FindImageFilename(image_location.c_str(),
                         image_isa,
                         &system_filename,
                         &has_system,
                         &cache_filename,
                         &dalvik_cache_exists,
                         &has_cache,
                         &is_global_cache)) {
    *error_msg = StringPrintf("Unable to find image file for %s and %s",
                              image_location.c_str(),
                              GetInstructionSetString(image_isa));
    return std::string();
  }

  DCHECK(has_system || has_cache);
  const std::string& filename = (order == ImageSpaceLoadingOrder::kSystemFirst)
      ? (has_system ? system_filename : cache_filename)
      : (has_cache ? cache_filename : system_filename);
  std::unique_ptr<ImageHeader> header = ReadSpecificImageHeader(filename.c_str(), error_msg);
  if (header == nullptr) {
    return std::string();
  }
  if (header->GetComponentCount() == 0u || header->GetComponentCount() > boot_class_path.size()) {
    *error_msg = StringPrintf("Unexpected component count in %s, received %u, "
                                  "expected non-zero and <= %zu",
                              filename.c_str(),
                              header->GetComponentCount(),
                              boot_class_path.size());
    return std::string();
  }

  std::string boot_image_checksum =
      StringPrintf("i;%d/%08x", header->GetComponentCount(), header->GetImageChecksum());
  ArrayRef<const std::string> boot_class_path_tail =
      ArrayRef<const std::string>(boot_class_path).SubArray(header->GetComponentCount());
  for (const std::string& bcp_filename : boot_class_path_tail) {
    std::vector<std::unique_ptr<const DexFile>> dex_files;
    const ArtDexFileLoader dex_file_loader;
    if (!dex_file_loader.Open(bcp_filename.c_str(),
                              bcp_filename,  // The location does not matter here.
                              /*verify=*/ false,
                              /*verify_checksum=*/ false,
                              error_msg,
                              &dex_files)) {
      return std::string();
    }
    DCHECK(!dex_files.empty());
    StringAppendF(&boot_image_checksum, ":d");
    for (const std::unique_ptr<const DexFile>& dex_file : dex_files) {
      StringAppendF(&boot_image_checksum, "/%08x", dex_file->GetLocationChecksum());
    }
  }
  return boot_image_checksum;
}

std::string ImageSpace::GetBootClassPathChecksums(
    const std::vector<ImageSpace*>& image_spaces,
    const std::vector<const DexFile*>& boot_class_path) {
  size_t pos = 0u;
  std::string boot_image_checksum;

  if (!image_spaces.empty()) {
    const ImageHeader& primary_header = image_spaces.front()->GetImageHeader();
    uint32_t component_count = primary_header.GetComponentCount();
    DCHECK_EQ(component_count, image_spaces.size());
    boot_image_checksum =
        StringPrintf("i;%d/%08x", component_count, primary_header.GetImageChecksum());
    for (const ImageSpace* space : image_spaces) {
      size_t num_dex_files = space->oat_file_non_owned_->GetOatDexFiles().size();
      if (kIsDebugBuild) {
        CHECK_NE(num_dex_files, 0u);
        CHECK_LE(space->oat_file_non_owned_->GetOatDexFiles().size(), boot_class_path.size() - pos);
        for (size_t i = 0; i != num_dex_files; ++i) {
          CHECK_EQ(space->oat_file_non_owned_->GetOatDexFiles()[i]->GetDexFileLocation(),
                   boot_class_path[pos + i]->GetLocation());
        }
      }
      pos += num_dex_files;
    }
  }

  ArrayRef<const DexFile* const> boot_class_path_tail =
      ArrayRef<const DexFile* const>(boot_class_path).SubArray(pos);
  DCHECK(boot_class_path_tail.empty() ||
         !DexFileLoader::IsMultiDexLocation(boot_class_path_tail.front()->GetLocation().c_str()));
  for (const DexFile* dex_file : boot_class_path_tail) {
    if (!DexFileLoader::IsMultiDexLocation(dex_file->GetLocation().c_str())) {
      StringAppendF(&boot_image_checksum, boot_image_checksum.empty() ? "d" : ":d");
    }
    StringAppendF(&boot_image_checksum, "/%08x", dex_file->GetLocationChecksum());
  }
  return boot_image_checksum;
}

std::vector<std::string> ImageSpace::ExpandMultiImageLocations(
    const std::vector<std::string>& dex_locations,
    const std::string& image_location) {
  return ExpandMultiImageLocations(ArrayRef<const std::string>(dex_locations), image_location);
}

std::vector<std::string> ImageSpace::ExpandMultiImageLocations(
    ArrayRef<const std::string> dex_locations,
    const std::string& image_location) {
  DCHECK(!dex_locations.empty());

  // Find the path.
  size_t last_slash = image_location.rfind('/');
  CHECK_NE(last_slash, std::string::npos);

  // We also need to honor path components that were encoded through '@'. Otherwise the loading
  // code won't be able to find the images.
  if (image_location.find('@', last_slash) != std::string::npos) {
    last_slash = image_location.rfind('@');
  }

  // Find the dot separating the primary image name from the extension.
  size_t last_dot = image_location.rfind('.');
  // Extract the extension and base (the path and primary image name).
  std::string extension;
  std::string base = image_location;
  if (last_dot != std::string::npos && last_dot > last_slash) {
    extension = image_location.substr(last_dot);  // Including the dot.
    base.resize(last_dot);
  }
  // For non-empty primary image name, add '-' to the `base`.
  if (last_slash + 1u != base.size()) {
    base += '-';
  }

  std::vector<std::string> locations;
  locations.reserve(dex_locations.size());
  locations.push_back(image_location);

  // Now create the other names. Use a counted loop to skip the first one.
  for (size_t i = 1u; i < dex_locations.size(); ++i) {
    // Replace path with `base` (i.e. image path and prefix) and replace the original
    // extension (if any) with `extension`.
    std::string name = dex_locations[i];
    size_t last_dex_slash = name.rfind('/');
    if (last_dex_slash != std::string::npos) {
      name = name.substr(last_dex_slash + 1);
    }
    size_t last_dex_dot = name.rfind('.');
    if (last_dex_dot != std::string::npos) {
      name.resize(last_dex_dot);
    }
    locations.push_back(base + name + extension);
  }
  return locations;
}

void ImageSpace::DumpSections(std::ostream& os) const {
  const uint8_t* base = Begin();
  const ImageHeader& header = GetImageHeader();
  for (size_t i = 0; i < ImageHeader::kSectionCount; ++i) {
    auto section_type = static_cast<ImageHeader::ImageSections>(i);
    const ImageSection& section = header.GetImageSection(section_type);
    os << section_type << " " << reinterpret_cast<const void*>(base + section.Offset())
       << "-" << reinterpret_cast<const void*>(base + section.End()) << "\n";
  }
}

void ImageSpace::DisablePreResolvedStrings() {
  // Clear dex cache pointers.
  ObjPtr<mirror::ObjectArray<mirror::DexCache>> dex_caches =
      GetImageHeader().GetImageRoot(ImageHeader::kDexCaches)->AsObjectArray<mirror::DexCache>();
  for (size_t len = dex_caches->GetLength(), i = 0; i < len; ++i) {
    ObjPtr<mirror::DexCache> dex_cache = dex_caches->Get(i);
    dex_cache->ClearPreResolvedStrings();
  }
}

void ImageSpace::ReleaseMetadata() {
  const ImageSection& metadata = GetImageHeader().GetMetadataSection();
  VLOG(image) << "Releasing " << metadata.Size() << " image metadata bytes";
  // In the case where new app images may have been added around the checkpoint, ensure that we
  // don't madvise the cache for these.
  ObjPtr<mirror::ObjectArray<mirror::DexCache>> dex_caches =
      GetImageHeader().GetImageRoot(ImageHeader::kDexCaches)->AsObjectArray<mirror::DexCache>();
  bool have_startup_cache = false;
  for (size_t len = dex_caches->GetLength(), i = 0; i < len; ++i) {
    ObjPtr<mirror::DexCache> dex_cache = dex_caches->Get(i);
    if (dex_cache->NumPreResolvedStrings() != 0u) {
      have_startup_cache = true;
    }
  }
  // Only safe to do for images that have their preresolved strings caches disabled. This is because
  // uncompressed images madvise to the original unrelocated image contents.
  if (!have_startup_cache) {
    // Avoid using ZeroAndReleasePages since the zero fill might not be word atomic.
    uint8_t* const page_begin = AlignUp(Begin() + metadata.Offset(), kPageSize);
    uint8_t* const page_end = AlignDown(Begin() + metadata.End(), kPageSize);
    if (page_begin < page_end) {
      CHECK_NE(madvise(page_begin, page_end - page_begin, MADV_DONTNEED), -1) << "madvise failed";
    }
  }
}

}  // namespace space
}  // namespace gc
}  // namespace art
