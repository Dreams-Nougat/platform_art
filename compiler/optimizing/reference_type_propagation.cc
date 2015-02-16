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

#include "reference_type_propagation.h"

#include "class_linker.h"
#include "mirror/class-inl.h"
#include "mirror/dex_cache.h"
#include "scoped_thread_state_change.h"

namespace art {

// TODO: Only do the analysis on reference types. We currently have to handle
// the `null` constant, that is represented as a `HIntConstant` and therefore
// has the Primitive::kPrimInt type.

// TODO: handle:
//  public Main ifNullTest(int count, Main a) {
//    Main m = new Main();
//    if (a == null) {
//      a = m;
//    }
//    return a.g();
//  }
//  public Main ifNotNullTest(Main a) {
//    if (a != null) {
//      return a.g();
//    }
//    return new Main();
//  }

void ReferenceTypePropagation::Run() {
  for (HReversePostOrderIterator it(*graph_); !it.Done(); it.Advance()) {
    TestForAndProcessInstanceOfSuccesor(it.Current());
  }
  // To properly propagate type info we need to visit in the dominator-based order.
  // Reverse post order guarantees a node's dominators are visited first.
  // We take advantage of this order in `VisitBasicBlock`.
  for (HReversePostOrderIterator it(*graph_); !it.Done(); it.Advance()) {
    VisitBasicBlock(it.Current());
  }
  ProcessWorklist();
}


// Re-computes and updates the nullability of the instruction. Returns whether or
// not the nullability was changed.
bool ReferenceTypePropagation::UpdateNullability(HPhi* phi) {
  bool existing_can_be_null = phi->CanBeNull();
  bool new_can_be_null = false;
  for (size_t i = 0; i < phi->InputCount(); i++) {
    new_can_be_null |= phi->InputAt(i)->CanBeNull();
  }
  phi->SetCanBeNull(new_can_be_null);

  return existing_can_be_null != new_can_be_null;
}

void ReferenceTypePropagation::MergeTypes(ReferenceTypeInfo& new_rti,
                                          const ReferenceTypeInfo& input_rti)
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  if (new_rti.IsTop()) {
    // nothing todo, we are already at top.
  } else if (input_rti.IsTop()) {
    new_rti.SetTop();
  } else if (new_rti.GetTypeHandle().Get() == input_rti.GetTypeHandle().Get()) {
    // nothing to do if we have the same type
  } else if (input_rti.IsSupertypeOf(new_rti)) {
    new_rti.SetTypeHandle(input_rti.GetTypeHandle(), false);
  } else if (new_rti.IsSupertypeOf(input_rti)) {
    new_rti.SetInexact();
  } else {
    // TODO: Find common parent.
    new_rti.SetTop();
  }
}

bool ReferenceTypePropagation::UpdateReferenceTypeInfo(HPhi* phi)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ReferenceTypeInfo existing_rti = phi->GetReferenceTypeInfo();
  ReferenceTypeInfo new_rti = phi->InputAt(0)->GetReferenceTypeInfo();

  if (new_rti.IsTop()) {
    // Early return if we are Top.
    return !new_rti.IsEqual(existing_rti);;
  }

  for (size_t i = 1; i < phi->InputCount(); i++) {
    ReferenceTypeInfo input_rti = phi->InputAt(i)->GetReferenceTypeInfo();
    MergeTypes(new_rti, input_rti);
    if (new_rti.IsTop()) {
      break;
    }
  }

  phi->SetReferenceTypeInfo(new_rti);
  return !new_rti.IsEqual(existing_rti);
}

void ReferenceTypePropagation::VisitNewInstance(HNewInstance* instr) {
  ScopedObjectAccess soa(Thread::Current());
  mirror::DexCache* dex_cache = dex_compilation_unit_.GetClassLinker()->FindDexCache(dex_file_);
  // Get type from dex cache assuming it was populated by the verifier.
  mirror::Class* resolved_class = dex_cache->GetResolvedType(instr->GetTypeIndex());
  if (resolved_class != nullptr) {
    MutableHandle<mirror::Class> handle = handles_->NewHandle(resolved_class);
    instr->SetReferenceTypeInfo(ReferenceTypeInfo(handle, true));
  }
}

void ReferenceTypePropagation::VisitLoadClass(HLoadClass* instr) {
  ScopedObjectAccess soa(Thread::Current());
  mirror::DexCache* dex_cache = dex_compilation_unit_.GetClassLinker()->FindDexCache(dex_file_);
  // Get type from dex cache assuming it was populated by the verifier.
  mirror::Class* resolved_class = dex_cache->GetResolvedType(instr->GetTypeIndex());
  if (resolved_class != nullptr) {
    Handle<mirror::Class> handle = handles_->NewHandle(resolved_class);
    instr->SetLoadedClassRTI(ReferenceTypeInfo(handle, true));
  }
  Handle<mirror::Class> class_handle = handles_->NewHandle(mirror::Class::GetJavaLangClass());
  instr->SetReferenceTypeInfo(ReferenceTypeInfo(class_handle, true));
}

