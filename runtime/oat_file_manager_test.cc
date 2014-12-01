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

#include "oat_file_manager.h"

#include <fstream>
#include <string>

#include <gtest/gtest.h>

#include "common_runtime_test.h"
#include "os.h"

namespace art {

class OatFileManagerTest : public CommonRuntimeTest {
 public:
  virtual void SetUp() {
    CommonRuntimeTest::SetUp();

    // Create a scratch directory to work from.
    scratch_dir_ = android_data_ + "/OatFileManagerTest";
    ASSERT_EQ(0, mkdir(scratch_dir_.c_str(), 0700));

    // Create a subdirectory in scratch for the current isa.
    // This is the location that will be used for odex files in the tests.
    isa_dir_ = scratch_dir_ + "/" + GetInstructionSetString(kRuntimeISA);
    ASSERT_EQ(0, mkdir(isa_dir_.c_str(), 0700));

    // Verify the environment is as we expect
    ASSERT_TRUE(OS::FileExists(GetImageFile().c_str()))
      << "Expected pre-compiled boot image to be at: " << GetImageFile();
    ASSERT_TRUE(OS::FileExists(GetDexSrc1().c_str()))
      << "Expected dex file to be at: " << GetDexSrc1();
    ASSERT_TRUE(OS::FileExists(GetDexSrc2().c_str()))
      << "Expected dex file to be at: " << GetDexSrc2();
  }

  virtual void SetUpRuntimeOptions(RuntimeOptions* options) {
    // Set up the image location.
    options->push_back(std::make_pair("-Ximage:" + GetImageLocation(),
          nullptr));

    // Make sure compilercallbacks are not set so that relocation will be
    // enabled.
    //
    // TODO: Having relocation enabled causes the tests to be flaky on the
    // host, failing with an error about mmap overlap. Why does this happen,
    // and how do we fix it?
    for (std::pair<std::string, const void*>& pair : *options) {
      if (pair.first == "compilercallbacks") {
        pair.second = nullptr;
      }
    }
  }

  virtual void TearDown() {
    ClearDirectory(isa_dir_.c_str());
    ASSERT_EQ(0, rmdir(isa_dir_.c_str()));

    ClearDirectory(scratch_dir_.c_str());
    ASSERT_EQ(0, rmdir(scratch_dir_.c_str()));

    CommonRuntimeTest::TearDown();
  }

  void Copy(std::string src, std::string dst) {
    std::ifstream  src_stream(src, std::ios::binary);
    std::ofstream  dst_stream(dst, std::ios::binary);

    dst_stream << src_stream.rdbuf();
  }

  // Returns the directory where the pre-compiled core.art can be found.
  // TODO: We should factor out this into common tests somewhere rather than
  // re-hardcoding it here (This was copied originally from the elf writer
  // test).
  std::string GetImageDirectory() {
    if (IsHost()) {
      const char* host_dir = getenv("ANDROID_HOST_OUT");
      CHECK(host_dir != NULL);
      return std::string(host_dir) + "/framework";
    } else {
      return std::string("/data/art-test");
    }
  }

  std::string GetImageLocation() {
    return GetImageDirectory() + "/core.art";
  }

  std::string GetImageFile() {
    return GetImageDirectory() + "/" + GetInstructionSetString(kRuntimeISA)
      + "/core.art";
  }

  std::string GetDexSrc1() {
    return GetTestDexFileName("Main");
  }

  std::string GetDexSrc2() {
    return GetTestDexFileName("Nested");
  }

  // Scratch directory, for dex and odex files (oat files will go in the
  // dalvik cache).
  std::string GetScratchDir() {
    return scratch_dir_;
  }

  // ISA directory is the subdirectory in the scratch directory where odex
  // files should be located.
  std::string GetISADir() {
    return isa_dir_;
  }

