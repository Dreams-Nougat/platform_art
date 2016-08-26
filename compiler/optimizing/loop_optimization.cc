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

#include "loop_optimization.h"

#include "base/arena_containers.h"
#include "induction_var_range.h"
#include "ssa_liveness_analysis.h"
#include "nodes.h"

namespace art {

// Helper method to match phi-add/sub cycle, where the add/sub is only used by the phi.
// TODO: Generalize to cycles, as found by induction analysis?
static bool IsPhiAddSub(HPhi* phi, /*out*/ HInstruction** addsub_out) {
  HInputsRef inputs = phi->GetInputs();
  if (inputs.size() == 2 && (inputs[1]->IsAdd() || inputs[1]->IsSub())) {
    HInstruction* addsub = inputs[1];
    if (addsub->InputAt(0) == phi || addsub->InputAt(1) == phi) {
      if (addsub->GetUses().HasExactlyOneElement()) {
        *addsub_out = addsub;
        return true;
      }
    }
  }
  return false;
}

// Helper method to find a phi-add/sub cycle with no uses inside the loop.
static bool IsOnlyUsedAfterLoop(HLoopInformation* loop, HPhi* phi, HInstruction* addsub) {
  for (const HUseListNode<HInstruction*>& use : phi->GetUses()) {
    if (use.GetUser() != addsub) {
      HLoopInformation* other_loop = use.GetUser()->GetBlock()->GetLoopInformation();
      if (other_loop != nullptr && other_loop->IsIn(*loop)) {
        return false;
      }
    }
  }
  return true;
}

// Helper method to find the header of an empty loop.
// Find: phi: Phi(init, addsub)
//       s:   SuspendCheck
//       c:   Condition(phi, bound)
//       i:   If(c)
// TODO: Find a less pattern matching approach?
static bool IsEmptyHeader(HBasicBlock* block, /*out*/ HInstruction** addsub) {
  HInstruction* phi = block->GetFirstPhi();
  if (phi != nullptr && phi->GetNext() == nullptr && IsPhiAddSub(phi->AsPhi(), addsub)) {
    HInstruction* s = block->GetFirstInstruction();
    if (s != nullptr && s->IsSuspendCheck()) {
      HInstruction* c = s->GetNext();
      if (c != nullptr && c->IsCondition() && c->GetUses().HasExactlyOneElement()) {
        HInstruction* i = c->GetNext();
        if (i != nullptr && i->IsIf() && i->InputAt(0) == c) {
          // Check that phi is only used inside loop as expected.
          for (const HUseListNode<HInstruction*>& use : phi->GetUses()) {
            if (use.GetUser() != *addsub && use.GetUser() != c) {
              return false;
            }
          }
          return true;
        }
      }
    }
  }
  return false;
}

// Helper method to find the body of an empty loop.
static bool IsEmptyBody(HBasicBlock* block, HInstruction* addsub) {
  HInstruction* phi = block->GetFirstPhi();
  HInstruction* i = block->GetFirstInstruction();
  return phi == nullptr && i == addsub && i->GetNext() != nullptr && i->GetNext()->IsGoto();
}

// Helper method to prepare pre-header for removal.
static HBasicBlock* CanRemovePreHeader(HBasicBlock* preheader, HBasicBlock* entry_block) {
  if (preheader->GetPredecessors().size() == 1) {
    HBasicBlock* entry = preheader->GetSinglePredecessor();
    HInstruction* anchor = entry->GetLastInstruction();
    // If the pre-header has a single predecessor we can remove it too if
    // either the pre-header just contains a goto, or if the predecessor
    // is not the entry block so we can push instructions backward
    // (moving computation into the entry block is too dangerous!).
    if (preheader->GetFirstInstruction() == nullptr ||
        preheader->GetFirstInstruction()->IsGoto() ||
        (entry != entry_block && anchor->IsGoto())) {
      // Push non-goto statements backward to empty the pre-header.
      for (HInstructionIterator it(preheader->GetInstructions()); !it.Done(); it.Advance()) {
        HInstruction* instruction = it.Current();
        if (!instruction->IsGoto()) {
          if (!instruction->CanBeMoved()) {
            return nullptr;  // pushing failed to move all
          }
          it.Current()->MoveBefore(anchor);
        }
      }
      return entry;
    }
  }
  return nullptr;
}

// Helper method to remove an instruction. A bit more elaborate than the
// usual instruction removal, since there may be a cycle in the use structure.
static void RemoveFromCycle(HInstruction* instruction) {
  instruction->RemoveAsUserOfAllInputs();
  instruction->RemoveEnvironmentUsers();
  instruction->GetBlock()->RemoveInstructionOrPhi(instruction, /*ensure_safety=*/ false);
}

//
// Class methods.
//

HLoopOptimization::HLoopOptimization(HGraph* graph,
                                     HInductionVarAnalysis* induction_analysis)
    : HOptimization(graph, kLoopOptimizationPassName),
      induction_range_(induction_analysis),
      top_loop_(nullptr),
      last_loop_(nullptr) {
}

void HLoopOptimization::Run() {
  // Well-behaved loops only.
  // TODO: make this less of a sledgehammer.
  if (graph_-> HasTryCatch() || graph_->HasIrreducibleLoops()) {
    return;
  }
  // Build the linear order and the loop hierarchy.
  graph_->Linearize();
  for (HLinearOrderIterator it_graph(*graph_); !it_graph.Done(); it_graph.Advance()) {
    HBasicBlock* block = it_graph.Current();
    if (block->IsLoopHeader()) {
      AddLoop(block->GetLoopInformation());
    }
  }
  if (top_loop_ == nullptr) {
    return;  // no loops
  }
  // Traverse the loop hierarchy inner-to-outer and optimize.
  TraverseLoopsInnerToOuter(top_loop_);
}

void HLoopOptimization::AddLoop(HLoopInformation* loop) {
  DCHECK(loop != nullptr);
  LoopNode* node = new (graph_->GetArena()) LoopNode(loop);
  if (last_loop_ == nullptr) {
    // First loop.
    DCHECK(top_loop_ == nullptr);
    last_loop_ = top_loop_ = node;
  } else if (loop->IsIn(*last_loop_->loop)) {
    // Inner loop.
    node->outer = last_loop_;
    DCHECK(last_loop_->inner == nullptr);
    last_loop_ = last_loop_->inner = node;
  } else {
    // Subsequent loop.
    while (last_loop_->outer != nullptr && !loop->IsIn(*last_loop_->outer->loop)) {
      last_loop_ = last_loop_->outer;
    }
    node->outer = last_loop_->outer;
    node->prev = last_loop_;
    DCHECK(last_loop_->next == nullptr);
    last_loop_ = last_loop_->next = node;
  }
}

void HLoopOptimization::TraverseLoopsInnerToOuter(LoopNode* node) {
  for ( ; node != nullptr; node = node->next) {
    if (node->inner != nullptr) {
      TraverseLoopsInnerToOuter(node->inner);
    }
    // Visit loop after its inner loops have been visited.
    SimplifyInduction(node);
    RemoveEmptyLoop(node);
  }
}

void HLoopOptimization::SimplifyInduction(LoopNode* node) {
  HLoopInformation* loop = node->loop;
  HBasicBlock* header = loop->GetHeader();
  HBasicBlock* preheader = loop->GetPreHeader();
  // Scan the phis in the header to find opportunities to optimize induction.
  for (HInstructionIterator it(header->GetPhis()); !it.Done(); it.Advance()) {
    HPhi* phi = it.Current()->AsPhi();
    HInstruction* addsub = nullptr;
    // Find phi-add/sub cycle.
    if (IsPhiAddSub(phi, &addsub)) {
      // Simple case, the induction is only used by itself. Although redundant,
      // later phases do not easily detect this property. Thus, eliminate here.
      // Example: for (int i = 0; x != null; i++) { .... no i .... }
      if (phi->GetUses().HasExactlyOneElement()) {
        // Remove the cycle, including all uses. Even env uses can be removed,
        // since these computations have no effect at all.
        RemoveFromCycle(phi);  // removes env uses too
        RemoveFromCycle(addsub);
        continue;
      }
      // Closed form case. Only the last value of the induction is needed. Remove all
      // overhead from the loop, and replace subsequent uses with the last value.
      // Example: for (int i = 0; i < 10; i++, k++) { .... no k .... } return k;
      if (IsOnlyUsedAfterLoop(loop, phi, addsub) && induction_range_.CanGenerateLastValue(phi)) {
        HInstruction* last = induction_range_.GenerateLastValue(phi, graph_, preheader);
        // Remove the cycle, replacing all uses. Even env uses can consume the final
        // value, since any first real use is outside the loop (although this may imply
        // that deopting may look "ahead" a bit on the phi value).
        ReplaceAllUses(phi, last, addsub);
        RemoveFromCycle(phi);  // removes env uses too
        RemoveFromCycle(addsub);
      }
    }
  }
}

void HLoopOptimization::RemoveEmptyLoop(LoopNode* node) {
  HLoopInformation* loop = node->loop;
  HBasicBlock* header = loop->GetHeader();
  HBasicBlock* preheader = loop->GetPreHeader();
  // Single header/body/exit logic.
  if (header->GetSuccessors().size() != 2) {
    return;
  }
  HBasicBlock* body = nullptr;
  for (HBlocksInLoopIterator it(*loop); !it.Done(); it.Advance()) {
    if (it.Current() != header) {
      if (body != nullptr) {
        return;
      }
      body = it.Current();
    }
  }
  HBasicBlock* exit = (header->GetSuccessors()[0] == body)
      ? header->GetSuccessors()[1]
      : header->GetSuccessors()[0];
  if (exit->GetPredecessors().size() != 1) {
    return;
  }
  // Detect an empty loop (no side effects other than plain iteration).
  HInstruction* addsub = nullptr;
  if (IsEmptyHeader(header, &addsub) && IsEmptyBody(body, addsub)) {
    HBasicBlock* entry = CanRemovePreHeader(preheader, graph_->GetEntryBlock());
    body->DisconnectAndDelete();
    exit->RemovePredecessor(header);
    header->RemoveSuccessor(exit);
    header->ClearDominanceInformation();
    header->SetDominator(preheader);  // needed by next disconnect.
    header->DisconnectAndDelete();
    // If allowed, remove preheader too, which may expose next outer empty loop
    // Otherwise, link preheader directly to exit to restore the flow graph.
    if (entry != nullptr) {
      entry->ReplaceSuccessor(preheader, exit);
      entry->AddDominatedBlock(exit);
      exit->SetDominator(entry);
      preheader->DisconnectAndDelete();
    } else {
      preheader->AddSuccessor(exit);
      preheader->AddInstruction(new (graph_->GetArena()) HGoto());
      preheader->AddDominatedBlock(exit);
      exit->SetDominator(preheader);
    }
    node->loop = nullptr;  // flag deletion
  }
}

void HLoopOptimization::ReplaceAllUses(HInstruction* instruction,
                                       HInstruction* replacement,
                                       HInstruction* exclusion) {
  const HUseList<HInstruction*>& uses = instruction->GetUses();
  for (auto it = uses.begin(), end = uses.end(); it != end;) {
    HInstruction* user = it->GetUser();
    size_t index = it->GetIndex();
    ++it;  // increment before replacing
    if (user != exclusion) {
      user->ReplaceInput(replacement, index);
      induction_range_.Replace(user, instruction, replacement);  // update induction
    }
  }
  const HUseList<HEnvironment*>& env_uses = instruction->GetEnvUses();
  for (auto it = env_uses.begin(), end = env_uses.end(); it != end;) {
    HEnvironment* user = it->GetUser();
    size_t index = it->GetIndex();
    ++it;  // increment before replacing
    if (user->GetHolder() != exclusion) {
      user->RemoveAsUserOfInput(index);
      user->SetRawEnvAt(index, replacement);
      replacement->AddEnvUseAt(user, index);
    }
  }
}

}  // namespace art
