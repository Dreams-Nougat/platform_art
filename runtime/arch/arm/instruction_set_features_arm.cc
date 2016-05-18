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

#include "instruction_set_features_arm.h"

#if defined(ART_TARGET_ANDROID) && defined(__arm__)
#include <sys/auxv.h>
#include <asm/hwcap.h>
#endif

#include "signal.h"
#include <fstream>

#include "base/stringprintf.h"
#include "utils.h"  // For Trim.

#if defined(__arm__)
extern "C" bool artCheckForArmSdivInstruction();
extern "C" bool artCheckForArmv8AInstructions();
#endif

namespace art {

const ArmInstructionSetFeatures* ArmInstructionSetFeatures::FromVariant(
    const std::string& variant, std::string* error_msg) {
  // Assume all ARM processors are SMP.
  // TODO: set the SMP support based on variant.
  const bool smp = true;

  static const char* arm_variants_with_armv8a[] =
  { "cortex-a53", "cortex-a57", "cortex-a35", "cortex-a72", "cortex-a53.a57", "cyclone", "denver" };
  bool has_armv8a =
      FindVariantInArray(arm_variants_with_armv8a, arraysize(arm_variants_with_armv8a), variant);

  // SDIV is always available on ARMv8-A. Look for divide support in other variants.
  static const char* non_armv8_variants_with_div[] = {
          "cortex-a7", "cortex-a12", "cortex-a15", "cortex-a17",  "krait", "swift" };

  bool has_div = has_armv8a || FindVariantInArray(non_armv8_variants_with_div,
                                                  arraysize(non_armv8_variants_with_div),
                                                  variant);

  // ARMv8-A CPUs that support 32bit have atomic 64bit accesses.
  // Look for other variants that have LPAE support. LPAE support implies atomic LDRD/STRD.
  static const char* non_armv8_variants_with_lpae[] = { "cortex-a7", "cortex-a15", "krait" };
  bool has_atomic_ldrd_strd =
      has_armv8a || FindVariantInArray(non_armv8_variants_with_lpae,
                                       arraysize(non_armv8_variants_with_lpae),
                                       variant);

  if (has_div == false && has_atomic_ldrd_strd == false) {
    // Avoid unsupported variants.
    static const char* unsupported_arm_variants[] = {
        // ARM processors that aren't ARMv7 compatible aren't supported.
        "arm2", "arm250", "arm3", "arm6", "arm60", "arm600", "arm610", "arm620",
        "cortex-m0", "cortex-m0plus", "cortex-m1",
        "fa526", "fa626", "fa606te", "fa626te", "fmp626", "fa726te",
        "iwmmxt", "iwmmxt2",
        "strongarm", "strongarm110", "strongarm1100", "strongarm1110",
        "xscale"
    };
    if (FindVariantInArray(unsupported_arm_variants, arraysize(unsupported_arm_variants),
                           variant)) {
      *error_msg = StringPrintf("Attempt to use unsupported ARM variant: %s", variant.c_str());
      return nullptr;
    }
    // Warn if the variant is unknown.
    // TODO: some of the variants below may have feature support, but that support is currently
    //       unknown so we'll choose conservative (sub-optimal) defaults without warning.
    // TODO: some of the architectures may not support all features required by ART and should be
    //       moved to unsupported_arm_variants[] above.
    static const char* arm_variants_without_known_features[] = {
        "default",
        "arm7", "arm7m", "arm7d", "arm7dm", "arm7di", "arm7dmi", "arm70", "arm700", "arm700i",
        "arm710", "arm710c", "arm7100", "arm720", "arm7500", "arm7500fe", "arm7tdmi", "arm7tdmi-s",
        "arm710t", "arm720t", "arm740t",
        "arm8", "arm810",
        "arm9", "arm9e", "arm920", "arm920t", "arm922t", "arm946e-s", "arm966e-s", "arm968e-s",
        "arm926ej-s", "arm940t", "arm9tdmi",
        "arm10tdmi", "arm1020t", "arm1026ej-s", "arm10e", "arm1020e", "arm1022e",
        "arm1136j-s", "arm1136jf-s",
        "arm1156t2-s", "arm1156t2f-s", "arm1176jz-s", "arm1176jzf-s",
        "cortex-a5", "cortex-a8", "cortex-a9", "cortex-a9-mp", "cortex-r4f",
        "marvell-pj4", "mpcore", "mpcorenovfp"
    };
    if (!FindVariantInArray(arm_variants_without_known_features,
                            arraysize(arm_variants_without_known_features),
                            variant)) {
      LOG(WARNING) << "Unknown instruction set features for ARM CPU variant (" << variant
          << ") using conservative defaults";
    }
  }

  return new ArmInstructionSetFeatures(smp, has_div, has_atomic_ldrd_strd, has_armv8a);
}

const ArmInstructionSetFeatures* ArmInstructionSetFeatures::FromBitmap(uint32_t bitmap) {
  bool smp = (bitmap & kSmpBitfield) != 0;
  bool has_div = (bitmap & kDivBitfield) != 0;
  bool has_atomic_ldrd_strd = (bitmap & kAtomicLdrdStrdBitfield) != 0;
  bool has_armv8a = (bitmap & kARMv8A) != 0;
  return new ArmInstructionSetFeatures(smp, has_div, has_atomic_ldrd_strd, has_armv8a);
}

const ArmInstructionSetFeatures* ArmInstructionSetFeatures::FromCppDefines() {
  const bool smp = true;
#if defined(__ARM_ARCH_8A__)
  const bool has_armv8a = true;
#else
  const bool has_armv8a = false;
#endif
  // SDIV is always available on ARMv8-A.
#if defined(__ARM_ARCH_8A__) || defined(__ARM_ARCH_EXT_IDIV__)
  const bool has_div = true;
#else
  const bool has_div = false;
#endif
#if defined(__ARM_ARCH_8A__) || defined(__ARM_FEATURE_LPAE)
  // ARMv8-A implies atomic 64bit accesses.
  const bool has_atomic_ldrd_strd = true;
#else
  const bool has_atomic_ldrd_strd = false;
#endif
  return new ArmInstructionSetFeatures(smp, has_div, has_atomic_ldrd_strd, has_armv8a);
}

const ArmInstructionSetFeatures* ArmInstructionSetFeatures::FromCpuInfo() {
  // Look in /proc/cpuinfo for features we need.  Only use this when we can guarantee that
  // the kernel puts the appropriate feature flags in here.  Sometimes it doesn't.
  bool smp = false;
  bool has_atomic_ldrd_strd = false;
  bool has_div = false;
  bool has_armv8a = false;

  std::ifstream in("/proc/cpuinfo");
  if (!in.fail()) {
    while (!in.eof()) {
      std::string line;
      std::getline(in, line);
      if (!in.eof()) {
        LOG(INFO) << "cpuinfo line: " << line;
        if (line.find("Features") != std::string::npos) {
          LOG(INFO) << "found features";
          if (line.find("idivt") != std::string::npos) {
            // We always expect both ARM and Thumb divide instructions to be available or not
            // available.
            CHECK_NE(line.find("idiva"), std::string::npos);
            has_div = true;
          }
          if (line.find("lpae") != std::string::npos) {
            has_atomic_ldrd_strd = true;
          }
        } else if (line.find("processor") != std::string::npos &&
            line.find(": 1") != std::string::npos) {
          smp = true;
        } else if (line.find("architecture") != std::string::npos &&
                   line.find(": 8") != std::string::npos) {
          LOG(INFO) << "found architecture ARMv8";
          // Android is only run on A cores, so ARMv8 implies ARMv8-A.
          has_armv8a = true;
          // SDIV is always available on ARMv8-A.
          has_div = true;
          // ARMv8-A CPUs that support 32bit have atomic 64bit accesses.
          has_atomic_ldrd_strd = true;
        }
      }
    }
    in.close();
  } else {
    LOG(ERROR) << "Failed to open /proc/cpuinfo";
  }

  return new ArmInstructionSetFeatures(smp, has_div, has_atomic_ldrd_strd, has_armv8a);
}

const ArmInstructionSetFeatures* ArmInstructionSetFeatures::FromHwcap() {
  bool smp = sysconf(_SC_NPROCESSORS_CONF) > 1;

  bool has_div = false;
  bool has_atomic_ldrd_strd = false;
  bool has_armv8a = false;

#if defined(ART_TARGET_ANDROID)
#if defined(__ARM_ARCH) && (__ARM_ARCH >= 8)
  has_armv8a = true;
  // SDIV is always available on ARMv8-A.
  has_div = true;
  // ARMv8-A CPUs that support 32bit have atomic 64bit accesses.
  has_atomic_ldrd_strd = true;
#elif defined(__arm__)
  uint64_t hwcaps = getauxval(AT_HWCAP);
  LOG(INFO) << "hwcaps=" << hwcaps;
  if ((hwcaps & HWCAP_IDIVT) != 0) {
    // We always expect both ARM and Thumb divide instructions to be available or not
    // available.
    CHECK_NE(hwcaps & HWCAP_IDIVA, 0U);
    has_div = true;
  }
  if ((hwcaps & HWCAP_LPAE) != 0) {
    has_atomic_ldrd_strd = true;
  }
#endif
#endif

  return new ArmInstructionSetFeatures(smp, has_div, has_atomic_ldrd_strd, has_armv8a);
}

// A signal handler called by a fault for an illegal instruction.  We record the fact in r0
// and then increment the PC in the signal context to return to the next instruction.  We know the
// instruction is an sdiv (4 bytes long).
static void bad_inst_handle(int signo ATTRIBUTE_UNUSED, siginfo_t* si ATTRIBUTE_UNUSED,
                            void* data) {
#if defined(__arm__)
  struct ucontext *uc = (struct ucontext *)data;
  struct sigcontext *sc = &uc->uc_mcontext;
  sc->arm_r0 = 0;     // Set R0 to #0 to signal error.
  sc->arm_pc += 4;    // Skip offending instruction.
#else
  UNUSED(data);
#endif
}

const ArmInstructionSetFeatures* ArmInstructionSetFeatures::FromAssembly() {
  const bool smp = true;

  // See if SDIV and ARMv8-A instructions are available. Register a signal handler and
  // try to execute an sdiv instruction. If we get a SIGILL then it's not supported.
  struct sigaction sa, osa;
  sa.sa_flags = SA_ONSTACK | SA_RESTART | SA_SIGINFO;
  sa.sa_sigaction = bad_inst_handle;
  sigaction(SIGILL, &sa, &osa);

  bool has_div = false;
  bool has_armv8a = false;
#if defined(__arm__)
  if (artCheckForArmSdivInstruction()) {
    has_div = true;
  }
  if (artCheckForArmv8AInstructions()) {
    has_armv8a = true;
  }
#endif

  // Restore the signal handler.
  sigaction(SIGILL, &osa, nullptr);

  // Use compile time features to "detect" LPAE support.
  // TODO: write an assembly LPAE support test.
#if defined(__ARM_FEATURE_LPAE)
  const bool has_lpae = true;
#else
  const bool has_lpae = false;
#endif
  return new ArmInstructionSetFeatures(smp, has_div, has_lpae, has_armv8a);
}

bool ArmInstructionSetFeatures::Equals(const InstructionSetFeatures* other) const {
  if (kArm != other->GetInstructionSet()) {
    return false;
  }
  const ArmInstructionSetFeatures* other_as_arm = other->AsArmInstructionSetFeatures();
  return IsSmp() == other_as_arm->IsSmp() &&
      has_div_ == other_as_arm->has_div_ &&
      has_atomic_ldrd_strd_ == other_as_arm->has_atomic_ldrd_strd_ &&
      has_armv8a_ == other_as_arm->has_armv8a_;
}

uint32_t ArmInstructionSetFeatures::AsBitmap() const {
  return (IsSmp() ? kSmpBitfield : 0) |
      (has_div_ ? kDivBitfield : 0) |
      (has_atomic_ldrd_strd_ ? kAtomicLdrdStrdBitfield : 0) |
      (has_armv8a_ ? kARMv8A : 0);
}

std::string ArmInstructionSetFeatures::GetFeatureString() const {
  std::string result;
  if (IsSmp()) {
    result += "smp";
  } else {
    result += "-smp";
  }
  if (has_div_) {
    result += ",div";
  } else {
    result += ",-div";
  }
  if (has_atomic_ldrd_strd_) {
    result += ",atomic_ldrd_strd";
  } else {
    result += ",-atomic_ldrd_strd";
  }
  if (has_armv8a_) {
    result += ",armv8a";
  } else {
    result += ",-armv8a";
  }
  return result;
}

const InstructionSetFeatures* ArmInstructionSetFeatures::AddFeaturesFromSplitString(
    const bool smp, const std::vector<std::string>& features, std::string* error_msg) const {
  bool has_atomic_ldrd_strd = has_atomic_ldrd_strd_;
  bool has_div = has_div_;
  bool has_armv8a = has_armv8a_;
  for (auto i = features.begin(); i != features.end(); i++) {
    std::string feature = Trim(*i);
    if (feature == "div") {
      has_div = true;
    } else if (feature == "-div") {
      has_div = false;
    } else if (feature == "atomic_ldrd_strd") {
      has_atomic_ldrd_strd = true;
    } else if (feature == "-atomic_ldrd_strd") {
      has_atomic_ldrd_strd = false;
    } else if (feature == "armv8a") {
      has_armv8a = true;
    } else if (feature == "-armv8a") {
      has_armv8a = false;
    } else {
      *error_msg = StringPrintf("Unknown instruction set feature: '%s'", feature.c_str());
      return nullptr;
    }
  }
  return new ArmInstructionSetFeatures(smp, has_div, has_atomic_ldrd_strd, has_armv8a);
}

}  // namespace art
