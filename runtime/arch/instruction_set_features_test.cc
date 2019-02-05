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

#include <array>

#include "instruction_set_features.h"

#include <gtest/gtest.h>

#ifdef ART_TARGET_ANDROID
#include <android-base/properties.h>
#endif

#include <android-base/logging.h>
#include <android-base/stringprintf.h>

namespace art {

#ifdef ART_TARGET_ANDROID

using android::base::StringPrintf;

#if defined(__aarch64__)
TEST(InstructionSetFeaturesTest, DISABLED_FeaturesFromSystemPropertyVariant) {
  LOG(WARNING) << "Test disabled due to no CPP define for A53 erratum 835769";
#else
TEST(InstructionSetFeaturesTest, FeaturesFromSystemPropertyVariant) {
#endif
  // Take the default set of instruction features from the build.
  std::unique_ptr<const InstructionSetFeatures> instruction_set_features(
      InstructionSetFeatures::FromCppDefines());

  // Read the variant property.
  std::string key = StringPrintf("dalvik.vm.isa.%s.variant", GetInstructionSetString(kRuntimeISA));
  std::string dex2oat_isa_variant = android::base::GetProperty(key, "");
  if (!dex2oat_isa_variant.empty()) {
    // Use features from property to build InstructionSetFeatures and check against build's
    // features.
    std::string error_msg;
    std::unique_ptr<const InstructionSetFeatures> property_features(
        InstructionSetFeatures::FromVariant(kRuntimeISA, dex2oat_isa_variant, &error_msg));
    ASSERT_TRUE(property_features.get() != nullptr) << error_msg;

    EXPECT_TRUE(property_features->HasAtLeast(instruction_set_features.get()))
      << "System property features: " << *property_features.get()
      << "\nFeatures from build: " << *instruction_set_features.get();
  }
}

#if defined(__aarch64__)
TEST(InstructionSetFeaturesTest, DISABLED_FeaturesFromSystemPropertyString) {
  LOG(WARNING) << "Test disabled due to no CPP define for A53 erratum 835769";
#else
TEST(InstructionSetFeaturesTest, FeaturesFromSystemPropertyString) {
#endif
  // Take the default set of instruction features from the build.
  std::unique_ptr<const InstructionSetFeatures> instruction_set_features(
      InstructionSetFeatures::FromCppDefines());

  // Read the variant property.
  std::string variant_key = StringPrintf("dalvik.vm.isa.%s.variant",
                                         GetInstructionSetString(kRuntimeISA));
  std::string dex2oat_isa_variant = android::base::GetProperty(variant_key, "");
  if (!dex2oat_isa_variant.empty()) {
    // Read the features property.
    std::string features_key = StringPrintf("dalvik.vm.isa.%s.features",
                                            GetInstructionSetString(kRuntimeISA));
    std::string dex2oat_isa_features = android::base::GetProperty(features_key, "");
    if (!dex2oat_isa_features.empty()) {
      // Use features from property to build InstructionSetFeatures and check against build's
      // features.
      std::string error_msg;
      std::unique_ptr<const InstructionSetFeatures> base_features(
          InstructionSetFeatures::FromVariant(kRuntimeISA, dex2oat_isa_variant, &error_msg));
      ASSERT_TRUE(base_features.get() != nullptr) << error_msg;

      std::unique_ptr<const InstructionSetFeatures> property_features(
          base_features->AddFeaturesFromString(dex2oat_isa_features, &error_msg));
      ASSERT_TRUE(property_features.get() != nullptr) << error_msg;

      EXPECT_TRUE(property_features->HasAtLeast(instruction_set_features.get()))
      << "System property features: " << *property_features.get()
      << "\nFeatures from build: " << *instruction_set_features.get();
    }
  }
}

#if defined(__arm__)
TEST(InstructionSetFeaturesTest, DISABLED_FeaturesFromCpuInfo) {
  LOG(WARNING) << "Test disabled due to buggy ARM kernels";
#else
TEST(InstructionSetFeaturesTest, FeaturesFromCpuInfo) {
#endif
  // Take the default set of instruction features from the build.
  std::unique_ptr<const InstructionSetFeatures> instruction_set_features(
      InstructionSetFeatures::FromCppDefines());

  // Check we get the same instruction set features using /proc/cpuinfo.
  std::unique_ptr<const InstructionSetFeatures> cpuinfo_features(
      InstructionSetFeatures::FromCpuInfo());
  EXPECT_TRUE(cpuinfo_features->HasAtLeast(instruction_set_features.get()))
      << "CPU Info features: " << *cpuinfo_features.get()
      << "\nFeatures from build: " << *instruction_set_features.get();
}
#endif

#ifndef ART_TARGET_ANDROID
TEST(InstructionSetFeaturesTest, HostFeaturesFromCppDefines) {
  std::string error_msg;
  std::unique_ptr<const InstructionSetFeatures> default_features(
      InstructionSetFeatures::FromVariant(kRuntimeISA, "default", &error_msg));
  ASSERT_TRUE(error_msg.empty());

  std::unique_ptr<const InstructionSetFeatures> cpp_features(
      InstructionSetFeatures::FromCppDefines());
  EXPECT_TRUE(cpp_features->HasAtLeast(default_features.get()))
      << "Default variant features: " << *default_features.get()
      << "\nFeatures from build: " << *cpp_features.get();
}
#endif

#if defined(__arm__)
TEST(InstructionSetFeaturesTest, DISABLED_FeaturesFromHwcap) {
  LOG(WARNING) << "Test disabled due to buggy ARM kernels";
#else
TEST(InstructionSetFeaturesTest, FeaturesFromHwcap) {
#endif
  // Take the default set of instruction features from the build.
  std::unique_ptr<const InstructionSetFeatures> instruction_set_features(
      InstructionSetFeatures::FromCppDefines());

  // Check we get the same instruction set features using AT_HWCAP.
  std::unique_ptr<const InstructionSetFeatures> hwcap_features(
      InstructionSetFeatures::FromHwcap());
  EXPECT_TRUE(hwcap_features->HasAtLeast(instruction_set_features.get()))
      << "Hwcap features: " << *hwcap_features.get()
      << "\nFeatures from build: " << *instruction_set_features.get();
}

TEST(InstructionSetFeaturesTest, FeaturesFromAssembly) {
  // Take the default set of instruction features from the build.
  std::unique_ptr<const InstructionSetFeatures> instruction_set_features(
      InstructionSetFeatures::FromCppDefines());

  // Check we get the same instruction set features using assembly tests.
  std::unique_ptr<const InstructionSetFeatures> assembly_features(
      InstructionSetFeatures::FromAssembly());
  EXPECT_TRUE(assembly_features->HasAtLeast(instruction_set_features.get()))
      << "Assembly features: " << *assembly_features.get()
      << "\nFeatures from build: " << *instruction_set_features.get();
}

TEST(InstructionSetFeaturesTest, FeaturesFromRuntimeDetection) {
  if (!InstructionSetFeatures::IsRuntimeDetectionSupported()) {
    EXPECT_EQ(InstructionSetFeatures::FromRuntimeDetection(), nullptr);
  }
}

// The instruction set feature string must not contain 'default' together with
// other feature names.
//
// Test that InstructionSetFeatures::AddFeaturesFromString returns nullptr and
// an error is reported when the value 'default' is specified together
// with other feature names in an instruction set feature string.
TEST(InstructionSetFeaturesTest, AddFeaturesFromStringWithDefaultAndOtherNames) {
  std::unique_ptr<const InstructionSetFeatures> cpp_defined_features(
      InstructionSetFeatures::FromCppDefines());
  std::vector<std::string> invalid_feature_strings = {
    "a,default",
    "default,a",
    "a,default,b",
    "a,b,default",
    "default,a,b,c",
    "a,b,default,c,d",
    "a, default ",
    " default , a",
    "a, default , b",
    "default,runtime"
  };

  for (const std::string& invalid_feature_string : invalid_feature_strings) {
    std::string error_msg;
    EXPECT_EQ(cpp_defined_features->AddFeaturesFromString(invalid_feature_string, &error_msg),
              nullptr) << " Invalid feature string: '" << invalid_feature_string << "'";
    EXPECT_EQ(error_msg,
              "Specific instruction set feature(s) cannot be used when 'default' is used.");
  }
}

// The instruction set feature string must not contain 'runtime' together with
// other feature names.
//
// Test that InstructionSetFeatures::AddFeaturesFromString returns nullptr and
// an error is reported when the value 'runtime' is specified together
// with other feature names in an instruction set feature string.
TEST(InstructionSetFeaturesTest, AddFeaturesFromStringWithRuntimeAndOtherNames) {
  std::unique_ptr<const InstructionSetFeatures> cpp_defined_features(
      InstructionSetFeatures::FromCppDefines());
  std::vector<std::string> invalid_feature_strings = {
    "a,runtime",
    "runtime,a",
    "a,runtime,b",
    "a,b,runtime",
    "runtime,a,b,c",
    "a,b,runtime,c,d",
    "a, runtime ",
    " runtime , a",
    "a, runtime , b",
    "runtime,default"
  };

  for (const std::string& invalid_feature_string : invalid_feature_strings) {
    std::string error_msg;
    EXPECT_EQ(cpp_defined_features->AddFeaturesFromString(invalid_feature_string, &error_msg),
              nullptr) << " Invalid feature string: '" << invalid_feature_string << "'";
    EXPECT_EQ(error_msg,
              "Specific instruction set feature(s) cannot be used when 'runtime' is used.");
  }
}

// Spaces and multiple commas are ignores in a instruction set feature string.
//
// Test that a use of spaces and multiple commas with 'default' and 'runtime'
// does not cause errors.
TEST(InstructionSetFeaturesTest, AddFeaturesFromValidStringContainingDefaultOrRuntime) {
  std::unique_ptr<const InstructionSetFeatures> cpp_defined_features(
      InstructionSetFeatures::FromCppDefines());
  std::vector<std::string> valid_feature_strings = {
    "default",
    ",,,default",
    "default,,,,",
    ",,,default,,,,",
    "default, , , ",
    " , , ,default",
    " , , ,default, , , ",
    " default , , , ",
    ",,,runtime",
    "runtime,,,,",
    ",,,runtime,,,,",
    "runtime, , , ",
    " , , ,runtime",
    " , , ,runtime, , , ",
    " runtime , , , "
  };
  for (const std::string& valid_feature_string : valid_feature_strings) {
    std::string error_msg;
    EXPECT_NE(cpp_defined_features->AddFeaturesFromString(valid_feature_string, &error_msg),
              nullptr) << " Valid feature string: '" << valid_feature_string << "'";
    EXPECT_TRUE(error_msg.empty()) << error_msg;
  }
}

// Spaces and multiple commas are ignores in a instruction set feature string.
//
// Test that a use of spaces and multiple commas without any feature names
// causes errors.
TEST(InstructionSetFeaturesTest, AddFeaturesFromInvalidStringWithoutFeatureNames) {
  std::unique_ptr<const InstructionSetFeatures> cpp_defined_features(
      InstructionSetFeatures::FromCppDefines());
  std::vector<std::string> invalid_feature_strings = {
    " ",
    "       ",
    ",",
    ",,",
    " , , ,,,,,,",
    "\t",
    "  \t     ",
    ",",
    ",,",
    " , , ,,,,,,"
  };
  for (const std::string& invalid_feature_string : invalid_feature_strings) {
    std::string error_msg;
    EXPECT_EQ(cpp_defined_features->AddFeaturesFromString(invalid_feature_string, &error_msg),
              nullptr) << " Invalid feature string: '" << invalid_feature_string << "'";
    EXPECT_EQ(error_msg, "No instruction set features specified");
  }
}

TEST(InstructionSetFeaturesTest, AddFeaturesFromStringRuntime) {
  std::unique_ptr<const InstructionSetFeatures> cpp_defined_features(
      InstructionSetFeatures::FromCppDefines());
  std::string error_msg;

  const std::unique_ptr<const InstructionSetFeatures> features =
      cpp_defined_features->AddFeaturesFromString("runtime", &error_msg);
  EXPECT_NE(features, nullptr);
  EXPECT_TRUE(error_msg.empty()) << error_msg;
  if (!InstructionSetFeatures::IsRuntimeDetectionSupported()) {
    EXPECT_TRUE(features->Equals(cpp_defined_features.get()));
  }
}

}  // namespace art
