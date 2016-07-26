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

#include "calling_convention_x86_64.h"

#include "base/bit_utils.h"
#include "base/logging.h"
#include "handle_scope-inl.h"
#include "utils/x86_64/managed_register_x86_64.h"

namespace art {
namespace x86_64 {

constexpr size_t kFramePointerSize = static_cast<size_t>(PointerSize::k64);

static_assert(kX86_64PointerSize == PointerSize::k64, "Unexpected x86_64 pointer size");

static constexpr ManagedRegister kCalleeSaveRegisters[] = {
    // Core registers.
    X86_64ManagedRegister::FromCpuRegister(RBX),
    X86_64ManagedRegister::FromCpuRegister(RBP),
    X86_64ManagedRegister::FromCpuRegister(R12),
    X86_64ManagedRegister::FromCpuRegister(R13),
    X86_64ManagedRegister::FromCpuRegister(R14),
    X86_64ManagedRegister::FromCpuRegister(R15),
    // Hard float registers.
    X86_64ManagedRegister::FromXmmRegister(XMM12),
    X86_64ManagedRegister::FromXmmRegister(XMM13),
    X86_64ManagedRegister::FromXmmRegister(XMM14),
    X86_64ManagedRegister::FromXmmRegister(XMM15),
};

static constexpr uint32_t CalculateCoreCalleeSpillMask() {
  // The spilled PC gets a special marker.
  uint32_t result = 1 << kNumberOfCpuRegisters;
  for (auto&& r : kCalleeSaveRegisters) {
    if (r.AsX86_64().IsCpuRegister()) {
      result |= (1 << r.AsX86_64().AsCpuRegister().AsRegister());
    }
  }
  return result;
}

static constexpr uint32_t CalculateFpCalleeSpillMask() {
  uint32_t result = 0;
  for (auto&& r : kCalleeSaveRegisters) {
    if (r.AsX86_64().IsXmmRegister()) {
      result |= (1 << r.AsX86_64().AsXmmRegister().AsFloatRegister());
    }
  }
  return result;
}

static constexpr uint32_t kCoreCalleeSpillMask = CalculateCoreCalleeSpillMask();
static constexpr uint32_t kFpCalleeSpillMask = CalculateFpCalleeSpillMask();

// Calling convention

ManagedRegister X86_64ManagedRuntimeCallingConvention::InterproceduralScratchRegister() {
  return X86_64ManagedRegister::FromCpuRegister(RAX);
}

ManagedRegister X86_64JniCallingConvention::InterproceduralScratchRegister() {
  return X86_64ManagedRegister::FromCpuRegister(RAX);
}

ManagedRegister X86_64JniCallingConvention::ReturnScratchRegister() const {
  return ManagedRegister::NoRegister();  // No free regs, so assembler uses push/pop
}

static ManagedRegister ReturnRegisterForShorty(const char* shorty, bool jni ATTRIBUTE_UNUSED) {
  if (shorty[0] == 'F' || shorty[0] == 'D') {
    return X86_64ManagedRegister::FromXmmRegister(XMM0);
  } else if (shorty[0] == 'J') {
    return X86_64ManagedRegister::FromCpuRegister(RAX);
  } else if (shorty[0] == 'V') {
    return ManagedRegister::NoRegister();
  } else {
    return X86_64ManagedRegister::FromCpuRegister(RAX);
  }
}

ManagedRegister X86_64ManagedRuntimeCallingConvention::ReturnRegister() {
  return ReturnRegisterForShorty(GetShorty(), false);
}

ManagedRegister X86_64JniCallingConvention::ReturnRegister() {
  return ReturnRegisterForShorty(GetShorty(), true);
}

ManagedRegister X86_64JniCallingConvention::IntReturnRegister() {
  return X86_64ManagedRegister::FromCpuRegister(RAX);
}

// Managed runtime calling convention

ManagedRegister X86_64ManagedRuntimeCallingConvention::MethodRegister() {
  return X86_64ManagedRegister::FromCpuRegister(RDI);
}

bool X86_64ManagedRuntimeCallingConvention::IsCurrentParamInRegister() {
  return !IsCurrentParamOnStack();
}

bool X86_64ManagedRuntimeCallingConvention::IsCurrentParamOnStack() {
  // We assume all parameters are on stack, args coming via registers are spilled as entry_spills
  return true;
}

ManagedRegister X86_64ManagedRuntimeCallingConvention::CurrentParamRegister() {
  ManagedRegister res = ManagedRegister::NoRegister();
  if (!IsCurrentParamAFloatOrDouble()) {
    switch (itr_args_ - itr_float_and_doubles_) {
    case 0: res = X86_64ManagedRegister::FromCpuRegister(RSI); break;
    case 1: res = X86_64ManagedRegister::FromCpuRegister(RDX); break;
    case 2: res = X86_64ManagedRegister::FromCpuRegister(RCX); break;
    case 3: res = X86_64ManagedRegister::FromCpuRegister(R8); break;
    case 4: res = X86_64ManagedRegister::FromCpuRegister(R9); break;
    }
  } else if (itr_float_and_doubles_ < 8) {
    // First eight float parameters are passed via XMM0..XMM7
    res = X86_64ManagedRegister::FromXmmRegister(
                                 static_cast<FloatRegister>(XMM0 + itr_float_and_doubles_));
  }
  return res;
}

FrameOffset X86_64ManagedRuntimeCallingConvention::CurrentParamStackOffset() {
  return FrameOffset(displacement_.Int32Value() +  // displacement
                     static_cast<size_t>(kX86_64PointerSize) +  // Method ref
                     itr_slots_ * sizeof(uint32_t));  // offset into in args
}

const ManagedRegisterEntrySpills& X86_64ManagedRuntimeCallingConvention::EntrySpills() {
  // We spill the argument registers on X86 to free them up for scratch use, we then assume
  // all arguments are on the stack.
  if (entry_spills_.size() == 0) {
    ResetIterator(FrameOffset(0));
    while (HasNext()) {
      ManagedRegister in_reg = CurrentParamRegister();
      if (!in_reg.IsNoRegister()) {
        int32_t size = IsParamALongOrDouble(itr_args_)? 8 : 4;
        int32_t spill_offset = CurrentParamStackOffset().Uint32Value();
        ManagedRegisterSpill spill(in_reg, size, spill_offset);
        entry_spills_.push_back(spill);
      }
      Next();
    }
  }
  return entry_spills_;
}

// JNI calling convention

X86_64JniCallingConvention::X86_64JniCallingConvention(bool is_static, bool is_synchronized,
                                                       const char* shorty)
    : JniCallingConvention(is_static, is_synchronized, shorty, kX86_64PointerSize) {
}

uint32_t X86_64JniCallingConvention::CoreSpillMask() const {
  return kCoreCalleeSpillMask;
}

uint32_t X86_64JniCallingConvention::FpSpillMask() const {
  return kFpCalleeSpillMask;
}

size_t X86_64JniCallingConvention::FrameSize() {
  // Method*, return address and callee save area size, local reference segment state
  size_t frame_data_size = static_cast<size_t>(kX86_64PointerSize) +
      (2 + CalleeSaveRegisters().size()) * kFramePointerSize;
  // References plus link_ (pointer) and number_of_references_ (uint32_t) for HandleScope header
  size_t handle_scope_size = HandleScope::SizeOf(kX86_64PointerSize, ReferenceCount());
  // Plus return value spill area size
  return RoundUp(frame_data_size + handle_scope_size + SizeOfReturnValue(), kStackAlignment);
}

size_t X86_64JniCallingConvention::OutArgSize() {
  return RoundUp(NumberOfOutgoingStackArgs() * kFramePointerSize, kStackAlignment);
}

ArrayRef<const ManagedRegister> X86_64JniCallingConvention::CalleeSaveRegisters() const {
  return ArrayRef<const ManagedRegister>(kCalleeSaveRegisters);
}

bool X86_64JniCallingConvention::IsCurrentParamInRegister() {
  return !IsCurrentParamOnStack();
}

bool X86_64JniCallingConvention::IsCurrentParamOnStack() {
  return CurrentParamRegister().IsNoRegister();
}

ManagedRegister X86_64JniCallingConvention::CurrentParamRegister() {
  ManagedRegister res = ManagedRegister::NoRegister();
  if (!IsCurrentParamAFloatOrDouble()) {
    switch (itr_args_ - itr_float_and_doubles_) {
    case 0: res = X86_64ManagedRegister::FromCpuRegister(RDI); break;
    case 1: res = X86_64ManagedRegister::FromCpuRegister(RSI); break;
    case 2: res = X86_64ManagedRegister::FromCpuRegister(RDX); break;
    case 3: res = X86_64ManagedRegister::FromCpuRegister(RCX); break;
    case 4: res = X86_64ManagedRegister::FromCpuRegister(R8); break;
    case 5: res = X86_64ManagedRegister::FromCpuRegister(R9); break;
    }
  } else if (itr_float_and_doubles_ < 8) {
    // First eight float parameters are passed via XMM0..XMM7
    res = X86_64ManagedRegister::FromXmmRegister(
                                 static_cast<FloatRegister>(XMM0 + itr_float_and_doubles_));
  }
  return res;
}

FrameOffset X86_64JniCallingConvention::CurrentParamStackOffset() {
  size_t offset = itr_args_
      - std::min(8U, itr_float_and_doubles_)               // Float arguments passed through Xmm0..Xmm7
      - std::min(6U, itr_args_ - itr_float_and_doubles_);  // Integer arguments passed through GPR
  return FrameOffset(displacement_.Int32Value() - OutArgSize() + (offset * kFramePointerSize));
}

size_t X86_64JniCallingConvention::NumberOfOutgoingStackArgs() {
  size_t static_args = IsStatic() ? 1 : 0;  // count jclass
  // regular argument parameters and this
  size_t param_args = NumArgs() + NumLongOrDoubleArgs();
  // count JNIEnv* and return pc (pushed after Method*)
  size_t total_args = static_args + param_args + 2;

  // Float arguments passed through Xmm0..Xmm7
  // Other (integer) arguments passed through GPR (RDI, RSI, RDX, RCX, R8, R9)
  size_t total_stack_args = total_args
                            - std::min(8U, static_cast<unsigned int>(NumFloatOrDoubleArgs()))
                            - std::min(6U, static_cast<unsigned int>(NumArgs() - NumFloatOrDoubleArgs()));

  return total_stack_args;
}

}  // namespace x86_64
}  // namespace art
