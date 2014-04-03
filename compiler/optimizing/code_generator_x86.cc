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

#include "code_generator_x86.h"
#include "utils/assembler.h"
#include "utils/x86/assembler_x86.h"

#include "mirror/array.h"
#include "mirror/art_method.h"

#define __ reinterpret_cast<X86Assembler*>(GetAssembler())->

namespace art {
namespace x86 {

static const int kNumberOfPushedRegistersAtEntry = 1;
static const int kCurrentMethodStackOffset = 0;

InstructionCodeGeneratorX86::InstructionCodeGeneratorX86(HGraph* graph, CodeGeneratorX86* codegen)
      : HGraphVisitor(graph),
        assembler_(codegen->GetAssembler()),
        codegen_(codegen) {}

void CodeGeneratorX86::GenerateFrameEntry() {
  // Create a fake register to mimic Quick.
  static const int kFakeReturnRegister = 8;
  core_spill_mask_ |= (1 << kFakeReturnRegister);

  // Add the current ART method to the frame size and the return PC.
  SetFrameSize(RoundUp(GetFrameSize() + 2 * kWordSize, kStackAlignment));
  // The return PC has already been pushed on the stack.
  __ subl(ESP, Immediate(GetFrameSize() - kNumberOfPushedRegistersAtEntry * kWordSize));
  __ movl(Address(ESP, 0), EAX);
}

void CodeGeneratorX86::GenerateFrameExit() {
  __ addl(ESP, Immediate(GetFrameSize() - kNumberOfPushedRegistersAtEntry * kWordSize));
}

void CodeGeneratorX86::Bind(Label* label) {
  __ Bind(label);
}

void InstructionCodeGeneratorX86::LoadCurrentMethod(Register reg) {
  __ movl(reg, Address(ESP, kCurrentMethodStackOffset));
}

int32_t CodeGeneratorX86::GetStackSlot(HLocal* local) const {
  return (GetGraph()->GetMaximumNumberOfOutVRegs() + local->GetRegNumber()) * kWordSize;
}

void CodeGeneratorX86::Move(HInstruction* instruction, Location location, HInstruction* move_for) {
  if (instruction->AsIntConstant() != nullptr) {
    __ movl(location.reg<Register>(), Immediate(instruction->AsIntConstant()->GetValue()));
  } else if (instruction->AsLoadLocal() != nullptr) {
    __ movl(location.reg<Register>(),
            Address(ESP, GetStackSlot(instruction->AsLoadLocal()->GetLocal())));
  } else {
    // This can currently only happen when the instruction that requests the move
    // is the next to be compiled.
    DCHECK_EQ(instruction->GetNext(), move_for);
    __ movl(location.reg<Register>(),
            instruction->GetLocations()->Out().reg<Register>());
  }
}

void LocationsBuilderX86::VisitGoto(HGoto* got) {
  got->SetLocations(nullptr);
}

void InstructionCodeGeneratorX86::VisitGoto(HGoto* got) {
  HBasicBlock* successor = got->GetSuccessor();
  if (GetGraph()->GetExitBlock() == successor) {
    codegen_->GenerateFrameExit();
  } else if (!codegen_->GoesToNextBlock(got->GetBlock(), successor)) {
    __ jmp(codegen_->GetLabelOf(successor));
  }
}

void LocationsBuilderX86::VisitExit(HExit* exit) {
  exit->SetLocations(nullptr);
}

void InstructionCodeGeneratorX86::VisitExit(HExit* exit) {
  if (kIsDebugBuild) {
    __ Comment("Unreachable");
    __ int3();
  }
}

void LocationsBuilderX86::VisitIf(HIf* if_instr) {
  LocationSummary* locations = new (GetGraph()->GetArena()) LocationSummary(if_instr);
  locations->SetInAt(0, Location(EAX));
  if_instr->SetLocations(locations);
}

void InstructionCodeGeneratorX86::VisitIf(HIf* if_instr) {
  // TODO: Generate the input as a condition, instead of materializing in a register.
  __ cmpl(if_instr->GetLocations()->InAt(0).reg<Register>(), Immediate(0));
  __ j(kEqual, codegen_->GetLabelOf(if_instr->IfFalseSuccessor()));
  if (!codegen_->GoesToNextBlock(if_instr->GetBlock(), if_instr->IfTrueSuccessor())) {
    __ jmp(codegen_->GetLabelOf(if_instr->IfTrueSuccessor()));
  }
}

void LocationsBuilderX86::VisitLocal(HLocal* local) {
  local->SetLocations(nullptr);
}

void InstructionCodeGeneratorX86::VisitLocal(HLocal* local) {
  DCHECK_EQ(local->GetBlock(), GetGraph()->GetEntryBlock());
  codegen_->SetFrameSize(codegen_->GetFrameSize() + kWordSize);
}

void LocationsBuilderX86::VisitLoadLocal(HLoadLocal* local) {
  local->SetLocations(nullptr);
}

void InstructionCodeGeneratorX86::VisitLoadLocal(HLoadLocal* load) {
  // Nothing to do, this is driven by the code generator.
}

void LocationsBuilderX86::VisitStoreLocal(HStoreLocal* local) {
  LocationSummary* locations = new (GetGraph()->GetArena()) LocationSummary(local);
  locations->SetInAt(1, Location(EAX));
  local->SetLocations(locations);
}

void InstructionCodeGeneratorX86::VisitStoreLocal(HStoreLocal* store) {
  __ movl(Address(ESP, codegen_->GetStackSlot(store->GetLocal())),
          store->GetLocations()->InAt(1).reg<Register>());
}

void LocationsBuilderX86::VisitEqual(HEqual* equal) {
  LocationSummary* locations = new (GetGraph()->GetArena()) LocationSummary(equal);
  locations->SetInAt(0, Location(EAX));
  locations->SetInAt(1, Location(ECX));
  locations->SetOut(Location(EAX));
  equal->SetLocations(locations);
}

void InstructionCodeGeneratorX86::VisitEqual(HEqual* equal) {
  __ cmpl(equal->GetLocations()->InAt(0).reg<Register>(),
          equal->GetLocations()->InAt(1).reg<Register>());
  __ setb(kEqual, equal->GetLocations()->Out().reg<Register>());
}

void LocationsBuilderX86::VisitIntConstant(HIntConstant* constant) {
  constant->SetLocations(nullptr);
}

void InstructionCodeGeneratorX86::VisitIntConstant(HIntConstant* constant) {
  // Will be generated at use site.
}

void LocationsBuilderX86::VisitReturnVoid(HReturnVoid* ret) {
  ret->SetLocations(nullptr);
}

void InstructionCodeGeneratorX86::VisitReturnVoid(HReturnVoid* ret) {
  codegen_->GenerateFrameExit();
  __ ret();
}

void LocationsBuilderX86::VisitReturn(HReturn* ret) {
  LocationSummary* locations = new (GetGraph()->GetArena()) LocationSummary(ret);
  locations->SetInAt(0, Location(EAX));
  ret->SetLocations(locations);
}

void InstructionCodeGeneratorX86::VisitReturn(HReturn* ret) {
  DCHECK_EQ(ret->GetLocations()->InAt(0).reg<Register>(), EAX);
  codegen_->GenerateFrameExit();
  __ ret();
}

const Register kParameterCoreRegisters[] = { ECX, EDX, EBX };
const int kParameterCoreRegistersLength = arraysize(kParameterCoreRegisters);

class InvokeStaticCallingConvention : public CallingConvention<Register> {
 public:
  InvokeStaticCallingConvention()
      : CallingConvention(kParameterCoreRegisters, kParameterCoreRegistersLength) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(InvokeStaticCallingConvention);
};

void LocationsBuilderX86::VisitPushArgument(HPushArgument* argument) {
  LocationSummary* locations = new (GetGraph()->GetArena()) LocationSummary(argument);
  InvokeStaticCallingConvention calling_convention;
  if (argument->GetArgumentIndex() < calling_convention.GetNumberOfRegisters()) {
    Location location = Location(calling_convention.GetRegisterAt(argument->GetArgumentIndex()));
    locations->SetInAt(0, location);
    locations->SetOut(location);
  } else {
    locations->SetInAt(0, Location(EAX));
  }
  argument->SetLocations(locations);
}

void InstructionCodeGeneratorX86::VisitPushArgument(HPushArgument* argument) {
  uint8_t argument_index = argument->GetArgumentIndex();
  InvokeStaticCallingConvention calling_convention;
  size_t parameter_registers = calling_convention.GetNumberOfRegisters();
  if (argument_index >= parameter_registers) {
    uint8_t offset = calling_convention.GetStackOffsetOf(argument_index);
    __ movl(Address(ESP, offset),
            argument->GetLocations()->InAt(0).reg<Register>());

  } else {
    DCHECK_EQ(argument->GetLocations()->Out().reg<Register>(),
              argument->GetLocations()->InAt(0).reg<Register>());
  }
}

void LocationsBuilderX86::VisitInvokeStatic(HInvokeStatic* invoke) {
  LocationSummary* locations = new (GetGraph()->GetArena()) LocationSummary(invoke);
  locations->AddTemp(Location(EAX));
  invoke->SetLocations(locations);
}

void InstructionCodeGeneratorX86::VisitInvokeStatic(HInvokeStatic* invoke) {
  Register temp = invoke->GetLocations()->GetTemp(0).reg<Register>();
  size_t index_in_cache = mirror::Array::DataOffset(sizeof(mirror::Object*)).Int32Value() +
      invoke->GetIndexInDexCache() * kWordSize;

  // TODO: Implement all kinds of calls:
  // 1) boot -> boot
  // 2) app -> boot
  // 3) app -> app
  //
  // Currently we implement the app -> app logic, which looks up in the resolve cache.

  // temp = method;
  LoadCurrentMethod(temp);
  // temp = temp->dex_cache_resolved_methods_;
  __ movl(temp, Address(temp, mirror::ArtMethod::DexCacheResolvedMethodsOffset().Int32Value()));
  // temp = temp[index_in_cache]
  __ movl(temp, Address(temp, index_in_cache));
  // (temp + offset_of_quick_compiled_code)()
  __ call(Address(temp, mirror::ArtMethod::EntryPointFromQuickCompiledCodeOffset().Int32Value()));

  codegen_->RecordPcInfo(invoke->GetDexPc());
}

void LocationsBuilderX86::VisitAdd(HAdd* add) {
  LocationSummary* locations = new (GetGraph()->GetArena()) LocationSummary(add);
  switch (add->GetResultType()) {
    case Primitive::kPrimInt: {
      locations->SetInAt(0, Location(EAX));
      locations->SetInAt(1, Location(ECX));
      locations->SetOut(Location(EAX));
      break;
    }
    default:
      LOG(FATAL) << "Unimplemented";
  }
  add->SetLocations(locations);
}

void InstructionCodeGeneratorX86::VisitAdd(HAdd* add) {
  LocationSummary* locations = add->GetLocations();
  switch (add->GetResultType()) {
    case Primitive::kPrimInt:
      DCHECK_EQ(locations->InAt(0).reg<Register>(), locations->Out().reg<Register>());
      __ addl(locations->InAt(0).reg<Register>(), locations->InAt(1).reg<Register>());
      break;
    default:
      LOG(FATAL) << "Unimplemented";
  }
}

}  // namespace x86
}  // namespace art
