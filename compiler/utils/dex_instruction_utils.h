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

#ifndef ART_COMPILER_UTILS_DEX_INSTRUCTION_UTILS_H_
#define ART_COMPILER_UTILS_DEX_INSTRUCTION_UTILS_H_

#include "dex_instruction.h"

namespace art {

// Dex invoke type corresponds to the ordering of INVOKE instructions;
// this order is the same for range and non-range invokes.
enum DexInvokeType : uint8_t {
  kDexInvokeVirtual = 0,  // invoke-virtual, invoke-virtual-range
  kDexInvokeSuper,        // invoke-super, invoke-super-range
  kDexInvokeDirect,       // invoke-direct, invoke-direct-range
  kDexInvokeStatic,       // invoke-static, invoke-static-range
  kDexInvokeInterface,    // invoke-interface, invoke-interface-range
  kDexInvokeTypeCount
};

// Dex instruction memory access types correspond to the ordering of GET/PUT instructions;
// this order is the same for IGET, IPUT, SGET, SPUT, AGET and APUT.
enum DexMemAccessType : uint8_t {
  kDexMemAccessWord = 0,  // op         0; int or float, the actual type is not encoded.
  kDexMemAccessWide,      // op_WIDE    1; long or double, the actual type is not encoded.
  kDexMemAccessObject,    // op_OBJECT  2; the actual reference type is not encoded.
  kDexMemAccessBoolean,   // op_BOOLEAN 3
  kDexMemAccessByte,      // op_BYTE    4
  kDexMemAccessChar,      // op_CHAR    5
  kDexMemAccessShort,     // op_SHORT   6
  kDexMemAccessTypeCount
};

std::ostream& operator<<(std::ostream& os, const DexMemAccessType& type);

// NOTE: The following functions disregard quickened instructions.

constexpr bool IsInstructionInvoke(Instruction::Code opcode) {
  return Instruction::INVOKE_VIRTUAL <= opcode && opcode <= Instruction::INVOKE_INTERFACE_RANGE &&
      opcode != Instruction::RETURN_VOID_BARRIER;
}

constexpr bool IsInstructionInvokeStatic(Instruction::Code opcode) {
  return opcode == Instruction::INVOKE_STATIC || opcode == Instruction::INVOKE_STATIC_RANGE;
}

constexpr bool IsInstructionIfCc(Instruction::Code opcode) {
  return Instruction::IF_EQ <= opcode && opcode <= Instruction::IF_LE;
}

constexpr bool IsInstructionIfCcZ(Instruction::Code opcode) {
  return Instruction::IF_EQZ <= opcode && opcode <= Instruction::IF_LEZ;
}

constexpr bool IsInstructionIGet(Instruction::Code code) {
  return Instruction::IGET <= code && code <= Instruction::IGET_SHORT;
}

constexpr bool IsInstructionIPut(Instruction::Code code) {
  return Instruction::IPUT <= code && code <= Instruction::IPUT_SHORT;
}

constexpr bool IsInstructionSGet(Instruction::Code code) {
  return Instruction::SGET <= code && code <= Instruction::SGET_SHORT;
}

constexpr bool IsInstructionSPut(Instruction::Code code) {
  return Instruction::SPUT <= code && code <= Instruction::SPUT_SHORT;
}

constexpr bool IsInstructionAGet(Instruction::Code code) {
  return Instruction::AGET <= code && code <= Instruction::AGET_SHORT;
}

constexpr bool IsInstructionAPut(Instruction::Code code) {
  return Instruction::APUT <= code && code <= Instruction::APUT_SHORT;
}

constexpr bool IsInstructionIGetOrIPut(Instruction::Code code) {
  return Instruction::IGET <= code && code <= Instruction::IPUT_SHORT;
}

constexpr bool IsInstructionSGetOrSPut(Instruction::Code code) {
  return Instruction::SGET <= code && code <= Instruction::SPUT_SHORT;
}

constexpr bool IsInstructionAGetOrAPut(Instruction::Code code) {
  return Instruction::AGET <= code && code <= Instruction::APUT_SHORT;
}

// TODO: Remove the #if guards below when we fully migrate to C++14.

constexpr bool IsInvokeInstructionRange(Instruction::Code opcode) {
#if __cplusplus >= 201402  // C++14 allows the DCHECK() in constexpr functions.
  DCHECK(IsInstructionInvoke(opcode));
#endif
  return opcode >= Instruction::INVOKE_VIRTUAL_RANGE;
}

constexpr DexInvokeType InvokeInstructionType(Instruction::Code opcode) {
#if __cplusplus >= 201402  // C++14 allows the DCHECK() in constexpr functions.
  DCHECK(IsInstructionInvoke(opcode));
#endif
  return static_cast<DexInvokeType>(IsInvokeInstructionRange(opcode)
                                    ? (opcode - Instruction::INVOKE_VIRTUAL_RANGE)
                                    : (opcode - Instruction::INVOKE_VIRTUAL));
}

constexpr DexMemAccessType IGetMemAccessType(Instruction::Code code) {
#if __cplusplus >= 201402  // C++14 allows the DCHECK() in constexpr functions.
  DCHECK(IsInstructionIGet(opcode));
#endif
  return static_cast<DexMemAccessType>(code - Instruction::IGET);
}

constexpr DexMemAccessType IPutMemAccessType(Instruction::Code code) {
#if __cplusplus >= 201402  // C++14 allows the DCHECK() in constexpr functions.
  DCHECK(IsInstructionIPut(opcode));
#endif
  return static_cast<DexMemAccessType>(code - Instruction::IPUT);
}

constexpr DexMemAccessType SGetMemAccessType(Instruction::Code code) {
#if __cplusplus >= 201402  // C++14 allows the DCHECK() in constexpr functions.
  DCHECK(IsInstructionSGet(opcode));
#endif
  return static_cast<DexMemAccessType>(code - Instruction::SGET);
}

constexpr DexMemAccessType SPutMemAccessType(Instruction::Code code) {
#if __cplusplus >= 201402  // C++14 allows the DCHECK() in constexpr functions.
  DCHECK(IsInstructionSPut(opcode));
#endif
  return static_cast<DexMemAccessType>(code - Instruction::SPUT);
}

constexpr DexMemAccessType AGetMemAccessType(Instruction::Code code) {
#if __cplusplus >= 201402  // C++14 allows the DCHECK() in constexpr functions.
  DCHECK(IsInstructionAGet(opcode));
#endif
  return static_cast<DexMemAccessType>(code - Instruction::AGET);
}

constexpr DexMemAccessType APutMemAccessType(Instruction::Code code) {
#if __cplusplus >= 201402  // C++14 allows the DCHECK() in constexpr functions.
  DCHECK(IsInstructionAPut(opcode));
#endif
  return static_cast<DexMemAccessType>(code - Instruction::APUT);
}

constexpr DexMemAccessType IGetOrIPutMemAccessType(Instruction::Code code) {
#if __cplusplus >= 201402  // C++14 allows the DCHECK() in constexpr functions.
  DCHECK(IsInstructionIGetOrIPut(opcode));
#endif
  return (code >= Instruction::IPUT) ? IPutMemAccessType(code) : IGetMemAccessType(code);
}

constexpr DexMemAccessType SGetOrSPutMemAccessType(Instruction::Code code) {
#if __cplusplus >= 201402  // C++14 allows the DCHECK() in constexpr functions.
  DCHECK(IsInstructionSGetOrSPut(opcode));
#endif
  return (code >= Instruction::SPUT) ? SPutMemAccessType(code) : SGetMemAccessType(code);
}

constexpr DexMemAccessType AGetOrAPutMemAccessType(Instruction::Code code) {
#if __cplusplus >= 201402  // C++14 allows the DCHECK() in constexpr functions.
  DCHECK(IsInstructionAGetOrAPut(opcode));
#endif
  return (code >= Instruction::APUT) ? APutMemAccessType(code) : AGetMemAccessType(code);
}

}  // namespace art

#endif  // ART_COMPILER_UTILS_DEX_INSTRUCTION_UTILS_H_
