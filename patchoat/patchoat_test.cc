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

#include <openssl/sha.h>
#include <dirent.h>
#include <sys/types.h>

#include <string>
#include <vector>

#include "android-base/stringprintf.h"
#include "android-base/strings.h"

#include "dexopt_test.h"
#include "leb128.h"
#include "runtime.h"

#include <gtest/gtest.h>

namespace art {

using android::base::StringPrintf;

class PatchoatTest : public DexoptTest {
 public:
  static bool ListDirFilesEndingWith(
      const std::string& dir,
      const std::string& suffix,
      std::vector<std::string>* filenames,
      std::string* error_msg) {
    DIR* d = opendir(dir.c_str());
    if (d == nullptr) {
      *error_msg = "Failed to open directory";
      return false;
    }
    dirent* e;
    struct stat s;
    size_t suffix_len = suffix.size();
    while ((e = readdir(d)) != nullptr) {
      if ((strcmp(e->d_name, ".") == 0) || (strcmp(e->d_name, "..") == 0)) {
        continue;
      }
      size_t name_len = strlen(e->d_name);
      if ((name_len < suffix_len) || (strcmp(&e->d_name[name_len - suffix_len], suffix.c_str()))) {
        continue;
      }
      std::string basename(e->d_name);
      std::string filename = dir + "/" + basename;
      int stat_result = lstat(filename.c_str(), &s);
      if (stat_result != 0) {
        *error_msg =
            StringPrintf("Failed to stat %s: stat returned %d", filename.c_str(), stat_result);
        return false;
      }
      if (S_ISDIR(s.st_mode)) {
        continue;
      }
      filenames->push_back(basename);
    }
    closedir(d);
    return true;
  }

  static void AddRuntimeArg(std::vector<std::string>& args, const std::string& arg) {
    args.push_back("--runtime-arg");
    args.push_back(arg);
  }

  bool CompileBootImage(const std::vector<std::string>& extra_args,
                        const std::string& image_file_name_prefix,
                        uint32_t base_addr,
                        std::string* error_msg) {
    Runtime* const runtime = Runtime::Current();
    std::vector<std::string> argv;
    argv.push_back(runtime->GetCompilerExecutable());
    AddRuntimeArg(argv, "-Xms64m");
    AddRuntimeArg(argv, "-Xmx64m");
    std::vector<std::string> dex_files = GetLibCoreDexFileNames();
    for (const std::string& dex_file : dex_files) {
      argv.push_back("--dex-file=" + dex_file);
      argv.push_back("--dex-location=" + dex_file);
    }
    if (runtime->IsJavaDebuggable()) {
      argv.push_back("--debuggable");
    }
    runtime->AddCurrentRuntimeFeaturesAsDex2OatArguments(&argv);

    AddRuntimeArg(argv, "-Xverify:softfail");

    if (!kIsTargetBuild) {
      argv.push_back("--host");
    }

    argv.push_back("--image=" + image_file_name_prefix + ".art");
    argv.push_back("--oat-file=" + image_file_name_prefix + ".oat");
    argv.push_back("--oat-location=" + image_file_name_prefix + ".oat");
    argv.push_back(StringPrintf("--base=0x%" PRIx32, base_addr));
    argv.push_back("--compile-pic");
    argv.push_back("--multi-image");
    argv.push_back("--no-generate-debug-info");

    std::vector<std::string> compiler_options = runtime->GetCompilerOptions();
    argv.insert(argv.end(), compiler_options.begin(), compiler_options.end());

    // We must set --android-root.
    const char* android_root = getenv("ANDROID_ROOT");
    CHECK(android_root != nullptr);
    argv.push_back("--android-root=" + std::string(android_root));
    argv.insert(argv.end(), extra_args.begin(), extra_args.end());

    return RunDex2OatOrPatchoat(argv, error_msg);
  }

