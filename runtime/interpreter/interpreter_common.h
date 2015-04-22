/*
 * Copyright (C) 2012 The Android Open Source Project
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

#ifndef ART_RUNTIME_INTERPRETER_INTERPRETER_COMMON_H_
#define ART_RUNTIME_INTERPRETER_INTERPRETER_COMMON_H_

#include "interpreter.h"

#include <math.h>

#include <iostream>
#include <sstream>

#include "art_field-inl.h"
#include "base/logging.h"
#include "base/macros.h"
#include "class_linker-inl.h"
#include "common_throws.h"
#include "dex_file-inl.h"
#include "dex_instruction-inl.h"
#include "entrypoints/entrypoint_utils-inl.h"
#include "handle_scope-inl.h"
#include "mirror/art_method-inl.h"
#include "mirror/class-inl.h"
#include "mirror/object-inl.h"
#include "mirror/object_array-inl.h"
#include "mirror/string-inl.h"
#include "thread.h"
#include "well_known_classes.h"

using ::art::mirror::ArtMethod;
using ::art::mirror::Array;
using ::art::mirror::BooleanArray;
using ::art::mirror::ByteArray;
using ::art::mirror::CharArray;
using ::art::mirror::Class;
using ::art::mirror::ClassLoader;
using ::art::mirror::IntArray;
using ::art::mirror::LongArray;
using ::art::mirror::Object;
using ::art::mirror::ObjectArray;
using ::art::mirror::ShortArray;
using ::art::mirror::String;
using ::art::mirror::Throwable;

namespace art {
namespace interpreter {

// External references to both interpreter implementations.

template<bool do_access_check, bool transaction_active>
extern JValue ExecuteSwitchImpl(Thread* self, const DexFile::CodeItem* code_item,
                                ShadowFrame& shadow_frame, JValue result_register);

template<bool do_access_check, bool transaction_active>
extern JValue ExecuteGotoImpl(Thread* self, const DexFile::CodeItem* code_item,
                              ShadowFrame& shadow_frame, JValue result_register);

void ThrowNullPointerExceptionFromInterpreter()
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

static inline void DoMonitorEnter(Thread* self, Object* ref) NO_THREAD_SAFETY_ANALYSIS {
  ref->MonitorEnter(self);
}

static inline void DoMonitorExit(Thread* self, Object* ref) NO_THREAD_SAFETY_ANALYSIS {
  ref->MonitorExit(self);
}

void AbortTransactionF(Thread* self, const char* fmt, ...)
    __attribute__((__format__(__printf__, 2, 3)))
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

void AbortTransactionV(Thread* self, const char* fmt, va_list args)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

void RecordArrayElementsInTransaction(mirror::Array* array, int32_t count)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

// Invokes the given method. This is part of the invocation support and is used by DoInvoke and
// DoInvokeVirtualQuick functions.
// Returns true on success, otherwise throws an exception and returns false.
template<bool is_range, bool do_assignability_check>
bool DoCall(ArtMethod* called_method, Thread* self, ShadowFrame& shadow_frame,
            const Instruction* inst, uint16_t inst_data, JValue* result);

// Invokes the given lambda closure. This is part of the invocation support and is used by
// DoLambdaInvoke functions.
// Returns true on success, otherwise throws an exception and returns false.
template<bool is_range, bool do_assignability_check>
bool DoLambdaCall(ArtMethod* called_method, Thread* self, ShadowFrame& shadow_frame,
                  const Instruction* inst, uint16_t inst_data, JValue* result);

// Validates that the art method corresponding to a lambda method target
// is semantically valid:
//
// Must be ACC_STATIC, ACC_LAMBDA. Must be a concrete managed implementation
// (i.e. not native, not proxy, not abstract, ...).
//
// If the validation fails, return false and raise an exception.
static inline bool ValidateLambdaTarget(mirror::ArtMethod* called_method)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  bool success = false;

  if (UNLIKELY(called_method == nullptr)) {
    // The shadow frame should already be pushed, so we don't need to update it.
  } else if (UNLIKELY(called_method->IsAbstract())) {
    ThrowAbstractMethodError(called_method);
    // TODO(iam): Also handle the case when the method is non-static, what error do we throw?
    // TODO(iam): Also make sure that ACC_LAMBDA is set.
  } else if (UNLIKELY(called_method->GetCodeItem() == nullptr)) {
    // Method could be native, proxy method, etc. Lambda targets have to be concrete impls,
    // so don't allow this.
  } else {
    success = true;
  }

  return success;
}

// Handles create-lambda instructions.
// Returns true on success, otherwise throws an exception and returns false.
//
// As a work-in-progress implementation, this shoves the ArtMethod object corresponding
// to the target method ID into the target register vA.
template<bool do_access_check>
static inline bool DoCreateLambda(Thread* self, ShadowFrame& shadow_frame, const Instruction* inst
                                  ) {
  /*
   * create-lambda is always 0x21c opcode
   * - vA is always the target register where the closure will be stored into
   * - vB is always the method ID which will be the target for a later invoke-lambda
   */
  const uint32_t method_idx = inst->VRegB_21c();
  Object* receiver = nullptr;  // Always static. (see 'kStatic')
  mirror::ArtMethod* sf_method = shadow_frame.GetMethod();
  ArtMethod* const called_method = FindMethodFromCode<kStatic, do_access_check>(
      method_idx, &receiver, &sf_method, self);

  if (UNLIKELY(!ValidateLambdaTarget(called_method))) {
    CHECK(self->IsExceptionPending());
    shadow_frame.SetVRegReference(inst->VRegA_21c(), nullptr);
    return false;
  }

  shadow_frame.SetVRegReference(inst->VRegA_21c(), called_method);
  return true;
}