void ReferenceTypePropagation::VisitBasicBlock(HBasicBlock* block) {
  if (block->IsLoopHeader()) {
    for (HInstructionIterator it(block->GetPhis()); !it.Done(); it.Advance()) {
      // Set the initial type for the phi. Use the non back edge input for reaching
      // a fixed point faster.
      HPhi* phi = it.Current()->AsPhi();
      AddToWorklist(phi);
      phi->SetCanBeNull(phi->InputAt(0)->CanBeNull());
      phi->SetReferenceTypeInfo(phi->InputAt(0)->GetReferenceTypeInfo());
    }
  } else {
    ScopedObjectAccess soa(Thread::Current());
    for (HInstructionIterator it(block->GetPhis()); !it.Done(); it.Advance()) {
      // Eagerly compute the type of the phi, for quicker convergence. Note
      // that we don't need to add users to the worklist because we are
      // doing a reverse post-order visit, therefore either the phi users are
      // non-loop phi and will be visited later in the visit, or are loop-phis,
      // and they are already in the work list.
      HPhi* phi = it.Current()->AsPhi();
      UpdateNullability(phi);
      UpdateReferenceTypeInfo(phi);
    }
  }
}

void ReferenceTypePropagation::TestForAndProcessInstanceOfSuccesor(HBasicBlock* block) {
  // TODO: handle other instructions that give type info
  // (NewArray/Call/Field accesses/array accesses)
  for (HInstructionIterator it(block->GetInstructions()); !it.Done(); it.Advance()) {
    HInstruction* instr = it.Current();
    if (instr->IsNewInstance()) {
      VisitNewInstance(instr->AsNewInstance());
    } else if (instr->IsLoadClass()) {
      VisitLoadClass(instr->AsLoadClass());
    }
  }
  GrowableArray<HBasicBlock*> predecessors = block->GetPredecessors();
  if (predecessors.IsEmpty() || predecessors.Get(0)->IsEntryBlock()) {
    return;
  }
  HBasicBlock* previousBlock = predecessors.Get(0);
  HInstruction* previousIf = previousBlock->GetLastInstruction();
  if (!previousIf->IsIf()
      || (previousIf->AsIf()->IfFalseSuccessor() != block)) {  // InstanceOf returns 0 when True.
    return;
  }
  HInstruction* ifInput = previousIf->InputAt(0);
  if (!ifInput->IsEqual()) {
    return;
  }
  HInstruction* instanceOf = ifInput->InputAt(0);
  HInstruction* zero = ifInput->InputAt(1);
  if (!instanceOf->IsInstanceOf() || !zero->IsConstant()
      || (zero->AsIntConstant()->GetValue() != 0)) {
    return;
  }

  HInstruction* obj = instanceOf->InputAt(0);
  HLoadClass* load_class = instanceOf->InputAt(1)->AsLoadClass();

  ReferenceTypeInfo class_rti = load_class->GetLoadedClassRTI();
  HBoundType* bound_type = new (graph_->GetArena()) HBoundType(obj, class_rti);

  bound_type->SetReferenceTypeInfo(class_rti);
  // TODO: merge
  LOG(INFO) << "CALIN: bound_type " << bound_type->GetReferenceTypeInfo();
  // Doesn't really matter where we insert it.
  previousBlock->InsertInstructionBefore(bound_type, ifInput);

  for (HUseIterator<HInstruction*> it(obj->GetUses()); !it.Done(); it.Advance()) {
    HInstruction* user = it.Current()->GetUser();
    if (block->Dominates(user->GetBlock())) {
      user->ReplaceInput(bound_type, it.Current()->GetIndex());
    }
  }
}

void ReferenceTypePropagation::ProcessWorklist() {
  ScopedObjectAccess soa(Thread::Current());
  while (!worklist_.IsEmpty()) {
    HPhi* instruction = worklist_.Pop();
    if (UpdateNullability(instruction) || UpdateReferenceTypeInfo(instruction)) {
      AddDependentInstructionsToWorklist(instruction);
    }
  }
}

void ReferenceTypePropagation::AddToWorklist(HPhi* instruction) {
  worklist_.Add(instruction);
}

void ReferenceTypePropagation::AddDependentInstructionsToWorklist(HPhi* instruction) {
  for (HUseIterator<HInstruction*> it(instruction->GetUses()); !it.Done(); it.Advance()) {
    HPhi* phi = it.Current()->GetUser()->AsPhi();
    if (phi != nullptr) {
      AddToWorklist(phi);
    }
  }
}

}  // namespace art
