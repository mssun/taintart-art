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

#include "instruction_set_features_arm64.h"

#if defined(ART_TARGET_ANDROID) && defined(__aarch64__)
#include <asm/hwcap.h>
#include <sys/auxv.h>
#endif

#include <fstream>
#include <sstream>

#include <android-base/logging.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>

#include "base/stl_util.h"

namespace art {

using android::base::StringPrintf;

Arm64FeaturesUniquePtr Arm64InstructionSetFeatures::FromVariant(
    const std::string& variant, std::string* error_msg) {
  // The CPU variant string is passed to ART through --instruction-set-variant option.
  // During build, such setting is from TARGET_CPU_VARIANT in device BoardConfig.mk, for example:
  //   TARGET_CPU_VARIANT := cortex-a75

  // Look for variants that need a fix for a53 erratum 835769.
  static const char* arm64_variants_with_a53_835769_bug[] = {
      // Pessimistically assume all generic CPUs are cortex-a53.
      "default",
      "generic",
      "cortex-a53",
      "cortex-a53.a57",
      "cortex-a53.a72",
      // Pessimistically assume following "big" cortex CPUs are paired with a cortex-a53.
      "cortex-a57",
      "cortex-a72",
      "cortex-a73",
  };

  static const char* arm64_variants_with_crc[] = {
      "default",
      "generic",
      "cortex-a35",
      "cortex-a53",
      "cortex-a53.a57",
      "cortex-a53.a72",
      "cortex-a57",
      "cortex-a72",
      "cortex-a73",
      "cortex-a55",
      "cortex-a75",
      "cortex-a76",
      "exynos-m1",
      "exynos-m2",
      "exynos-m3",
      "kryo",
      "kryo385",
  };

  static const char* arm64_variants_with_lse[] = {
      "cortex-a55",
      "cortex-a75",
      "cortex-a76",
      "kryo385",
  };

  static const char* arm64_variants_with_fp16[] = {
      "cortex-a55",
      "cortex-a75",
      "cortex-a76",
      "kryo385",
  };

  static const char* arm64_variants_with_dotprod[] = {
      "cortex-a55",
      "cortex-a75",
      "cortex-a76",
  };

  bool needs_a53_835769_fix = FindVariantInArray(arm64_variants_with_a53_835769_bug,
                                                 arraysize(arm64_variants_with_a53_835769_bug),
                                                 variant);
  // The variants that need a fix for 843419 are the same that need a fix for 835769.
  bool needs_a53_843419_fix = needs_a53_835769_fix;

  bool has_crc = FindVariantInArray(arm64_variants_with_crc,
                                    arraysize(arm64_variants_with_crc),
                                    variant);

  bool has_lse = FindVariantInArray(arm64_variants_with_lse,
                                    arraysize(arm64_variants_with_lse),
                                    variant);

  bool has_fp16 = FindVariantInArray(arm64_variants_with_fp16,
                                     arraysize(arm64_variants_with_fp16),
                                     variant);

  bool has_dotprod = FindVariantInArray(arm64_variants_with_dotprod,
                                        arraysize(arm64_variants_with_dotprod),
                                        variant);

  if (!needs_a53_835769_fix) {
    // Check to see if this is an expected variant.
    static const char* arm64_known_variants[] = {
        "cortex-a35",
        "cortex-a55",
        "cortex-a75",
        "cortex-a76",
        "exynos-m1",
        "exynos-m2",
        "exynos-m3",
        "denver64",
        "kryo",
        "kryo385",
    };
    if (!FindVariantInArray(arm64_known_variants, arraysize(arm64_known_variants), variant)) {
      std::ostringstream os;
      os << "Unexpected CPU variant for Arm64: " << variant;
      *error_msg = os.str();
      return nullptr;
    }
  }

  return Arm64FeaturesUniquePtr(new Arm64InstructionSetFeatures(needs_a53_835769_fix,
                                                                needs_a53_843419_fix,
                                                                has_crc,
                                                                has_lse,
                                                                has_fp16,
                                                                has_dotprod));
}

Arm64FeaturesUniquePtr Arm64InstructionSetFeatures::FromBitmap(uint32_t bitmap) {
  bool is_a53 = (bitmap & kA53Bitfield) != 0;
  bool has_crc = (bitmap & kCRCBitField) != 0;
  bool has_lse = (bitmap & kLSEBitField) != 0;
  bool has_fp16 = (bitmap & kFP16BitField) != 0;
  bool has_dotprod = (bitmap & kDotProdBitField) != 0;
  return Arm64FeaturesUniquePtr(new Arm64InstructionSetFeatures(is_a53,
                                                                is_a53,
                                                                has_crc,
                                                                has_lse,
                                                                has_fp16,
                                                                has_dotprod));
}

Arm64FeaturesUniquePtr Arm64InstructionSetFeatures::FromCppDefines() {
  // For more details about ARM feature macros, refer to
  // Arm C Language Extensions Documentation (ACLE).
  // https://developer.arm.com/docs/101028/latest
  bool needs_a53_835769_fix = false;
  bool needs_a53_843419_fix = needs_a53_835769_fix;
  bool has_crc = false;
  bool has_lse = false;
  bool has_fp16 = false;
  bool has_dotprod = false;

#if defined (__ARM_FEATURE_CRC32)
  has_crc = true;
#endif

#if defined (__ARM_ARCH_8_1A__) || defined (__ARM_ARCH_8_2A__)
  // There is no specific ACLE macro defined for ARMv8.1 LSE features.
  has_lse = true;
#endif

#if defined (__ARM_FEATURE_FP16_SCALAR_ARITHMETIC) || defined (__ARM_FEATURE_FP16_VECTOR_ARITHMETIC)
  has_fp16 = true;
#endif

#if defined (__ARM_FEATURE_DOTPROD)
  has_dotprod = true;
#endif

  return Arm64FeaturesUniquePtr(new Arm64InstructionSetFeatures(needs_a53_835769_fix,
                                                                needs_a53_843419_fix,
                                                                has_crc,
                                                                has_lse,
                                                                has_fp16,
                                                                has_dotprod));
}

Arm64FeaturesUniquePtr Arm64InstructionSetFeatures::FromCpuInfo() {
  UNIMPLEMENTED(WARNING);
  return FromCppDefines();
}

Arm64FeaturesUniquePtr Arm64InstructionSetFeatures::FromHwcap() {
  bool needs_a53_835769_fix = false;  // No HWCAP for this.
  bool needs_a53_843419_fix = false;  // No HWCAP for this.
  bool has_crc = false;
  bool has_lse = false;
  bool has_fp16 = false;
  bool has_dotprod = false;

#if defined(ART_TARGET_ANDROID) && defined(__aarch64__)
  uint64_t hwcaps = getauxval(AT_HWCAP);
  has_crc = hwcaps & HWCAP_CRC32 ? true : false;
  has_lse = hwcaps & HWCAP_ATOMICS ? true : false;
  has_fp16 = hwcaps & HWCAP_FPHP ? true : false;
  has_dotprod = hwcaps & HWCAP_ASIMDDP ? true : false;
#endif

  return Arm64FeaturesUniquePtr(new Arm64InstructionSetFeatures(needs_a53_835769_fix,
                                                                needs_a53_843419_fix,
                                                                has_crc,
                                                                has_lse,
                                                                has_fp16,
                                                                has_dotprod));
}

Arm64FeaturesUniquePtr Arm64InstructionSetFeatures::FromAssembly() {
  UNIMPLEMENTED(WARNING);
  return FromCppDefines();
}

bool Arm64InstructionSetFeatures::Equals(const InstructionSetFeatures* other) const {
  if (InstructionSet::kArm64 != other->GetInstructionSet()) {
    return false;
  }
  const Arm64InstructionSetFeatures* other_as_arm64 = other->AsArm64InstructionSetFeatures();
  return fix_cortex_a53_835769_ == other_as_arm64->fix_cortex_a53_835769_ &&
      fix_cortex_a53_843419_ == other_as_arm64->fix_cortex_a53_843419_ &&
      has_crc_ == other_as_arm64->has_crc_ &&
      has_lse_ == other_as_arm64->has_lse_ &&
      has_fp16_ == other_as_arm64->has_fp16_ &&
      has_dotprod_ == other_as_arm64->has_dotprod_;
}

bool Arm64InstructionSetFeatures::HasAtLeast(const InstructionSetFeatures* other) const {
  if (InstructionSet::kArm64 != other->GetInstructionSet()) {
    return false;
  }
  // Currently 'default' feature is cortex-a53 with fixes 835769 and 843419.
  // Newer CPUs are not required to have such features,
  // so these two a53 fix features are not tested for HasAtLeast.
  const Arm64InstructionSetFeatures* other_as_arm64 = other->AsArm64InstructionSetFeatures();
  return (has_crc_ || !other_as_arm64->has_crc_)
      && (has_lse_ || !other_as_arm64->has_lse_)
      && (has_fp16_ || !other_as_arm64->has_fp16_)
      && (has_dotprod_ || !other_as_arm64->has_dotprod_);
}

uint32_t Arm64InstructionSetFeatures::AsBitmap() const {
  return (fix_cortex_a53_835769_ ? kA53Bitfield : 0)
      | (has_crc_ ? kCRCBitField : 0)
      | (has_lse_ ? kLSEBitField: 0)
      | (has_fp16_ ? kFP16BitField: 0)
      | (has_dotprod_ ? kDotProdBitField : 0);
}

std::string Arm64InstructionSetFeatures::GetFeatureString() const {
  std::string result;
  if (fix_cortex_a53_835769_) {
    result += "a53";
  } else {
    result += "-a53";
  }
  if (has_crc_) {
    result += ",crc";
  } else {
    result += ",-crc";
  }
  if (has_lse_) {
    result += ",lse";
  } else {
    result += ",-lse";
  }
  if (has_fp16_) {
    result += ",fp16";
  } else {
    result += ",-fp16";
  }
  if (has_dotprod_) {
    result += ",dotprod";
  } else {
    result += ",-dotprod";
  }
  return result;
}

std::unique_ptr<const InstructionSetFeatures>
Arm64InstructionSetFeatures::AddFeaturesFromSplitString(
    const std::vector<std::string>& features, std::string* error_msg) const {
  // This 'features' string is from '--instruction-set-features=' option in ART.
  // These ARMv8.x feature strings align with those introduced in other compilers:
  // https://gcc.gnu.org/onlinedocs/gcc/ARM-Options.html
  // User can also use armv8.x-a to select group of features:
  //   armv8.1-a is equivalent to crc,lse
  //   armv8.2-a is equivalent to crc,lse,fp16
  //   armv8.3-a is equivalent to crc,lse,fp16
  //   armv8.4-a is equivalent to crc,lse,fp16,dotprod
  // For detailed optional & mandatory features support in armv8.x-a,
  // please refer to section 'A1.7 ARMv8 architecture extensions' in
  // ARM Architecture Reference Manual ARMv8 document:
  // https://developer.arm.com/products/architecture/cpu-architecture/a-profile/docs/ddi0487/latest/
  // arm-architecture-reference-manual-armv8-for-armv8-a-architecture-profile/
  bool is_a53 = fix_cortex_a53_835769_;
  bool has_crc = has_crc_;
  bool has_lse = has_lse_;
  bool has_fp16 = has_fp16_;
  bool has_dotprod = has_dotprod_;
  for (auto i = features.begin(); i != features.end(); i++) {
    std::string feature = android::base::Trim(*i);
    if (feature == "a53") {
      is_a53 = true;
    } else if (feature == "-a53") {
      is_a53 = false;
    } else if (feature == "crc") {
      has_crc = true;
    } else if (feature == "-crc") {
      has_crc = false;
    } else if (feature == "lse") {
      has_lse = true;
    } else if (feature == "-lse") {
      has_lse = false;
    } else if (feature == "fp16") {
      has_fp16 = true;
    } else if (feature == "-fp16") {
      has_fp16 = false;
    } else if (feature == "dotprod") {
      has_dotprod = true;
    } else if (feature == "-dotprod") {
      has_dotprod = false;
    } else if (feature == "armv8.1-a") {
      has_crc = true;
      has_lse = true;
    } else if (feature == "armv8.2-a") {
      has_crc = true;
      has_lse = true;
      has_fp16 = true;
    } else if (feature == "armv8.3-a") {
      has_crc = true;
      has_lse = true;
      has_fp16 = true;
    } else if (feature == "armv8.4-a") {
      has_crc = true;
      has_lse = true;
      has_fp16 = true;
      has_dotprod = true;
    } else {
      *error_msg = StringPrintf("Unknown instruction set feature: '%s'", feature.c_str());
      return nullptr;
    }
  }
  return std::unique_ptr<const InstructionSetFeatures>(
      new Arm64InstructionSetFeatures(is_a53,  // erratum 835769
                                      is_a53,  // erratum 843419
                                      has_crc,
                                      has_lse,
                                      has_fp16,
                                      has_dotprod));
}

}  // namespace art
