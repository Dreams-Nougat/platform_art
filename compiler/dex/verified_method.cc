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

#include "verified_method.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "art_method-inl.h"
#include "base/enums.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "dex_file.h"
#include "dex_instruction-inl.h"
#include "dex_instruction_utils.h"
#include "mirror/class-inl.h"
#include "mirror/dex_cache-inl.h"
#include "mirror/object-inl.h"
#include "utils.h"
#include "verifier/method_verifier-inl.h"
#include "verifier/reg_type-inl.h"
#include "verifier/register_line-inl.h"

namespace art {

VerifiedMethod::VerifiedMethod(uint32_t encountered_error_types, bool has_runtime_throw)
    : encountered_error_types_(encountered_error_types),
      has_runtime_throw_(has_runtime_throw) {
}

const VerifiedMethod* VerifiedMethod::Create(verifier::MethodVerifier* method_verifier) {
  DCHECK(Runtime::Current()->IsAotCompiler());
  std::unique_ptr<VerifiedMethod> verified_method(
      new VerifiedMethod(method_verifier->GetEncounteredFailureTypes(),
                         method_verifier->HasInstructionThatWillThrow()));

  if (method_verifier->HasCheckCasts()) {
    verified_method->GenerateSafeCastSet(method_verifier);
  }

  return verified_method.release();
}

bool VerifiedMethod::IsSafeCast(uint32_t pc) const {
  return std::binary_search(safe_cast_set_.begin(), safe_cast_set_.end(), pc);
}

bool VerifiedMethod::GenerateDequickenMap(verifier::MethodVerifier* method_verifier) {
  if (method_verifier->HasFailures()) {
    return false;
  }
  const DexFile::CodeItem* code_item = method_verifier->CodeItem();
  const uint16_t* insns = code_item->insns_;
  const Instruction* inst = Instruction::At(insns);
  const Instruction* end = Instruction::At(insns + code_item->insns_size_in_code_units_);
  for (; inst < end; inst = inst->Next()) {
    const bool is_virtual_quick = inst->Opcode() == Instruction::INVOKE_VIRTUAL_QUICK;
    const bool is_range_quick = inst->Opcode() == Instruction::INVOKE_VIRTUAL_RANGE_QUICK;
    if (is_virtual_quick || is_range_quick) {
      uint32_t dex_pc = inst->GetDexPc(insns);
      verifier::RegisterLine* line = method_verifier->GetRegLine(dex_pc);
      ArtMethod* method =
          method_verifier->GetQuickInvokedMethod(inst, line, is_range_quick, true);
      if (method == nullptr) {
        // It can be null if the line wasn't verified since it was unreachable.
        return false;
      }
      // The verifier must know what the type of the object was or else we would have gotten a
      // failure. Put the dex method index in the dequicken map since we need this to get number of
      // arguments in the compiler.
      dequicken_map_.Put(dex_pc, DexFileReference(method->GetDexFile(),
                                                  method->GetDexMethodIndex()));
    } else if (IsInstructionIGetQuickOrIPutQuick(inst->Opcode())) {
      uint32_t dex_pc = inst->GetDexPc(insns);
      verifier::RegisterLine* line = method_verifier->GetRegLine(dex_pc);
      ArtField* field = method_verifier->GetQuickFieldAccess(inst, line);
      if (field == nullptr) {
        // It can be null if the line wasn't verified since it was unreachable.
        return false;
      }
      // The verifier must know what the type of the field was or else we would have gotten a
      // failure. Put the dex field index in the dequicken map since we need this for lowering
      // in the compiler.
      // TODO: Putting a field index in a method reference is gross.
      dequicken_map_.Put(dex_pc, DexFileReference(field->GetDexFile(), field->GetDexFieldIndex()));
    }
  }
  return true;
}

void VerifiedMethod::GenerateSafeCastSet(verifier::MethodVerifier* method_verifier) {
  /*
   * Walks over the method code and adds any cast instructions in which
   * the type cast is implicit to a set, which is used in the code generation
   * to elide these casts.
   */
  if (method_verifier->HasFailures()) {
    return;
  }
  const DexFile::CodeItem* code_item = method_verifier->CodeItem();
  const Instruction* inst = Instruction::At(code_item->insns_);
  const Instruction* end = Instruction::At(code_item->insns_ +
                                           code_item->insns_size_in_code_units_);

  for (; inst < end; inst = inst->Next()) {
    Instruction::Code code = inst->Opcode();
    if ((code == Instruction::CHECK_CAST) || (code == Instruction::APUT_OBJECT)) {
      uint32_t dex_pc = inst->GetDexPc(code_item->insns_);
      if (!method_verifier->GetInstructionFlags(dex_pc).IsVisited()) {
        // Do not attempt to quicken this instruction, it's unreachable anyway.
        continue;
      }
      const verifier::RegisterLine* line = method_verifier->GetRegLine(dex_pc);
      bool is_safe_cast = false;
      if (code == Instruction::CHECK_CAST) {
        const verifier::RegType& reg_type(line->GetRegisterType(method_verifier,
                                                                inst->VRegA_21c()));
        const verifier::RegType& cast_type =
            method_verifier->ResolveCheckedClass(dex::TypeIndex(inst->VRegB_21c()));
        is_safe_cast = cast_type.IsStrictlyAssignableFrom(reg_type, method_verifier);
      } else {
        const verifier::RegType& array_type(line->GetRegisterType(method_verifier,
                                                                  inst->VRegB_23x()));
        // We only know its safe to assign to an array if the array type is precise. For example,
        // an Object[] can have any type of object stored in it, but it may also be assigned a
        // String[] in which case the stores need to be of Strings.
        if (array_type.IsPreciseReference()) {
          const verifier::RegType& value_type(line->GetRegisterType(method_verifier,
                                                                    inst->VRegA_23x()));
          const verifier::RegType& component_type = method_verifier->GetRegTypeCache()
              ->GetComponentType(array_type, method_verifier->GetClassLoader());
          is_safe_cast = component_type.IsStrictlyAssignableFrom(value_type, method_verifier);
        }
      }
      if (is_safe_cast) {
        // Verify ordering for push_back() to the sorted vector.
        DCHECK(safe_cast_set_.empty() || safe_cast_set_.back() < dex_pc);
        safe_cast_set_.push_back(dex_pc);
      }
    }
  }
}

}  // namespace art