 private:
  std::string scratch_dir_;
  std::string isa_dir_;
};

class OatFileManagerNoDex2OatTest : public OatFileManagerTest {
 public:
  virtual void SetUpRuntimeOptions(RuntimeOptions* options) {
    OatFileManagerTest::SetUpRuntimeOptions(options);
    options->push_back(std::make_pair("-Xnodex2oat", nullptr));
  }
};

// Case: We have a DEX file, but no OAT file for it.
// Expect: The oat file status is kNeedsGeneration.
TEST_F(OatFileManagerTest, DexNoOat) {
  std::string dex_location = GetScratchDir() + "/DexNoOat.jar";
  Copy(GetDexSrc1(), dex_location);

  OatFileManager oat_file_manager(dex_location.c_str(), kRuntimeISA);

  EXPECT_EQ(OatFileManager::kNeedsGeneration, oat_file_manager.GetStatus());

  EXPECT_TRUE(oat_file_manager.DexFileExists());
  EXPECT_FALSE(oat_file_manager.IsInBootClassPath());
  EXPECT_FALSE(oat_file_manager.OdexFileExists());
  EXPECT_TRUE(oat_file_manager.OdexFileIsOutOfDate());
  EXPECT_FALSE(oat_file_manager.OdexFileIsRelocated());
  EXPECT_FALSE(oat_file_manager.OatFileExists());
  EXPECT_TRUE(oat_file_manager.OatFileIsOutOfDate());
  EXPECT_FALSE(oat_file_manager.OatFileIsRelocated());
}

// Case: We have a DEX file and up-to-date OAT file for it.
// Expect: The oat file status is kUpToDate.
TEST_F(OatFileManagerTest, OatUpToDate) {
  std::string dex_location = GetScratchDir() + "/OatUpToDate.jar";
  Copy(GetDexSrc1(), dex_location);

  OatFileManager oat_file_manager(dex_location.c_str(), kRuntimeISA);
  ASSERT_TRUE(oat_file_manager.GenerateOatFile());

  EXPECT_EQ(OatFileManager::kUpToDate, oat_file_manager.GetStatus());

  EXPECT_TRUE(oat_file_manager.DexFileExists());
  EXPECT_FALSE(oat_file_manager.IsInBootClassPath());
  EXPECT_FALSE(oat_file_manager.OdexFileExists());
  EXPECT_TRUE(oat_file_manager.OdexFileIsOutOfDate());
  EXPECT_FALSE(oat_file_manager.OdexFileIsRelocated());
  EXPECT_TRUE(oat_file_manager.OatFileExists());
  EXPECT_FALSE(oat_file_manager.OatFileIsOutOfDate());
  EXPECT_TRUE(oat_file_manager.OatFileIsRelocated());
}

// Case: We have a DEX file and out of date OAT file.
// Expect: The oat file status is kNeedsGeneration.
TEST_F(OatFileManagerTest, OatOutOfDate) {
  std::string dex_location = GetScratchDir() + "/OatOutOfDate.jar";

  // We create a dex, generate an oat for it, then overwrite the dex with a
  // different dex to make the oat out of date.
  OatFileManager oat_file_manager(dex_location.c_str(), kRuntimeISA);
  Copy(GetDexSrc1(), dex_location);
  ASSERT_TRUE(oat_file_manager.GenerateOatFile());
  Copy(GetDexSrc2(), dex_location);

  EXPECT_EQ(OatFileManager::kNeedsGeneration, oat_file_manager.GetStatus());

  EXPECT_TRUE(oat_file_manager.DexFileExists());
  EXPECT_FALSE(oat_file_manager.IsInBootClassPath());
  EXPECT_FALSE(oat_file_manager.OdexFileExists());
  EXPECT_TRUE(oat_file_manager.OdexFileIsOutOfDate());
  EXPECT_FALSE(oat_file_manager.OdexFileIsRelocated());
  EXPECT_TRUE(oat_file_manager.OatFileExists());
  EXPECT_TRUE(oat_file_manager.OatFileIsOutOfDate());
  EXPECT_FALSE(oat_file_manager.OatFileIsRelocated());
}

// Case: We have a DEX file and an ODEX file, but no OAT file.
// Expect: The oat file status is kNeedsRelocation.
TEST_F(OatFileManagerTest, DexOdexNoOat) {
  std::string dex_location = GetScratchDir() + "/DexOdexNoOat.jar";
  std::string odex_location = GetISADir() + "/DexOdexNoOat.odex";

  // Create the dex file
  Copy(GetDexSrc1(), dex_location);

  // Create the odex file
  // For this operation, we temporally redirect the dalvik cache so dex2oat
  // doesn't find the relocated image file.
  std::string android_data_tmp = GetScratchDir() + "AndroidDataTmp";
  setenv("ANDROID_DATA", android_data_tmp.c_str(), 1);
  std::vector<std::string> args;
  args.push_back("--dex-file=" + dex_location);
  args.push_back("--oat-file=" + odex_location);
  args.push_back("--include-patch-information");
  args.push_back("--runtime-arg");
  args.push_back("-Xnorelocate");
  std::string error_msg;
  ASSERT_TRUE(OatFileManager::Dex2Oat(args, &error_msg)) << error_msg;
  setenv("ANDROID_DATA", android_data_.c_str(), 1);

  // Verify the status.
  OatFileManager oat_file_manager(dex_location.c_str(), kRuntimeISA);

  EXPECT_EQ(OatFileManager::kNeedsRelocation, oat_file_manager.GetStatus());

  EXPECT_TRUE(oat_file_manager.DexFileExists());
  EXPECT_FALSE(oat_file_manager.IsInBootClassPath());
  EXPECT_TRUE(oat_file_manager.OdexFileExists());
  EXPECT_FALSE(oat_file_manager.OdexFileIsOutOfDate());
  EXPECT_FALSE(oat_file_manager.OdexFileIsRelocated());
  EXPECT_FALSE(oat_file_manager.OatFileExists());
  EXPECT_TRUE(oat_file_manager.OatFileIsOutOfDate());
  EXPECT_FALSE(oat_file_manager.OatFileIsRelocated());
}

// Case: We have a DEX file and a PIC ODEX file, but no OAT file.
// Expect: The oat file status is kUpToDate, because PIC needs no relocation.
TEST_F(OatFileManagerTest, DexPicOdexNoOat) {
  std::string dex_location = GetScratchDir() + "/DexPicOdexNoOat.jar";
  std::string odex_location = GetISADir() + "/DexPicOdexNoOat.odex";

  // Create the dex file
  Copy(GetDexSrc1(), dex_location);

  // Create the odex file
  // For this operation, we temporally redirect the dalvik cache so dex2oat
  // doesn't find the relocated image file.
  std::string android_data_tmp = GetScratchDir() + "AndroidDataTmp";
  setenv("ANDROID_DATA", android_data_tmp.c_str(), 1);
  std::vector<std::string> args;
  args.push_back("--dex-file=" + dex_location);
  args.push_back("--oat-file=" + odex_location);
  args.push_back("--compile-pic");
  args.push_back("--runtime-arg");
  args.push_back("-Xnorelocate");
  std::string error_msg;
  ASSERT_TRUE(OatFileManager::Dex2Oat(args, &error_msg)) << error_msg;
  setenv("ANDROID_DATA", android_data_.c_str(), 1);

  // Verify the status.
  OatFileManager oat_file_manager(dex_location.c_str(), kRuntimeISA);

  EXPECT_EQ(OatFileManager::kUpToDate, oat_file_manager.GetStatus());

  EXPECT_TRUE(oat_file_manager.DexFileExists());
  EXPECT_FALSE(oat_file_manager.IsInBootClassPath());
  EXPECT_TRUE(oat_file_manager.OdexFileExists());
  EXPECT_FALSE(oat_file_manager.OdexFileIsOutOfDate());
  EXPECT_TRUE(oat_file_manager.OdexFileIsRelocated());
  EXPECT_FALSE(oat_file_manager.OatFileExists());
  EXPECT_TRUE(oat_file_manager.OatFileIsOutOfDate());
  EXPECT_FALSE(oat_file_manager.OatFileIsRelocated());
}

// Case: We have a DEX file and up-to-date OAT file for it.
// Expect: We should load an executable dex file.
TEST_F(OatFileManagerTest, LoadOatUpToDate) {
  std::string dex_location = GetScratchDir() + "/LoadOatUpToDate.jar";

  // We generate the up to date oat using an oat file manager unrelated to the
  // one we want to test.
  Copy(GetDexSrc1(), dex_location);
  OatFileManager oat_file_generator(dex_location.c_str(), kRuntimeISA);
  ASSERT_TRUE(oat_file_generator.GenerateOatFile());
  EXPECT_EQ(OatFileManager::kUpToDate, oat_file_generator.GetStatus());

  // Load the oat using an executable oat file manager.
  ExecutableOatFileManager oat_file_manager(dex_location.c_str());

  std::vector<const DexFile*> dex_files;
  std::unique_ptr<OatFile> oat_file;
  EXPECT_TRUE(oat_file_manager.LoadDexFiles(&dex_files, oat_file));
  ASSERT_TRUE(oat_file.get() != nullptr);
  EXPECT_TRUE(oat_file->IsExecutable());
  EXPECT_EQ(1u, dex_files.size());
}

// Case: We have a DEX file.
// Expect: We should load an executable dex file from an alternative oat
// location.
TEST_F(OatFileManagerTest, LoadDexNoAlternateOat) {
  std::string dex_location = GetScratchDir() + "/LoadDexNoAlternateOat.jar";
  std::string oat_location = GetScratchDir() + "/LoadDexNoAlternateOat.oat";

  Copy(GetDexSrc1(), dex_location);

  ExecutableOatFileManager oat_file_manager(dex_location.c_str(),
      oat_location.c_str());
  ASSERT_TRUE(oat_file_manager.MakeUpToDate());

  std::vector<const DexFile*> dex_files;
  std::unique_ptr<OatFile> oat_file;
  EXPECT_TRUE(oat_file_manager.LoadDexFiles(&dex_files, oat_file));
  ASSERT_TRUE(oat_file.get() != nullptr);
  EXPECT_TRUE(oat_file->IsExecutable());
  EXPECT_EQ(1u, dex_files.size());

  EXPECT_TRUE(OS::FileExists(oat_location.c_str()));

  // Verify it didn't create an oat in the default location.
  OatFileManager ofm(dex_location.c_str(), kRuntimeISA);
  EXPECT_FALSE(ofm.OatFileExists());
}

// Case: We have a DEX file only, and dex2oat is disabled.
// Expect: We should load the original dex file.
TEST_F(OatFileManagerNoDex2OatTest, LoadDexNoOat) {
  std::string dex_location = GetScratchDir() + "/LoadDexNoOat.jar";
  Copy(GetDexSrc1(), dex_location);

  // Load the dex using an executable oat file manager.
  ExecutableOatFileManager oat_file_manager(dex_location.c_str());

  std::vector<const DexFile*> dex_files;
  std::unique_ptr<OatFile> oat_file;
  EXPECT_TRUE(oat_file_manager.LoadDexFiles(&dex_files, oat_file));
  EXPECT_EQ(nullptr, oat_file.get());
  EXPECT_EQ(1u, dex_files.size());
}

// Case: We have a DEX file and an ODEX file, no OAT file, and dex2oat is
// disabled.
// Expect: We should load the odex file non-executable.
TEST_F(OatFileManagerNoDex2OatTest, LoadDexOdexNoOat) {
  std::string dex_location = GetScratchDir() + "/LoadDexOdexNoOat.jar";
  std::string odex_location = GetISADir() + "/LoadDexOdexNoOat.odex";

  // Create the dex file
  Copy(GetDexSrc1(), dex_location);

  // Create the odex file
  // For this operation, we temporally redirect the dalvik cache so dex2oat
  // doesn't find the relocated image file.
  std::string android_data_tmp = GetScratchDir() + "AndroidDataTmp";
  setenv("ANDROID_DATA", android_data_tmp.c_str(), 1);
  std::vector<std::string> args;
  args.push_back("--dex-file=" + dex_location);
  args.push_back("--oat-file=" + odex_location);
  args.push_back("--include-patch-information");
  args.push_back("--runtime-arg");
  args.push_back("-Xnorelocate");
  std::string error_msg;
  ASSERT_TRUE(OatFileManager::Dex2Oat(args, &error_msg)) << error_msg;
  setenv("ANDROID_DATA", android_data_.c_str(), 1);

  // Load the oat using an executable oat file manager.
  ExecutableOatFileManager oat_file_manager(dex_location.c_str());

  std::vector<const DexFile*> dex_files;
  std::unique_ptr<OatFile> oat_file;
  EXPECT_TRUE(oat_file_manager.LoadDexFiles(&dex_files, oat_file));
  ASSERT_TRUE(oat_file.get() != nullptr);
  EXPECT_FALSE(oat_file->IsExecutable());
  EXPECT_EQ(1u, dex_files.size());
}

// TODO: More Tests:
//  * Test multidex files
//  * Test using secondary isa

}  // namespace art