template<bool do_access_check>
static inline bool DoInvokeLambda(Thread* self, ShadowFrame& shadow_frame, const Instruction* inst,
                                  uint16_t inst_data, JValue* result) {
  /*
   * invoke-lambda is always 0x25 opcode
   *
   * - vC is always the closure register
   * - vB is always the number of additional registers up to {vD,vE,vF,vG}
   * - the rest of the registers are always var-args
   *
   * - reading var-args for 0x25 gets us vD,vE,vF,vG (but not vB)
   */
  uint32_t vC = inst->VRegC_25x();

  // TODO(iam): this should be a proper closure instead of just an ArtMethod.
  mirror::Object* vcRef = shadow_frame.GetVRegReference(vC);

  // Guard against the user passing a null closure, which is odd but (sadly) semantically valid.
  if (UNLIKELY(vcRef == nullptr)) {
    ThrowNullPointerExceptionFromInterpreter();
    result->SetJ(0);
    return false;
  }

  ArtMethod* const called_method = vcRef->AsArtMethod();

  if (UNLIKELY(!ValidateLambdaTarget(called_method))) {
    CHECK(self->IsExceptionPending());
    result->SetJ(0);
    return false;
  } else {
    return DoLambdaCall<false, do_access_check>(called_method, self, shadow_frame, inst, inst_data,
                                                result);
  }
}

// Handles invoke-XXX/range instructions.
// Returns true on success, otherwise throws an exception and returns false.
template<InvokeType type, bool is_range, bool do_access_check>
static inline bool DoInvoke(Thread* self, ShadowFrame& shadow_frame, const Instruction* inst,
                            uint16_t inst_data, JValue* result) {
  const uint32_t method_idx = (is_range) ? inst->VRegB_3rc() : inst->VRegB_35c();
  const uint32_t vregC = (is_range) ? inst->VRegC_3rc() : inst->VRegC_35c();
  Object* receiver = (type == kStatic) ? nullptr : shadow_frame.GetVRegReference(vregC);
  mirror::ArtMethod* sf_method = shadow_frame.GetMethod();
  ArtMethod* const called_method = FindMethodFromCode<type, do_access_check>(
      method_idx, &receiver, &sf_method, self);
  // The shadow frame should already be pushed, so we don't need to update it.
  // XX: Why aren't these guards checked by do_access_check? Shouldn't verifier
  // take care of at least the IsAbstract situation if not also failing to find the method target?
  if (UNLIKELY(called_method == nullptr)) {
    CHECK(self->IsExceptionPending());
    result->SetJ(0);
    return false;
  } else if (UNLIKELY(called_method->IsAbstract())) {
    ThrowAbstractMethodError(called_method);
    result->SetJ(0);
    return false;
  } else {
    return DoCall<is_range, do_access_check>(called_method, self, shadow_frame, inst, inst_data,
                                             result);
  }
}

