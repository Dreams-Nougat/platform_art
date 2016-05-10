/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef ART_COMPILER_LINKER_MIPS_RELATIVE_PATCHER_MIPS_H_
#define ART_COMPILER_LINKER_MIPS_RELATIVE_PATCHER_MIPS_H_

#include "linker/mips/relative_patcher_mips_base.h"
#include "arch/mips/instruction_set_features_mips.h"

namespace art {
namespace linker {

class MipsRelativePatcher FINAL : public MipsBaseRelativePatcher {
 public:
  explicit MipsRelativePatcher(const MipsInstructionSetFeatures* features)
      : is_r6(features->IsR6()) {}

  void PatchPcRelativeReference(std::vector<uint8_t>* code,
                                const LinkerPatch& patch,
                                uint32_t patch_offset,
                                uint32_t target_offset) OVERRIDE;

 private:
  bool is_r6;
};

}  // namespace linker
}  // namespace art

#endif  // ART_COMPILER_LINKER_MIPS_RELATIVE_PATCHER_MIPS_H_
