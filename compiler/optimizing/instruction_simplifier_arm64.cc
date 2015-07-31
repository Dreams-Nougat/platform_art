/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include "instruction_simplifier_arm64.h"

#include "mirror/array-inl.h"

namespace art {
namespace arm64 {

// The `side_effects_` field of `HInstruction` is const, so we clone the
// HInstruction to add the new side effect.
HInstruction* AddGCDependency(HInstruction* access) {
  HInstruction* new_access;
  ArenaAllocator* arena = access->GetBlock()->GetGraph()->GetArena();
  if (access->IsArrayGet()) {
    HArrayGet* get = access->AsArrayGet();
    new_access = new (arena) HArrayGet(get->GetArray(),
                                       get->GetIndex(),
                                       get->GetType(),
                                       SideEffects::DependsOnGC());
  } else {
    DCHECK(access->IsArraySet());
    HArraySet* set = access->AsArraySet();
    HArraySet* new_set = new (arena) HArraySet(set->GetArray(),
                                               set->GetIndex(),
                                               set->GetValue(),
                                               set->GetRawExpectedComponentType(),
                                               set->GetDexPc(),
                                               SideEffects::DependsOnGC());
    if (!set->GetValueCanBeNull()) {
      new_set->ClearValueCanBeNull();
    }
    if (!set->NeedsTypeCheck()) {
      new_set->ClearNeedsTypeCheck();
    }
    new_access = new_set;
  }
  access->GetBlock()->ReplaceAndRemoveInstructionWith(access, new_access);
  return new_access;
}

void InstructionSimplifierArm64::TryExtractArrayAccessAddress(HInstruction* access,
                                                              HInstruction* array,
                                                              HInstruction* index,
                                                              int access_size) {
  if (index->IsConstant() ||
      (index->IsBoundsCheck() && index->AsBoundsCheck()->GetIndex()->IsConstant())) {
    // When the index is a constant all the addressing can be fitted in the
    // memory access instruction, so do not split the access.
    return;
  }
  if (access->IsArraySet() && access->AsArraySet()->NeedsTypeCheck()) {
    // This requires a runtime call.
    return;
  }

  // Proceed to extract the base address computation.
  ArenaAllocator* arena = GetGraph()->GetArena();

  HIntConstant* offset =
      GetGraph()->GetIntConstant(mirror::Array::DataOffset(access_size).Uint32Value());
  HIntermediateAddress* address = new (arena) HIntermediateAddress(array, offset);
  access->GetBlock()->InsertInstructionBefore(address, access);
  access->ReplaceInput(address, 0);
  // Both instructions must depend on GC to prevent any instruction that can
  // trigger GC to be inserted between the two.
  HInstruction* new_access = AddGCDependency(access);
  DCHECK(address->GetSideEffects().Includes(SideEffects::DependsOnGC()));
  DCHECK(new_access->GetSideEffects().Includes(SideEffects::DependsOnGC()));
  // TODO: Code generation for HArrayGet and HArraySet will check whether the input address
  // is an HIntermediateAddress and generate appropriate code.
  // We would like to replace the `HArrayGet` and `HArraySet` with custom instructions (maybe
  // `HArm64Load` and `HArm64Store`). We defer these changes because these new instructions would
  // not bring any advantages yet.
  // Also see the comments in
  // `InstructionCodeGeneratorARM64::VisitArrayGet()` and
  // `InstructionCodeGeneratorARM64::VisitArraySet()`.
  RecordSimplification();
}

void InstructionSimplifierArm64::VisitArrayGet(HArrayGet* instruction) {
  TryExtractArrayAccessAddress(instruction,
                               instruction->GetArray(),
                               instruction->GetIndex(),
                               Primitive::ComponentSize(instruction->GetType()));
}

void InstructionSimplifierArm64::VisitArraySet(HArraySet* instruction) {
  TryExtractArrayAccessAddress(instruction,
                               instruction->GetArray(),
                               instruction->GetIndex(),
                               Primitive::ComponentSize(instruction->GetComponentType()));
}

}  // namespace arm64
}  // namespace art