// Handles invoke-virtual-quick and invoke-virtual-quick-range instructions.
// Returns true on success, otherwise throws an exception and returns false.
template<bool is_range>
static inline bool DoInvokeVirtualQuick(Thread* self, ShadowFrame& shadow_frame,
                                        const Instruction* inst, uint16_t inst_data,
                                        JValue* result) {
  const uint32_t vregC = (is_range) ? inst->VRegC_3rc() : inst->VRegC_35c();
  Object* const receiver = shadow_frame.GetVRegReference(vregC);
  if (UNLIKELY(receiver == nullptr)) {
    // We lost the reference to the method index so we cannot get a more
    // precised exception message.
    ThrowNullPointerExceptionFromDexPC();
    return false;
  }
  const uint32_t vtable_idx = (is_range) ? inst->VRegB_3rc() : inst->VRegB_35c();
  CHECK(receiver->GetClass()->ShouldHaveEmbeddedImtAndVTable());
  ArtMethod* const called_method = receiver->GetClass()->GetEmbeddedVTableEntry(vtable_idx);
  if (UNLIKELY(called_method == nullptr)) {
    CHECK(self->IsExceptionPending());
    result->SetJ(0);
    return false;
  } else if (UNLIKELY(called_method->IsAbstract())) {
    ThrowAbstractMethodError(called_method);
    result->SetJ(0);
    return false;
  } else {
    // No need to check since we've been quickened.
    return DoCall<is_range, false>(called_method, self, shadow_frame, inst, inst_data, result);
  }
}

// Handles iget-XXX and sget-XXX instructions.
// Returns true on success, otherwise throws an exception and returns false.
template<FindFieldType find_type, Primitive::Type field_type, bool do_access_check>
bool DoFieldGet(Thread* self, ShadowFrame& shadow_frame, const Instruction* inst,
                uint16_t inst_data) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

// Handles iget-quick, iget-wide-quick and iget-object-quick instructions.
// Returns true on success, otherwise throws an exception and returns false.
template<Primitive::Type field_type>
bool DoIGetQuick(ShadowFrame& shadow_frame, const Instruction* inst, uint16_t inst_data)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

// Handles iput-XXX and sput-XXX instructions.
// Returns true on success, otherwise throws an exception and returns false.
template<FindFieldType find_type, Primitive::Type field_type, bool do_access_check,
         bool transaction_active>
bool DoFieldPut(Thread* self, const ShadowFrame& shadow_frame, const Instruction* inst,
                uint16_t inst_data) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

// Handles iput-quick, iput-wide-quick and iput-object-quick instructions.
// Returns true on success, otherwise throws an exception and returns false.
template<Primitive::Type field_type, bool transaction_active>
bool DoIPutQuick(const ShadowFrame& shadow_frame, const Instruction* inst, uint16_t inst_data)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);


// Handles string resolution for const-string and const-string-jumbo instructions. Also ensures the
// java.lang.String class is initialized.
static inline String* ResolveString(Thread* self, ShadowFrame& shadow_frame, uint32_t string_idx)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  CHECK(!kMovingMethods);
  Class* java_lang_string_class = String::GetJavaLangString();
  if (UNLIKELY(!java_lang_string_class->IsInitialized())) {
    ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
    StackHandleScope<1> hs(self);
    Handle<mirror::Class> h_class(hs.NewHandle(java_lang_string_class));
    if (UNLIKELY(!class_linker->EnsureInitialized(self, h_class, true, true))) {
      DCHECK(self->IsExceptionPending());
      return nullptr;
    }
  }
  mirror::ArtMethod* method = shadow_frame.GetMethod();
  mirror::Class* declaring_class = method->GetDeclaringClass();
  mirror::String* s = declaring_class->GetDexCacheStrings()->Get(string_idx);
  if (UNLIKELY(s == nullptr)) {
    StackHandleScope<1> hs(self);
    Handle<mirror::DexCache> dex_cache(hs.NewHandle(declaring_class->GetDexCache()));
    s = Runtime::Current()->GetClassLinker()->ResolveString(*method->GetDexFile(), string_idx,
                                                            dex_cache);
  }
  return s;
}

