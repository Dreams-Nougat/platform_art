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

#include "nodes.h"
#include "null_propagation.h"

namespace art {

void NullVisitor::VisitInvoke(HInvoke* instr) {
  if (instr->GetType() != Primitive::kPrimNot) {
    return;
  }

  if (!(instr->IsInvokeStaticOrDirect() && instr->AsInvokeStaticOrDirect()->IsStatic())) {
    SetProperty(instr->InputAt(0), NullInfo(false));
  }
  SetProperty(instr, NullInfo(true));
}

void NullVisitor::VisitPhi(HPhi* phi) {
  if (phi->GetType() != Primitive::kPrimNot) {
    return;
  }

  phi->SetCanBeNull(GetProperty(phi).can_be_null);
}

}  // namespace art