  bool RelocateBootImage(const std::string& input_image_location,
                         const std::string& output_image_filename,
                         off_t base_offset_delta,
                         std::string* error_msg) {
    Runtime* const runtime = Runtime::Current();
    std::vector<std::string> argv;
    argv.push_back(runtime->GetPatchoatExecutable());
    argv.push_back("--input-image-location=" + input_image_location);
    argv.push_back("--output-image-file=" + output_image_filename);
    argv.push_back(StringPrintf("--base-offset-delta=0x%jx", (intmax_t) base_offset_delta));
    argv.push_back(StringPrintf("--instruction-set=%s", GetInstructionSetString(kRuntimeISA)));

    return RunDex2OatOrPatchoat(argv, error_msg);
  }

  bool GenerateBootImageRelFile(const std::string& input_image_location,
                                const std::string& output_rel_filename,
                                off_t base_offset_delta,
                                std::string* error_msg) {
    Runtime* const runtime = Runtime::Current();
    std::vector<std::string> argv;
    argv.push_back(runtime->GetPatchoatExecutable());
    argv.push_back("--input-image-location=" + input_image_location);
    argv.push_back("--output-image-relocation-file=" + output_rel_filename);
    argv.push_back(StringPrintf("--base-offset-delta=0x%jx", (intmax_t) base_offset_delta));
    argv.push_back(StringPrintf("--instruction-set=%s", GetInstructionSetString(kRuntimeISA)));

    return RunDex2OatOrPatchoat(argv, error_msg);
  }

  bool RunDex2OatOrPatchoat(const std::vector<std::string>& args, std::string* error_msg) {
    int link[2];

    if (pipe(link) == -1) {
      return false;
    }

    pid_t pid = fork();
    if (pid == -1) {
      return false;
    }

    if (pid == 0) {
      // We need dex2oat to actually log things.
      setenv("ANDROID_LOG_TAGS", "*:e", 1);
      dup2(link[1], STDERR_FILENO);
      close(link[0]);
      close(link[1]);
      std::vector<const char*> c_args;
      for (const std::string& str : args) {
        c_args.push_back(str.c_str());
      }
      c_args.push_back(nullptr);
      execv(c_args[0], const_cast<char* const*>(c_args.data()));
      exit(1);
      UNREACHABLE();
    } else {
      close(link[1]);
      char buffer[128];
      memset(buffer, 0, 128);
      ssize_t bytes_read = 0;

      while (TEMP_FAILURE_RETRY(bytes_read = read(link[0], buffer, 128)) > 0) {
        *error_msg += std::string(buffer, bytes_read);
      }
      close(link[0]);
      int status = -1;
      if (waitpid(pid, &status, 0) != -1) {
        return (status == 0);
      }
      return false;
    }
  }

  bool CompileBootImageToDir(
      const std::string& output_dir,
      const std::vector<std::string>& dex2oat_extra_args,
      uint32_t base_addr,
      std::string* error_msg) {
    return CompileBootImage(dex2oat_extra_args, output_dir + "/boot", base_addr, error_msg);
  }

  bool CopyImageChecksumAndSetPatchDelta(
      const std::string& src_image_filename,
      const std::string& dest_image_filename,
      off_t dest_patch_delta,
      std::string* error_msg) {
    std::unique_ptr<File> src_file(OS::OpenFileForReading(src_image_filename.c_str()));
    if (src_file.get() == nullptr) {
      *error_msg = StringPrintf("Failed to open source image file %s", src_image_filename.c_str());
      return false;
    }
    ImageHeader src_header;
    if (!src_file->ReadFully(&src_header, sizeof(src_header))) {
      *error_msg = StringPrintf("Failed to read source image file %s", src_image_filename.c_str());
      return false;
    }

    std::unique_ptr<File> dest_file(OS::OpenFileReadWrite(dest_image_filename.c_str()));
    if (dest_file.get() == nullptr) {
      *error_msg =
          StringPrintf("Failed to open destination image file %s", dest_image_filename.c_str());
      return false;
    }
    ImageHeader dest_header;
    if (!dest_file->ReadFully(&dest_header, sizeof(dest_header))) {
      *error_msg =
          StringPrintf("Failed to read destination image file %s", dest_image_filename.c_str());
      return false;
    }
    dest_header.SetOatChecksum(src_header.GetOatChecksum());
    dest_header.SetPatchDelta(dest_patch_delta);
    if (!dest_file->ResetOffset()) {
      *error_msg =
          StringPrintf(
              "Failed to seek to start of destination image file %s", dest_image_filename.c_str());
      return false;
    }
    if (!dest_file->WriteFully(&dest_header, sizeof(dest_header))) {
      *error_msg =
          StringPrintf("Failed to write to destination image file %s", dest_image_filename.c_str());
      dest_file->Erase();
      return false;
    }
    if (dest_file->FlushCloseOrErase() != 0) {
      *error_msg =
          StringPrintf(
              "Failed to flush/close destination image file %s", dest_image_filename.c_str());
      return false;
    }

    return true;
  }