// Handles div-int, div-int/2addr, div-int/li16 and div-int/lit8 instructions.
// Returns true on success, otherwise throws a java.lang.ArithmeticException and return false.
static inline bool DoIntDivide(ShadowFrame& shadow_frame, size_t result_reg,
                               int32_t dividend, int32_t divisor)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  constexpr int32_t kMinInt = std::numeric_limits<int32_t>::min();
  if (UNLIKELY(divisor == 0)) {
    ThrowArithmeticExceptionDivideByZero();
    return false;
  }
  if (UNLIKELY(dividend == kMinInt && divisor == -1)) {
    shadow_frame.SetVReg(result_reg, kMinInt);
  } else {
    shadow_frame.SetVReg(result_reg, dividend / divisor);
  }
  return true;
}

// Handles rem-int, rem-int/2addr, rem-int/li16 and rem-int/lit8 instructions.
// Returns true on success, otherwise throws a java.lang.ArithmeticException and return false.
static inline bool DoIntRemainder(ShadowFrame& shadow_frame, size_t result_reg,
                                  int32_t dividend, int32_t divisor)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  constexpr int32_t kMinInt = std::numeric_limits<int32_t>::min();
  if (UNLIKELY(divisor == 0)) {
    ThrowArithmeticExceptionDivideByZero();
    return false;
  }
  if (UNLIKELY(dividend == kMinInt && divisor == -1)) {
    shadow_frame.SetVReg(result_reg, 0);
  } else {
    shadow_frame.SetVReg(result_reg, dividend % divisor);
  }
  return true;
}

// Handles div-long and div-long-2addr instructions.
// Returns true on success, otherwise throws a java.lang.ArithmeticException and return false.
static inline bool DoLongDivide(ShadowFrame& shadow_frame, size_t result_reg,
                                int64_t dividend, int64_t divisor)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  const int64_t kMinLong = std::numeric_limits<int64_t>::min();
  if (UNLIKELY(divisor == 0)) {
    ThrowArithmeticExceptionDivideByZero();
    return false;
  }
  if (UNLIKELY(dividend == kMinLong && divisor == -1)) {
    shadow_frame.SetVRegLong(result_reg, kMinLong);
  } else {
    shadow_frame.SetVRegLong(result_reg, dividend / divisor);
  }
  return true;
}

// Handles rem-long and rem-long-2addr instructions.
// Returns true on success, otherwise throws a java.lang.ArithmeticException and return false.
static inline bool DoLongRemainder(ShadowFrame& shadow_frame, size_t result_reg,
                                   int64_t dividend, int64_t divisor)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  const int64_t kMinLong = std::numeric_limits<int64_t>::min();
  if (UNLIKELY(divisor == 0)) {
    ThrowArithmeticExceptionDivideByZero();
    return false;
  }
  if (UNLIKELY(dividend == kMinLong && divisor == -1)) {
    shadow_frame.SetVRegLong(result_reg, 0);
  } else {
    shadow_frame.SetVRegLong(result_reg, dividend % divisor);
  }
  return true;
}

// Handles filled-new-array and filled-new-array-range instructions.
// Returns true on success, otherwise throws an exception and returns false.
template <bool is_range, bool do_access_check, bool transaction_active>
bool DoFilledNewArray(const Instruction* inst, const ShadowFrame& shadow_frame,
                      Thread* self, JValue* result);

