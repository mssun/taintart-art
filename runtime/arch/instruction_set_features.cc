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

#include <algorithm>

#include "instruction_set_features.h"

#include <algorithm>
#include <ostream>

#include "android-base/strings.h"

#include "base/casts.h"
#include "base/utils.h"

#include "arm/instruction_set_features_arm.h"
#include "arm64/instruction_set_features_arm64.h"
#include "mips/instruction_set_features_mips.h"
#include "mips64/instruction_set_features_mips64.h"
#include "x86/instruction_set_features_x86.h"
#include "x86_64/instruction_set_features_x86_64.h"

namespace art {

std::unique_ptr<const InstructionSetFeatures> InstructionSetFeatures::FromVariant(
    InstructionSet isa, const std::string& variant, std::string* error_msg) {
  switch (isa) {
    case InstructionSet::kArm:
    case InstructionSet::kThumb2:
      return ArmInstructionSetFeatures::FromVariant(variant, error_msg);
    case InstructionSet::kArm64:
      return Arm64InstructionSetFeatures::FromVariant(variant, error_msg);
    case InstructionSet::kMips:
      return MipsInstructionSetFeatures::FromVariant(variant, error_msg);
    case InstructionSet::kMips64:
      return Mips64InstructionSetFeatures::FromVariant(variant, error_msg);
    case InstructionSet::kX86:
      return X86InstructionSetFeatures::FromVariant(variant, error_msg);
    case InstructionSet::kX86_64:
      return X86_64InstructionSetFeatures::FromVariant(variant, error_msg);

    case InstructionSet::kNone:
      break;
  }
  UNIMPLEMENTED(FATAL) << isa;
  UNREACHABLE();
}

std::unique_ptr<const InstructionSetFeatures> InstructionSetFeatures::FromBitmap(InstructionSet isa,
                                                                                 uint32_t bitmap) {
  std::unique_ptr<const InstructionSetFeatures> result;
  switch (isa) {
    case InstructionSet::kArm:
    case InstructionSet::kThumb2:
      result = ArmInstructionSetFeatures::FromBitmap(bitmap);
      break;
    case InstructionSet::kArm64:
      result = Arm64InstructionSetFeatures::FromBitmap(bitmap);
      break;
    case InstructionSet::kMips:
      result = MipsInstructionSetFeatures::FromBitmap(bitmap);
      break;
    case InstructionSet::kMips64:
      result = Mips64InstructionSetFeatures::FromBitmap(bitmap);
      break;
    case InstructionSet::kX86:
      result = X86InstructionSetFeatures::FromBitmap(bitmap);
      break;
    case InstructionSet::kX86_64:
      result = X86_64InstructionSetFeatures::FromBitmap(bitmap);
      break;

    case InstructionSet::kNone:
    default:
      UNIMPLEMENTED(FATAL) << isa;
      UNREACHABLE();
  }
  CHECK_EQ(bitmap, result->AsBitmap());
  return result;
}

std::unique_ptr<const InstructionSetFeatures> InstructionSetFeatures::FromCppDefines() {
  switch (kRuntimeISA) {
    case InstructionSet::kArm:
    case InstructionSet::kThumb2:
      return ArmInstructionSetFeatures::FromCppDefines();
    case InstructionSet::kArm64:
      return Arm64InstructionSetFeatures::FromCppDefines();
    case InstructionSet::kMips:
      return MipsInstructionSetFeatures::FromCppDefines();
    case InstructionSet::kMips64:
      return Mips64InstructionSetFeatures::FromCppDefines();
    case InstructionSet::kX86:
      return X86InstructionSetFeatures::FromCppDefines();
    case InstructionSet::kX86_64:
      return X86_64InstructionSetFeatures::FromCppDefines();

    case InstructionSet::kNone:
      break;
  }
  UNIMPLEMENTED(FATAL) << kRuntimeISA;
  UNREACHABLE();
}

std::unique_ptr<const InstructionSetFeatures> InstructionSetFeatures::FromRuntimeDetection() {
  switch (kRuntimeISA) {
#ifdef ART_TARGET_ANDROID
    case InstructionSet::kArm64:
      return Arm64InstructionSetFeatures::FromHwcap();
#endif
    default:
      return nullptr;
  }
}

std::unique_ptr<const InstructionSetFeatures> InstructionSetFeatures::FromCpuInfo() {
  switch (kRuntimeISA) {
    case InstructionSet::kArm:
    case InstructionSet::kThumb2:
      return ArmInstructionSetFeatures::FromCpuInfo();
    case InstructionSet::kArm64:
      return Arm64InstructionSetFeatures::FromCpuInfo();
    case InstructionSet::kMips:
      return MipsInstructionSetFeatures::FromCpuInfo();
    case InstructionSet::kMips64:
      return Mips64InstructionSetFeatures::FromCpuInfo();
    case InstructionSet::kX86:
      return X86InstructionSetFeatures::FromCpuInfo();
    case InstructionSet::kX86_64:
      return X86_64InstructionSetFeatures::FromCpuInfo();

    case InstructionSet::kNone:
      break;
  }
  UNIMPLEMENTED(FATAL) << kRuntimeISA;
  UNREACHABLE();
}

std::unique_ptr<const InstructionSetFeatures> InstructionSetFeatures::FromHwcap() {
  switch (kRuntimeISA) {
    case InstructionSet::kArm:
    case InstructionSet::kThumb2:
      return ArmInstructionSetFeatures::FromHwcap();
    case InstructionSet::kArm64:
      return Arm64InstructionSetFeatures::FromHwcap();
    case InstructionSet::kMips:
      return MipsInstructionSetFeatures::FromHwcap();
    case InstructionSet::kMips64:
      return Mips64InstructionSetFeatures::FromHwcap();
    case InstructionSet::kX86:
      return X86InstructionSetFeatures::FromHwcap();
    case InstructionSet::kX86_64:
      return X86_64InstructionSetFeatures::FromHwcap();

    case InstructionSet::kNone:
      break;
  }
  UNIMPLEMENTED(FATAL) << kRuntimeISA;
  UNREACHABLE();
}

std::unique_ptr<const InstructionSetFeatures> InstructionSetFeatures::FromAssembly() {
  switch (kRuntimeISA) {
    case InstructionSet::kArm:
    case InstructionSet::kThumb2:
      return ArmInstructionSetFeatures::FromAssembly();
    case InstructionSet::kArm64:
      return Arm64InstructionSetFeatures::FromAssembly();
    case InstructionSet::kMips:
      return MipsInstructionSetFeatures::FromAssembly();
    case InstructionSet::kMips64:
      return Mips64InstructionSetFeatures::FromAssembly();
    case InstructionSet::kX86:
      return X86InstructionSetFeatures::FromAssembly();
    case InstructionSet::kX86_64:
      return X86_64InstructionSetFeatures::FromAssembly();

    case InstructionSet::kNone:
      break;
  }
  UNIMPLEMENTED(FATAL) << kRuntimeISA;
  UNREACHABLE();
}

std::unique_ptr<const InstructionSetFeatures> InstructionSetFeatures::AddFeaturesFromString(
    const std::string& feature_list, /* out */ std::string* error_msg) const {
  std::vector<std::string> features;
  Split(feature_list, ',', &features);
  std::transform(std::begin(features), std::end(features), std::begin(features),
                 [](const std::string &s) { return android::base::Trim(s); });
  auto empty_strings_begin = std::copy_if(std::begin(features), std::end(features),
                                          std::begin(features),
                                          [](const std::string& s) { return !s.empty(); });
  features.erase(empty_strings_begin, std::end(features));
  if (features.empty()) {
    *error_msg = "No instruction set features specified";
    return nullptr;
  }

  bool use_default = false;
  bool use_runtime_detection = false;
  for (const std::string& feature : features) {
    if (feature == "default") {
      if (features.size() > 1) {
        *error_msg = "Specific instruction set feature(s) cannot be used when 'default' is used.";
        return nullptr;
      }
      use_default = true;
      features.pop_back();
      break;
    } else if (feature == "runtime") {
      if (features.size() > 1) {
        *error_msg = "Specific instruction set feature(s) cannot be used when 'runtime' is used.";
        return nullptr;
      }
      use_runtime_detection = true;
      features.pop_back();
      break;
    }
  }
  // Expectation: "default" and "runtime" are standalone, no other feature names.
  // But an empty features vector after processing can also come along if the
  // handled feature names  are the only ones in the list. So
  // logically, we check "default or runtime => features.empty."
  DCHECK((!use_default && !use_runtime_detection) || features.empty());

  std::unique_ptr<const InstructionSetFeatures> runtime_detected_features;
  if (use_runtime_detection) {
    runtime_detected_features = FromRuntimeDetection();
  }

  if (runtime_detected_features != nullptr) {
    return AddRuntimeDetectedFeatures(runtime_detected_features.get());
  } else {
    return AddFeaturesFromSplitString(features, error_msg);
  }
}

const ArmInstructionSetFeatures* InstructionSetFeatures::AsArmInstructionSetFeatures() const {
  DCHECK_EQ(InstructionSet::kArm, GetInstructionSet());
  return down_cast<const ArmInstructionSetFeatures*>(this);
}

const Arm64InstructionSetFeatures* InstructionSetFeatures::AsArm64InstructionSetFeatures() const {
  DCHECK_EQ(InstructionSet::kArm64, GetInstructionSet());
  return down_cast<const Arm64InstructionSetFeatures*>(this);
}

const MipsInstructionSetFeatures* InstructionSetFeatures::AsMipsInstructionSetFeatures() const {
  DCHECK_EQ(InstructionSet::kMips, GetInstructionSet());
  return down_cast<const MipsInstructionSetFeatures*>(this);
}

const Mips64InstructionSetFeatures* InstructionSetFeatures::AsMips64InstructionSetFeatures() const {
  DCHECK_EQ(InstructionSet::kMips64, GetInstructionSet());
  return down_cast<const Mips64InstructionSetFeatures*>(this);
}

const X86InstructionSetFeatures* InstructionSetFeatures::AsX86InstructionSetFeatures() const {
  DCHECK(InstructionSet::kX86 == GetInstructionSet() ||
         InstructionSet::kX86_64 == GetInstructionSet());
  return down_cast<const X86InstructionSetFeatures*>(this);
}

const X86_64InstructionSetFeatures* InstructionSetFeatures::AsX86_64InstructionSetFeatures() const {
  DCHECK_EQ(InstructionSet::kX86_64, GetInstructionSet());
  return down_cast<const X86_64InstructionSetFeatures*>(this);
}

bool InstructionSetFeatures::FindVariantInArray(const char* const variants[], size_t num_variants,
                                                const std::string& variant) {
  const char* const * begin = variants;
  const char* const * end = begin + num_variants;
  return std::find(begin, end, variant) != end;
}

std::unique_ptr<const InstructionSetFeatures> InstructionSetFeatures::AddRuntimeDetectedFeatures(
    const InstructionSetFeatures *features ATTRIBUTE_UNUSED) const {
  UNIMPLEMENTED(FATAL) << kRuntimeISA;
  UNREACHABLE();
}

std::ostream& operator<<(std::ostream& os, const InstructionSetFeatures& rhs) {
  os << "ISA: " << rhs.GetInstructionSet() << " Feature string: " << rhs.GetFeatureString();
  return os;
}

}  // namespace art