  bool ReadFully(
      const std::string& filename, std::vector<uint8_t>* contents, std::string* error_msg) {
    std::unique_ptr<File> file(OS::OpenFileForReading(filename.c_str()));
    if (file.get() == nullptr) {
      *error_msg = "Failed to open";
      return false;
    }
    int64_t size = file->GetLength();
    if (size < 0) {
      *error_msg = "Failed to get size";
      return false;
    }
    contents->resize(size);
    if (!file->ReadFully(&(*contents)[0], size)) {
      *error_msg = "Failed to read";
      contents->clear();
      return false;
    }
    return true;
  }

  bool BinaryDiff(
      const std::string& filename1,
      const std::vector<uint8_t>& data1,
      const std::string& filename2,
      const std::vector<uint8_t>& data2,
      std::string* error_msg) {
    if (data1.size() != data1.size()) {
      *error_msg =
          StringPrintf(
              "%s and %s are of different size: %zu vs %zu",
              filename1.c_str(),
              filename2.c_str(),
              data1.size(),
              data2.size());
      return true;
    }
    size_t size = data1.size();
    for (size_t i = 0; i < size; i++) {
      if (data1[i] != data2[i]) {
        *error_msg =
            StringPrintf("%s and %s differ at offset %zu", filename1.c_str(), filename2.c_str(), i);
        return true;
      }
    }

    return false;
  }

  bool BinaryDiff(
      const std::string& filename1, const std::string& filename2, std::string* error_msg) {
    std::string read_error_msg;
    std::vector<uint8_t> image1;
    if (!ReadFully(filename1, &image1, &read_error_msg)) {
      *error_msg = StringPrintf("Failed to read %s: %s", filename1.c_str(), read_error_msg.c_str());
      return true;
    }
    std::vector<uint8_t> image2;
    if (!ReadFully(filename2, &image2, &read_error_msg)) {
      *error_msg = StringPrintf("Failed to read %s: %s", filename2.c_str(), read_error_msg.c_str());
      return true;
    }
    return BinaryDiff(filename1, image1, filename2, image2, error_msg);
  }