// Handles packed-switch instruction.
// Returns the branch offset to the next instruction to execute.
static inline int32_t DoPackedSwitch(const Instruction* inst, const ShadowFrame& shadow_frame,
                                     uint16_t inst_data)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  DCHECK(inst->Opcode() == Instruction::PACKED_SWITCH);
  const uint16_t* switch_data = reinterpret_cast<const uint16_t*>(inst) + inst->VRegB_31t();
  int32_t test_val = shadow_frame.GetVReg(inst->VRegA_31t(inst_data));
  DCHECK_EQ(switch_data[0], static_cast<uint16_t>(Instruction::kPackedSwitchSignature));
  uint16_t size = switch_data[1];
  DCHECK_GT(size, 0);
  const int32_t* keys = reinterpret_cast<const int32_t*>(&switch_data[2]);
  DCHECK(IsAligned<4>(keys));
  int32_t first_key = keys[0];
  const int32_t* targets = reinterpret_cast<const int32_t*>(&switch_data[4]);
  DCHECK(IsAligned<4>(targets));
  int32_t index = test_val - first_key;
  if (index >= 0 && index < size) {
    return targets[index];
  } else {
    // No corresponding value: move forward by 3 (size of PACKED_SWITCH).
    return 3;
  }
}

// Handles sparse-switch instruction.
// Returns the branch offset to the next instruction to execute.
static inline int32_t DoSparseSwitch(const Instruction* inst, const ShadowFrame& shadow_frame,
                                     uint16_t inst_data)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  DCHECK(inst->Opcode() == Instruction::SPARSE_SWITCH);
  const uint16_t* switch_data = reinterpret_cast<const uint16_t*>(inst) + inst->VRegB_31t();
  int32_t test_val = shadow_frame.GetVReg(inst->VRegA_31t(inst_data));
  DCHECK_EQ(switch_data[0], static_cast<uint16_t>(Instruction::kSparseSwitchSignature));
  uint16_t size = switch_data[1];
  // Return length of SPARSE_SWITCH if size is 0.
  if (size == 0) {
    return 3;
  }
  const int32_t* keys = reinterpret_cast<const int32_t*>(&switch_data[2]);
  DCHECK(IsAligned<4>(keys));
  const int32_t* entries = keys + size;
  DCHECK(IsAligned<4>(entries));
  int lo = 0;
  int hi = size - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    int32_t foundVal = keys[mid];
    if (test_val < foundVal) {
      hi = mid - 1;
    } else if (test_val > foundVal) {
      lo = mid + 1;
    } else {
      return entries[mid];
    }
  }
  // No corresponding value: move forward by 3 (size of SPARSE_SWITCH).
  return 3;
}

uint32_t FindNextInstructionFollowingException(Thread* self, ShadowFrame& shadow_frame,
    uint32_t dex_pc, const instrumentation::Instrumentation* instrumentation)
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

NO_RETURN void UnexpectedOpcode(const Instruction* inst, const ShadowFrame& shadow_frame)
  __attribute__((cold))
  SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

static inline void TraceExecution(const ShadowFrame& shadow_frame, const Instruction* inst,
                                  const uint32_t dex_pc)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  constexpr bool kTracing = false;
  if (kTracing) {
#define TRACE_LOG std::cerr
    std::ostringstream oss;
    oss << PrettyMethod(shadow_frame.GetMethod())
        << StringPrintf("\n0x%x: ", dex_pc)
        << inst->DumpString(shadow_frame.GetMethod()->GetDexFile()) << "\n";
    for (uint32_t i = 0; i < shadow_frame.NumberOfVRegs(); ++i) {
      uint32_t raw_value = shadow_frame.GetVReg(i);
      Object* ref_value = shadow_frame.GetVRegReference(i);
      oss << StringPrintf(" vreg%u=0x%08X", i, raw_value);
      if (ref_value != nullptr) {
        if (ref_value->GetClass()->IsStringClass() &&
            ref_value->AsString()->GetValue() != nullptr) {
          oss << "/java.lang.String \"" << ref_value->AsString()->ToModifiedUtf8() << "\"";
        } else {
          oss << "/" << PrettyTypeOf(ref_value);
        }
      }
    }
    TRACE_LOG << oss.str() << "\n";
#undef TRACE_LOG
  }
}

static inline bool IsBackwardBranch(int32_t branch_offset) {
  return branch_offset <= 0;
}

// Explicitly instantiate all DoInvoke functions.
#define EXPLICIT_DO_INVOKE_TEMPLATE_DECL(_type, _is_range, _do_check)                      \
  template SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)                                     \
  bool DoInvoke<_type, _is_range, _do_check>(Thread* self, ShadowFrame& shadow_frame,      \
                                             const Instruction* inst, uint16_t inst_data,  \
                                             JValue* result)

#define EXPLICIT_DO_INVOKE_ALL_TEMPLATE_DECL(_type)       \
  EXPLICIT_DO_INVOKE_TEMPLATE_DECL(_type, false, false);  \
  EXPLICIT_DO_INVOKE_TEMPLATE_DECL(_type, false, true);   \
  EXPLICIT_DO_INVOKE_TEMPLATE_DECL(_type, true, false);   \
  EXPLICIT_DO_INVOKE_TEMPLATE_DECL(_type, true, true);

EXPLICIT_DO_INVOKE_ALL_TEMPLATE_DECL(kStatic)      // invoke-static/range.
EXPLICIT_DO_INVOKE_ALL_TEMPLATE_DECL(kDirect)      // invoke-direct/range.
EXPLICIT_DO_INVOKE_ALL_TEMPLATE_DECL(kVirtual)     // invoke-virtual/range.
EXPLICIT_DO_INVOKE_ALL_TEMPLATE_DECL(kSuper)       // invoke-super/range.
EXPLICIT_DO_INVOKE_ALL_TEMPLATE_DECL(kInterface)   // invoke-interface/range.
#undef EXPLICIT_DO_INVOKE_ALL_TEMPLATE_DECL
#undef EXPLICIT_DO_INVOKE_TEMPLATE_DECL

// Explicitly instantiate all DoInvokeVirtualQuick functions.
#define EXPLICIT_DO_INVOKE_VIRTUAL_QUICK_TEMPLATE_DECL(_is_range)                    \
  template SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)                               \
  bool DoInvokeVirtualQuick<_is_range>(Thread* self, ShadowFrame& shadow_frame,      \
                                       const Instruction* inst, uint16_t inst_data,  \
                                       JValue* result)

EXPLICIT_DO_INVOKE_VIRTUAL_QUICK_TEMPLATE_DECL(false);  // invoke-virtual-quick.
EXPLICIT_DO_INVOKE_VIRTUAL_QUICK_TEMPLATE_DECL(true);   // invoke-virtual-quick-range.
#undef EXPLICIT_INSTANTIATION_DO_INVOKE_VIRTUAL_QUICK

// Explicitly instantiate all DoCreateLambda functions.
#define EXPLICIT_DO_CREATE_LAMBDA_DECL(_do_check)                                    \
template SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)                                 \
bool DoCreateLambda<_do_check>(Thread* self, ShadowFrame& shadow_frame,              \
                        const Instruction* inst)

EXPLICIT_DO_CREATE_LAMBDA_DECL(false);  // create-lambda
EXPLICIT_DO_CREATE_LAMBDA_DECL(true);   // create-lambda
#undef EXPLICIT_DO_CREATE_LAMBDA_DECL

// Explicitly instantiate all DoInvokeLambda functions.
#define EXPLICIT_DO_INVOKE_LAMBDA_DECL(_do_check)                                    \
template SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)                                 \
bool DoInvokeLambda<_do_check>(Thread* self, ShadowFrame& shadow_frame, const Instruction* inst, \
                               uint16_t inst_data, JValue* result);

EXPLICIT_DO_INVOKE_LAMBDA_DECL(false);  // invoke-lambda
EXPLICIT_DO_INVOKE_LAMBDA_DECL(true);   // invoke-lambda
#undef EXPLICIT_DO_INVOKE_LAMBDA_DECL


}  // namespace interpreter
}  // namespace art

#endif  // ART_RUNTIME_INTERPRETER_INTERPRETER_COMMON_H_