  bool IsImageIdenticalToOriginalExceptForRelocation(
      const std::string& relocated_filename,
      const std::string& original_filename,
      const std::string& rel_filename,
      std::string* error_msg) {
    *error_msg = "";
    std::string read_error_msg;
    std::vector<uint8_t> rel;
    if (!ReadFully(rel_filename, &rel, &read_error_msg)) {
      *error_msg =
          StringPrintf("Failed to read %s: %s", rel_filename.c_str(), read_error_msg.c_str());
      return false;
    }
    std::vector<uint8_t> relocated;
    if (!ReadFully(relocated_filename, &relocated, &read_error_msg)) {
      *error_msg =
          StringPrintf("Failed to read %s: %s", relocated_filename.c_str(), read_error_msg.c_str());
      return false;
    }

    size_t image_size = relocated.size();
    if ((image_size % 4) != 0) {
      *error_msg =
          StringPrintf(
              "Relocated image file %s size not multiple of 4: %zu",
                  relocated_filename.c_str(), image_size);
      return false;
    }
    if (image_size > UINT32_MAX) {
      *error_msg =
          StringPrintf(
              "Relocated image file %s too large: %zu" , relocated_filename.c_str(), image_size);
      return false;
    }

    const ImageHeader& relocated_header = *reinterpret_cast<const ImageHeader*>(relocated.data());
    off_t expected_diff = relocated_header.GetPatchDelta();

    if (expected_diff != 0) {
      // Relocated image is expected to differ from the original due to relocation.
      // Unrelocate the image in memory to compensate.
      uint8_t* image_start = relocated.data();
      const uint8_t* rel_end = &rel[rel.size()];
      if (rel.size() < SHA256_DIGEST_LENGTH) {
        *error_msg =
            StringPrintf("Malformed image relocation file %s: too short", rel_filename.c_str());
        return false;
      }
      const uint8_t* rel_ptr = &rel[SHA256_DIGEST_LENGTH];
      // The remaining .rel file consists of offsets at which relocation should've occurred.
      // For each offset, we "unrelocate" the image by subtracting the expected relocation
      // diff value (as specified in the image header).
      //
      // Each offset is encoded as a delta/diff relative to the previous offset. With the
      // very first offset being encoded relative to offset 0.
      // Deltas are encoded using little-endian 7 bits per byte encoding, with all bytes except
      // the last one having the highest bit set.
      uint32_t offset = 0;
      while (rel_ptr != rel_end) {
        uint32_t offset_delta = 0;
        if (DecodeUnsignedLeb128Checked(&rel_ptr, rel_end, &offset_delta)) {
          offset += offset_delta;
          uint32_t *image_value = reinterpret_cast<uint32_t*>(image_start + offset);
          *image_value -= expected_diff;
        } else {
            *error_msg =
                StringPrintf(
                    "Malformed image relocation file %s: "
                    "last byte has it's most significant bit set",
                    rel_filename.c_str());
            return false;
        }
      }
    }

    // Image in memory is now supposed to be identical to the original. Compare it to the original.
    std::vector<uint8_t> original;
    if (!ReadFully(original_filename, &original, &read_error_msg)) {
      *error_msg =
          StringPrintf("Failed to read %s: %s", original_filename.c_str(), read_error_msg.c_str());
      return false;
    }
    if (BinaryDiff(relocated_filename, relocated, original_filename, original, error_msg)) {
      return false;
    }

    // Relocated image is identical to the original, once relocations are taken into account
    return true;
  }
};

TEST_F(PatchoatTest, PatchoatRelocationSameAsDex2oatRelocation) {
#if defined(ART_USE_READ_BARRIER)
  // This test checks that relocating a boot image using patchoat produces the same result as
  // producing the boot image for that relocated base address using dex2oat. To be precise, these
  // two files will have two small differences: the OAT checksum and base address. However, this
  // test takes this into account.

  // Compile boot image into a random directory using dex2oat
  ScratchFile dex2oat_orig_scratch;
  dex2oat_orig_scratch.Unlink();
  std::string dex2oat_orig_dir = dex2oat_orig_scratch.GetFilename();
  ASSERT_EQ(0, mkdir(dex2oat_orig_dir.c_str(), 0700));
  const uint32_t orig_base_addr = 0x60000000;
  // Force deterministic output. We want the boot images created by this dex2oat run and the run
  // below to differ only in their base address.
  std::vector<std::string> dex2oat_extra_args;
  dex2oat_extra_args.push_back("--force-determinism");
  dex2oat_extra_args.push_back("-j1");  // Might not be needed. Causes a 3-5x slowdown.
  std::string error_msg;
  if (!CompileBootImageToDir(dex2oat_orig_dir, dex2oat_extra_args, orig_base_addr, &error_msg)) {
    FAIL() << "CompileBootImage1 failed: " << error_msg;
  }

  // Compile a "relocated" boot image into a random directory using dex2oat. This image is relocated
  // in the sense that it uses a different base address.
  ScratchFile dex2oat_reloc_scratch;
  dex2oat_reloc_scratch.Unlink();
  std::string dex2oat_reloc_dir = dex2oat_reloc_scratch.GetFilename();
  ASSERT_EQ(0, mkdir(dex2oat_reloc_dir.c_str(), 0700));
  const uint32_t reloc_base_addr = 0x70000000;
  if (!CompileBootImageToDir(dex2oat_reloc_dir, dex2oat_extra_args, reloc_base_addr, &error_msg)) {
    FAIL() << "CompileBootImage2 failed: " << error_msg;
  }
  const off_t base_addr_delta = reloc_base_addr - orig_base_addr;

  // Relocate the original boot image using patchoat. The image is relocated by the same amount
  // as the second/relocated image produced by dex2oat.
  ScratchFile patchoat_scratch;
  patchoat_scratch.Unlink();
  std::string patchoat_dir = patchoat_scratch.GetFilename();
  ASSERT_EQ(0, mkdir(patchoat_dir.c_str(), 0700));
  std::string dex2oat_orig_with_arch_dir =
      dex2oat_orig_dir + "/" + GetInstructionSetString(kRuntimeISA);
  // The arch-including symlink is needed by patchoat
  ASSERT_EQ(0, symlink(dex2oat_orig_dir.c_str(), dex2oat_orig_with_arch_dir.c_str()));
  if (!RelocateBootImage(
      dex2oat_orig_dir + "/boot.art",
      patchoat_dir + "/boot.art",
      base_addr_delta,
      &error_msg)) {
    FAIL() << "RelocateBootImage failed: " << error_msg;
  }

  // Assert that patchoat created the same set of .art files as dex2oat
  std::vector<std::string> dex2oat_image_basenames;
  std::vector<std::string> patchoat_image_basenames;
  if (!ListDirFilesEndingWith(dex2oat_reloc_dir, ".art", &dex2oat_image_basenames, &error_msg)) {
    FAIL() << "Failed to list *.art files in " << dex2oat_reloc_dir << ": " << error_msg;
  }
  if (!ListDirFilesEndingWith(patchoat_dir, ".art", &patchoat_image_basenames, &error_msg)) {
    FAIL() << "Failed to list *.art files in " << patchoat_dir << ": " << error_msg;
  }
  std::sort(dex2oat_image_basenames.begin(), dex2oat_image_basenames.end());
  std::sort(patchoat_image_basenames.begin(), patchoat_image_basenames.end());
  // .art file names output by patchoat look like tmp@art-data-<random>-<random>@boot*.art. To
  // compare these with .art file names output by dex2oat we retain only the part of the file name
  // after the last @.
  std::vector<std::string> patchoat_image_shortened_basenames(patchoat_image_basenames.size());
  for (size_t i = 0; i < patchoat_image_basenames.size(); i++) {
    patchoat_image_shortened_basenames[i] =
        patchoat_image_basenames[i].substr(patchoat_image_basenames[i].find_last_of("@") + 1);
  }
  ASSERT_EQ(dex2oat_image_basenames, patchoat_image_shortened_basenames);

  // Patch up the dex2oat-relocated image files so that it looks as though they were relocated by
  // patchoat. patchoat preserves the OAT checksum header field and sets patch delta header field.
  for (const std::string& image_basename : dex2oat_image_basenames) {
    if (!CopyImageChecksumAndSetPatchDelta(
        dex2oat_orig_dir + "/" + image_basename,
        dex2oat_reloc_dir + "/" + image_basename,
        base_addr_delta,
        &error_msg)) {
      FAIL() << "Unable to patch up " << image_basename << ": " << error_msg;
    }
  }

  // Assert that the patchoat-relocated images are identical to the dex2oat-relocated images
  for (size_t i = 0; i < dex2oat_image_basenames.size(); i++) {
    const std::string& dex2oat_image_basename = dex2oat_image_basenames[i];
    const std::string& dex2oat_image_filename = dex2oat_reloc_dir + "/" + dex2oat_image_basename;
    const std::string& patchoat_image_filename = patchoat_dir + "/" + patchoat_image_basenames[i];
    if (BinaryDiff(dex2oat_image_filename, patchoat_image_filename, &error_msg)) {
      FAIL() << "patchoat- and dex2oat-relocated variants of " << dex2oat_image_basename
          << " differ: " << error_msg;
    }
  }

  ClearDirectory(dex2oat_orig_dir.c_str(), /*recursive*/ true);
  ClearDirectory(dex2oat_reloc_dir.c_str(), /*recursive*/ true);
  ClearDirectory(patchoat_dir.c_str(), /*recursive*/ true);
  rmdir(dex2oat_orig_dir.c_str());
  rmdir(dex2oat_reloc_dir.c_str());
  rmdir(patchoat_dir.c_str());
#else
  LOG(INFO) << "Skipping PatchoatRelocationSameAsDex2oatRelocation";
  // Force-print to std::cout so it's also outside the logcat.
  std::cout << "Skipping PatchoatRelocationSameAsDex2oatRelocation" << std::endl;
#endif
}

TEST_F(PatchoatTest, RelFileSufficientToUnpatch) {
  // This test checks that a boot image relocated using patchoat can be unrelocated using the .rel
  // file created by patchoat.

  // This test doesn't work when heap poisoning is enabled because some of the
  // references are negated. b/72117833 is tracking the effort to have patchoat
  // and its tests support heap poisoning.
  TEST_DISABLED_FOR_HEAP_POISONING();

  // Compile boot image into a random directory using dex2oat
  ScratchFile dex2oat_orig_scratch;
  dex2oat_orig_scratch.Unlink();
  std::string dex2oat_orig_dir = dex2oat_orig_scratch.GetFilename();
  ASSERT_EQ(0, mkdir(dex2oat_orig_dir.c_str(), 0700));
  const uint32_t orig_base_addr = 0x60000000;
  std::vector<std::string> dex2oat_extra_args;
  std::string error_msg;
  if (!CompileBootImageToDir(dex2oat_orig_dir, dex2oat_extra_args, orig_base_addr, &error_msg)) {
    FAIL() << "CompileBootImage1 failed: " << error_msg;
  }

  // Generate image relocation file for the original boot image
  ScratchFile rel_scratch;
  rel_scratch.Unlink();
  std::string rel_dir = rel_scratch.GetFilename();
  ASSERT_EQ(0, mkdir(rel_dir.c_str(), 0700));
  std::string dex2oat_orig_with_arch_dir =
      dex2oat_orig_dir + "/" + GetInstructionSetString(kRuntimeISA);
  // The arch-including symlink is needed by patchoat
  ASSERT_EQ(0, symlink(dex2oat_orig_dir.c_str(), dex2oat_orig_with_arch_dir.c_str()));
  off_t base_addr_delta = 0x100000;
  if (!GenerateBootImageRelFile(
      dex2oat_orig_dir + "/boot.art",
      rel_dir + "/boot.art.rel",
      base_addr_delta,
      &error_msg)) {
    FAIL() << "RelocateBootImage failed: " << error_msg;
  }

  // Relocate the original boot image using patchoat
  ScratchFile relocated_scratch;
  relocated_scratch.Unlink();
  std::string relocated_dir = relocated_scratch.GetFilename();
  ASSERT_EQ(0, mkdir(relocated_dir.c_str(), 0700));
  // Use a different relocation delta from the one used when generating .rel files above. This is
  // to make sure .rel files are not specific to a particular relocation delta.
  base_addr_delta -= 0x10000;
  if (!RelocateBootImage(
      dex2oat_orig_dir + "/boot.art",
      relocated_dir + "/boot.art",
      base_addr_delta,
      &error_msg)) {
    FAIL() << "RelocateBootImage failed: " << error_msg;
  }

  // Assert that patchoat created the same set of .art and .art.rel files
  std::vector<std::string> rel_basenames;
  std::vector<std::string> relocated_image_basenames;
  if (!ListDirFilesEndingWith(rel_dir, "", &rel_basenames, &error_msg)) {
    FAIL() << "Failed to list *.art.rel files in " << rel_dir << ": " << error_msg;
  }
  if (!ListDirFilesEndingWith(relocated_dir, ".art", &relocated_image_basenames, &error_msg)) {
    FAIL() << "Failed to list *.art files in " << relocated_dir << ": " << error_msg;
  }
  std::sort(rel_basenames.begin(), rel_basenames.end());
  std::sort(relocated_image_basenames.begin(), relocated_image_basenames.end());

  // .art and .art.rel file names output by patchoat look like
  // tmp@art-data-<random>-<random>@boot*.art, encoding the name of the directory in their name.
  // To compare these with each other, we retain only the part of the file name after the last @,
  // and we also drop the extension.
  std::vector<std::string> rel_shortened_basenames(rel_basenames.size());
  std::vector<std::string> relocated_image_shortened_basenames(relocated_image_basenames.size());
  for (size_t i = 0; i < rel_basenames.size(); i++) {
    rel_shortened_basenames[i] = rel_basenames[i].substr(rel_basenames[i].find_last_of("@") + 1);
    rel_shortened_basenames[i] =
        rel_shortened_basenames[i].substr(0, rel_shortened_basenames[i].find("."));
  }
  for (size_t i = 0; i < relocated_image_basenames.size(); i++) {
    relocated_image_shortened_basenames[i] =
        relocated_image_basenames[i].substr(relocated_image_basenames[i].find_last_of("@") + 1);
    relocated_image_shortened_basenames[i] =
        relocated_image_shortened_basenames[i].substr(
            0, relocated_image_shortened_basenames[i].find("."));
  }
  ASSERT_EQ(rel_shortened_basenames, relocated_image_shortened_basenames);

  // For each image file, assert that unrelocating the image produces its original version
  for (size_t i = 0; i < relocated_image_basenames.size(); i++) {
    const std::string& original_image_filename =
        dex2oat_orig_dir + "/" + relocated_image_shortened_basenames[i] + ".art";
    const std::string& relocated_image_filename =
        relocated_dir + "/" + relocated_image_basenames[i];
    const std::string& rel_filename = rel_dir + "/" + rel_basenames[i];

    // Assert that relocated image differs from the original
    if (!BinaryDiff(original_image_filename, relocated_image_filename, &error_msg)) {
      FAIL() << "Relocated image " << relocated_image_filename
          << " identical to the original image " << original_image_filename;
    }

    // Assert that relocated image is identical to the original except for relocations described in
    // the .rel file
    if (!IsImageIdenticalToOriginalExceptForRelocation(
        relocated_image_filename, original_image_filename, rel_filename, &error_msg)) {
      FAIL() << "Unrelocating " << relocated_image_filename << " using " << rel_filename
          << " did not produce the same output as " << original_image_filename << ": " << error_msg;
    }

    // Assert that the digest of original image in .rel file is as expected
    std::vector<uint8_t> original;
    if (!ReadFully(original_image_filename, &original, &error_msg)) {
      FAIL() << "Failed to read original image " << original_image_filename;
    }
    std::vector<uint8_t> rel;
    if (!ReadFully(rel_filename, &rel, &error_msg)) {
      FAIL() << "Failed to read image relocation file " << rel_filename;
    }
    uint8_t original_image_digest[SHA256_DIGEST_LENGTH];
    SHA256(original.data(), original.size(), original_image_digest);
    const uint8_t* original_image_digest_in_rel_file = rel.data();
    if (memcmp(original_image_digest_in_rel_file, original_image_digest, SHA256_DIGEST_LENGTH)) {
      FAIL() << "Digest of original image in " << rel_filename << " does not match the original"
          " image " << original_image_filename;
    }
  }

  ClearDirectory(dex2oat_orig_dir.c_str(), /*recursive*/ true);
  ClearDirectory(rel_dir.c_str(), /*recursive*/ true);
  ClearDirectory(relocated_dir.c_str(), /*recursive*/ true);

  rmdir(dex2oat_orig_dir.c_str());
  rmdir(rel_dir.c_str());
  rmdir(relocated_dir.c_str());
}

}  // namespace art