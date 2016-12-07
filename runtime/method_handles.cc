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

#include "method_handles-inl.h"

#include "android-base/stringprintf.h"

#include "common_dex_operations.h"
#include "jvalue.h"
#include "jvalue-inl.h"
#include "mirror/emulated_stack_frame.h"
#include "mirror/method_handle_impl.h"
#include "mirror/method_type.h"
#include "reflection.h"
#include "reflection-inl.h"
#include "well_known_classes.h"

namespace art {

using android::base::StringPrintf;

namespace {

#define PRIMITIVES_LIST(V) \
  V(Primitive::kPrimBoolean, Boolean, Boolean, Z) \
  V(Primitive::kPrimByte, Byte, Byte, B)          \
  V(Primitive::kPrimChar, Char, Character, C)     \
  V(Primitive::kPrimShort, Short, Short, S)       \
  V(Primitive::kPrimInt, Int, Integer, I)         \
  V(Primitive::kPrimLong, Long, Long, J)          \
  V(Primitive::kPrimFloat, Float, Float, F)       \
  V(Primitive::kPrimDouble, Double, Double, D)

// Assigns |type| to the primitive type associated with |klass|. Returns
// true iff. |klass| was a boxed type (Integer, Long etc.), false otherwise.
bool GetUnboxedPrimitiveType(ObjPtr<mirror::Class> klass, Primitive::Type* type)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ScopedAssertNoThreadSuspension ants(__FUNCTION__);
#define LOOKUP_PRIMITIVE(primitive, _, __, ___)                         \
  if (klass->DescriptorEquals(Primitive::BoxedDescriptor(primitive))) { \
    *type = primitive;                                                  \
    return true;                                                        \
  }

  PRIMITIVES_LIST(LOOKUP_PRIMITIVE);
#undef LOOKUP_PRIMITIVE
  return false;
}

ObjPtr<mirror::Class> GetBoxedPrimitiveClass(Primitive::Type type)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ScopedAssertNoThreadSuspension ants(__FUNCTION__);
  jmethodID m = nullptr;
  switch (type) {
#define CASE_PRIMITIVE(primitive, _, java_name, __)              \
    case primitive:                                              \
      m = WellKnownClasses::java_lang_ ## java_name ## _valueOf; \
      break;
    PRIMITIVES_LIST(CASE_PRIMITIVE);
#undef CASE_PRIMITIVE
    case Primitive::Type::kPrimNot:
    case Primitive::Type::kPrimVoid:
      return nullptr;
  }
  return jni::DecodeArtMethod(m)->GetDeclaringClass();
}

bool GetUnboxedTypeAndValue(ObjPtr<mirror::Object> o, Primitive::Type* type, JValue* value)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ScopedAssertNoThreadSuspension ants(__FUNCTION__);
  ObjPtr<mirror::Class> klass = o->GetClass();
  ArtField* primitive_field = &klass->GetIFieldsPtr()->At(0);
#define CASE_PRIMITIVE(primitive, abbrev, _, shorthand)         \
  if (klass == GetBoxedPrimitiveClass(primitive)) {             \
    *type = primitive;                                          \
    value->Set ## shorthand(primitive_field->Get ## abbrev(o)); \
    return true;                                                \
  }
  PRIMITIVES_LIST(CASE_PRIMITIVE)
#undef CASE_PRIMITIVE
  return false;
}

inline bool IsReferenceType(Primitive::Type type) {
  return type == Primitive::kPrimNot;
}

inline bool IsPrimitiveType(Primitive::Type type) {
  return !IsReferenceType(type);
}

}  // namespace

bool IsParameterTypeConvertible(ObjPtr<mirror::Class> from, ObjPtr<mirror::Class> to)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  // This function returns true if there's any conceivable conversion
  // between |from| and |to|. It's expected this method will be used
  // to determine if a WrongMethodTypeException should be raised. The
  // decision logic follows the documentation for MethodType.asType().
  if (from == to) {
    return true;
  }

  Primitive::Type from_primitive = from->GetPrimitiveType();
  Primitive::Type to_primitive = to->GetPrimitiveType();
  DCHECK(from_primitive != Primitive::Type::kPrimVoid);
  DCHECK(to_primitive != Primitive::Type::kPrimVoid);

  // If |to| and |from| are references.
  if (IsReferenceType(from_primitive) && IsReferenceType(to_primitive)) {
    // Assignability is determined during parameter conversion when
    // invoking the associated method handle.
    return true;
  }

  // If |to| and |from| are primitives and a widening conversion exists.
  if (Primitive::IsWidenable(from_primitive, to_primitive)) {
    return true;
  }

  // If |to| is a reference and |from| is a primitive, then boxing conversion.
  if (IsReferenceType(to_primitive) && IsPrimitiveType(from_primitive)) {
    return to->IsAssignableFrom(GetBoxedPrimitiveClass(from_primitive));
  }

  // If |from| is a reference and |to| is a primitive, then unboxing conversion.
  if (IsPrimitiveType(to_primitive) && IsReferenceType(from_primitive)) {
    if (from->DescriptorEquals("Ljava/lang/Object;")) {
      // Object might be converted into a primitive during unboxing.
      return true;
    } else if (Primitive::IsNumericType(to_primitive) &&
               from->DescriptorEquals("Ljava/lang/Number;")) {
      // Number might be unboxed into any of the number primitive types.
      return true;
    }
    Primitive::Type unboxed_type;
    if (GetUnboxedPrimitiveType(from, &unboxed_type)) {
      if (unboxed_type == to_primitive) {
        // Straightforward unboxing conversion such as Boolean => boolean.
        return true;
      } else {
        // Check if widening operations for numeric primitives would work,
        // such as Byte => byte => long.
        return Primitive::IsWidenable(unboxed_type, to_primitive);
      }
    }
  }

  return false;
}

bool IsReturnTypeConvertible(ObjPtr<mirror::Class> from, ObjPtr<mirror::Class> to)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  if (to->GetPrimitiveType() == Primitive::Type::kPrimVoid) {
    // Result will be ignored.
    return true;
  } else if (from->GetPrimitiveType() == Primitive::Type::kPrimVoid) {
    // Returned value will be 0 / null.
    return true;
  } else {
    // Otherwise apply usual parameter conversion rules.
    return IsParameterTypeConvertible(from, to);
  }
}

bool ConvertJValueCommon(
    Handle<mirror::MethodType> callsite_type,
    Handle<mirror::MethodType> callee_type,
    ObjPtr<mirror::Class> from,
    ObjPtr<mirror::Class> to,
    JValue* value) {
  // The reader maybe concerned about the safety of the heap object
  // that may be in |value|. There is only one case where allocation
  // is obviously needed and that's for boxing. However, in the case
  // of boxing |value| contains a non-reference type.

  const Primitive::Type from_type = from->GetPrimitiveType();
  const Primitive::Type to_type = to->GetPrimitiveType();

  // Put incoming value into |src_value| and set return value to 0.
  // Errors and conversions from void require the return value to be 0.
  const JValue src_value(*value);
  value->SetJ(0);

  // Conversion from void set result to zero.
  if (from_type == Primitive::kPrimVoid) {
    return true;
  }

  // This method must be called only when the types don't match.
  DCHECK(from != to);

  if (IsPrimitiveType(from_type) && IsPrimitiveType(to_type)) {
    // The source and target types are both primitives.
    if (UNLIKELY(!ConvertPrimitiveValueNoThrow(from_type, to_type, src_value, value))) {
      ThrowWrongMethodTypeException(callee_type.Get(), callsite_type.Get());
      return false;
    }
    return true;
  } else if (IsReferenceType(from_type) && IsReferenceType(to_type)) {
    // They're both reference types. If "from" is null, we can pass it
    // through unchanged. If not, we must generate a cast exception if
    // |to| is not assignable from the dynamic type of |ref|.
    //
    // Playing it safe with StackHandleScope here, not expecting any allocation
    // in mirror::Class::IsAssignable().
    StackHandleScope<2> hs(Thread::Current());
    Handle<mirror::Class> h_to(hs.NewHandle(to));
    Handle<mirror::Object> h_obj(hs.NewHandle(src_value.GetL()));
    if (h_obj.Get() != nullptr && !to->IsAssignableFrom(h_obj->GetClass())) {
      ThrowClassCastException(h_to.Get(), h_obj->GetClass());
      return false;
    }
    value->SetL(h_obj.Get());
    return true;
  } else if (IsReferenceType(to_type)) {
    DCHECK(IsPrimitiveType(from_type));
    // The source type is a primitive and the target type is a reference, so we must box.
    // The target type maybe a super class of the boxed source type, for example,
    // if the source type is int, it's boxed type is java.lang.Integer, and the target
    // type could be java.lang.Number.
    Primitive::Type type;
    if (!GetUnboxedPrimitiveType(to, &type)) {
      ObjPtr<mirror::Class> boxed_from_class = GetBoxedPrimitiveClass(from_type);
      if (boxed_from_class->IsSubClass(to)) {
        type = from_type;
      } else {
        ThrowWrongMethodTypeException(callee_type.Get(), callsite_type.Get());
        return false;
      }
    }

    if (UNLIKELY(from_type != type)) {
      ThrowWrongMethodTypeException(callee_type.Get(), callsite_type.Get());
      return false;
    }

    if (!ConvertPrimitiveValueNoThrow(from_type, type, src_value, value)) {
      ThrowWrongMethodTypeException(callee_type.Get(), callsite_type.Get());
      return false;
    }

    // Then perform the actual boxing, and then set the reference.
    ObjPtr<mirror::Object> boxed = BoxPrimitive(type, src_value);
    value->SetL(boxed.Ptr());
    return true;
  } else {
    // The source type is a reference and the target type is a primitive, so we must unbox.
    DCHECK(IsReferenceType(from_type));
    DCHECK(IsPrimitiveType(to_type));

    ObjPtr<mirror::Object> from_obj(src_value.GetL());
    if (UNLIKELY(from_obj == nullptr)) {
      ThrowNullPointerException(
          StringPrintf("Expected to unbox a '%s' primitive type but was returned null",
                       from->PrettyDescriptor().c_str()).c_str());
      return false;
    }

    Primitive::Type unboxed_type;
    JValue unboxed_value;
    if (UNLIKELY(!GetUnboxedTypeAndValue(from_obj, &unboxed_type, &unboxed_value))) {
      ThrowWrongMethodTypeException(callee_type.Get(), callsite_type.Get());
      return false;
    }

    if (UNLIKELY(!ConvertPrimitiveValueNoThrow(unboxed_type, to_type, unboxed_value, value))) {
      ThrowClassCastException(from, to);
      return false;
    }

    return true;
  }
}

namespace {

template <bool is_range>
inline void CopyArgumentsFromCallerFrame(const ShadowFrame& caller_frame,
                                         ShadowFrame* callee_frame,
                                         const uint32_t (&args)[Instruction::kMaxVarArgRegs],
                                         uint32_t first_arg,
                                         const size_t first_dst_reg,
                                         const size_t num_regs)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  for (size_t i = 0; i < num_regs; ++i) {
    size_t dst_reg = first_dst_reg + i;
    size_t src_reg = is_range ? (first_arg + i) : args[i];
    // Uint required, so that sign extension does not make this wrong on 64-bit systems
    uint32_t src_value = caller_frame.GetVReg(src_reg);
    ObjPtr<mirror::Object> o = caller_frame.GetVRegReference<kVerifyNone>(src_reg);
    // If both register locations contains the same value, the register probably holds a reference.
    // Note: As an optimization, non-moving collectors leave a stale reference value
    // in the references array even after the original vreg was overwritten to a non-reference.
    if (src_value == reinterpret_cast<uintptr_t>(o.Ptr())) {
      callee_frame->SetVRegReference(dst_reg, o.Ptr());
    } else {
      callee_frame->SetVReg(dst_reg, src_value);
    }
  }
}

template <bool is_range>
inline bool ConvertAndCopyArgumentsFromCallerFrame(
    Thread* self,
    Handle<mirror::MethodType> callsite_type,
    Handle<mirror::MethodType> callee_type,
    const ShadowFrame& caller_frame,
    const uint32_t (&args)[Instruction::kMaxVarArgRegs],
    uint32_t first_arg,
    uint32_t first_dst_reg,
    ShadowFrame* callee_frame)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ObjPtr<mirror::ObjectArray<mirror::Class>> from_types(callsite_type->GetPTypes());
  ObjPtr<mirror::ObjectArray<mirror::Class>> to_types(callee_type->GetPTypes());

  const int32_t num_method_params = from_types->GetLength();
  if (to_types->GetLength() != num_method_params) {
    ThrowWrongMethodTypeException(callee_type.Get(), callsite_type.Get());
    return false;
  }

  ShadowFrameGetter<is_range> getter(first_arg, args, caller_frame);
  ShadowFrameSetter setter(callee_frame, first_dst_reg);

  return PerformConversions<ShadowFrameGetter<is_range>, ShadowFrameSetter>(self,
                                                                            callsite_type,
                                                                            callee_type,
                                                                            &getter,
                                                                            &setter,
                                                                            num_method_params);
}

inline bool IsMethodHandleInvokeExact(const ArtMethod* const method) {
  if (method == jni::DecodeArtMethod(WellKnownClasses::java_lang_invoke_MethodHandle_invokeExact)) {
    return true;
  } else {
    DCHECK_EQ(method, jni::DecodeArtMethod(WellKnownClasses::java_lang_invoke_MethodHandle_invoke));
    return false;
  }
}

inline bool IsInvoke(const mirror::MethodHandle::Kind handle_kind) {
  return handle_kind <= mirror::MethodHandle::Kind::kLastInvokeKind;
}

inline bool IsInvokeTransform(const mirror::MethodHandle::Kind handle_kind) {
  return (handle_kind == mirror::MethodHandle::Kind::kInvokeTransform
          || handle_kind == mirror::MethodHandle::Kind::kInvokeCallSiteTransform);
}

inline bool IsFieldAccess(mirror::MethodHandle::Kind handle_kind) {
  return (handle_kind >= mirror::MethodHandle::Kind::kFirstAccessorKind
          && handle_kind <= mirror::MethodHandle::Kind::kLastAccessorKind);
}

// Calculate the number of ins for a proxy or native method, where we
// can't just look at the code item.
static inline size_t GetInsForProxyOrNativeMethod(ArtMethod* method)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  DCHECK(method->IsNative() || method->IsProxyMethod());

  method = method->GetInterfaceMethodIfProxy(kRuntimePointerSize);
  size_t num_ins = 0;
  // Separate accounting for the receiver, which isn't a part of the
  // shorty.
  if (!method->IsStatic()) {
    ++num_ins;
  }

  uint32_t shorty_len = 0;
  const char* shorty = method->GetShorty(&shorty_len);
  for (size_t i = 1; i < shorty_len; ++i) {
    const char c = shorty[i];
    ++num_ins;
    if (c == 'J' || c == 'D') {
      ++num_ins;
    }
  }

  return num_ins;
}

// Returns true iff. the callsite type for a polymorphic invoke is transformer
// like, i.e that it has a single input argument whose type is
// dalvik.system.EmulatedStackFrame.
static inline bool IsCallerTransformer(Handle<mirror::MethodType> callsite_type)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ObjPtr<mirror::ObjectArray<mirror::Class>> param_types(callsite_type->GetPTypes());
  if (param_types->GetLength() == 1) {
    ObjPtr<mirror::Class> param(param_types->GetWithoutChecks(0));
    return param == WellKnownClasses::ToClass(WellKnownClasses::dalvik_system_EmulatedStackFrame);
  }

  return false;
}

template <bool is_range>
static inline bool DoCallPolymorphic(ArtMethod* called_method,
                                     Handle<mirror::MethodType> callsite_type,
                                     Handle<mirror::MethodType> target_type,
                                     Thread* self,
                                     ShadowFrame& shadow_frame,
                                     const uint32_t (&args)[Instruction::kMaxVarArgRegs],
                                     uint32_t first_arg,
                                     JValue* result,
                                     const mirror::MethodHandle::Kind handle_kind)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  // Compute method information.
  const DexFile::CodeItem* code_item = called_method->GetCodeItem();

  // Number of registers for the callee's call frame. Note that for non-exact
  // invokes, we always derive this information from the callee method. We
  // cannot guarantee during verification that the number of registers encoded
  // in the invoke is equal to the number of ins for the callee. This is because
  // some transformations (such as boxing a long -> Long or wideining an
  // int -> long will change that number.
  uint16_t num_regs;
  size_t num_input_regs;
  size_t first_dest_reg;
  if (LIKELY(code_item != nullptr)) {
    num_regs = code_item->registers_size_;
    first_dest_reg = num_regs - code_item->ins_size_;
    num_input_regs = code_item->ins_size_;
    // Parameter registers go at the end of the shadow frame.
    DCHECK_NE(first_dest_reg, (size_t)-1);
  } else {
    // No local regs for proxy and native methods.
    DCHECK(called_method->IsNative() || called_method->IsProxyMethod());
    num_regs = num_input_regs = GetInsForProxyOrNativeMethod(called_method);
    first_dest_reg = 0;
  }

  // Allocate shadow frame on the stack.
  ShadowFrameAllocaUniquePtr shadow_frame_unique_ptr =
      CREATE_SHADOW_FRAME(num_regs, &shadow_frame, called_method, /* dex pc */ 0);
  ShadowFrame* new_shadow_frame = shadow_frame_unique_ptr.get();

  // Whether this polymorphic invoke was issued by a transformer method.
  bool is_caller_transformer = false;
  // Thread might be suspended during PerformArgumentConversions due to the
  // allocations performed during boxing.
  {
    ScopedStackedShadowFramePusher pusher(
        self, new_shadow_frame, StackedShadowFrameType::kShadowFrameUnderConstruction);
    if (callsite_type->IsExactMatch(target_type.Get())) {
      // This is an exact invoke, we can take the fast path of just copying all
      // registers without performing any argument conversions.
      CopyArgumentsFromCallerFrame<is_range>(shadow_frame,
                                             new_shadow_frame,
                                             args,
                                             first_arg,
                                             first_dest_reg,
                                             num_input_regs);
    } else {
      // This includes the case where we're entering this invoke-polymorphic
      // from a transformer method. In that case, the callsite_type will contain
      // a single argument of type dalvik.system.EmulatedStackFrame. In that
      // case, we'll have to unmarshal the EmulatedStackFrame into the
      // new_shadow_frame and perform argument conversions on it.
      if (IsCallerTransformer(callsite_type)) {
        is_caller_transformer = true;
        // The emulated stack frame is the first and only argument when we're coming
        // through from a transformer.
        size_t first_arg_register = (is_range) ? first_arg : args[0];
        ObjPtr<mirror::EmulatedStackFrame> emulated_stack_frame(
            reinterpret_cast<mirror::EmulatedStackFrame*>(
                shadow_frame.GetVRegReference(first_arg_register)));
        if (!emulated_stack_frame->WriteToShadowFrame(self,
                                                      target_type,
                                                      first_dest_reg,
                                                      new_shadow_frame)) {
          DCHECK(self->IsExceptionPending());
          result->SetL(0);
          return false;
        }
      } else if (!ConvertAndCopyArgumentsFromCallerFrame<is_range>(self,
                                                                   callsite_type,
                                                                   target_type,
                                                                   shadow_frame,
                                                                   args,
                                                                   first_arg,
                                                                   first_dest_reg,
                                                                   new_shadow_frame)) {
        DCHECK(self->IsExceptionPending());
        result->SetL(0);
        return false;
      }
    }
  }

  // See TODO in DoInvokePolymorphic : We need to perform this dynamic, receiver
  // based dispatch right before we perform the actual call, because the
  // receiver isn't known very early.
  if (handle_kind == mirror::MethodHandle::Kind::kInvokeVirtual ||
      handle_kind == mirror::MethodHandle::Kind::kInvokeInterface) {
    ObjPtr<mirror::Object> receiver(new_shadow_frame->GetVRegReference(first_dest_reg));
    ObjPtr<mirror::Class> declaring_class(called_method->GetDeclaringClass());
    // Verify that _vRegC is an object reference and of the type expected by
    // the receiver.
    if (!VerifyObjectIsClass(receiver, declaring_class)) {
      DCHECK(self->IsExceptionPending());
      return false;
    }

    called_method = receiver->GetClass()->FindVirtualMethodForVirtualOrInterface(
        called_method, kRuntimePointerSize);
  }

  PerformCall(self, code_item, shadow_frame.GetMethod(), first_dest_reg, new_shadow_frame, result);
  if (self->IsExceptionPending()) {
    return false;
  }

  // If the caller of this signature polymorphic method was a transformer,
  // we need to copy the result back out to the emulated stack frame.
  if (is_caller_transformer) {
    StackHandleScope<2> hs(self);
    size_t first_callee_register = is_range ? (first_arg) : args[0];
    Handle<mirror::EmulatedStackFrame> emulated_stack_frame(
        hs.NewHandle(reinterpret_cast<mirror::EmulatedStackFrame*>(
            shadow_frame.GetVRegReference(first_callee_register))));
    Handle<mirror::MethodType> emulated_stack_type(hs.NewHandle(emulated_stack_frame->GetType()));
    JValue local_result;
    local_result.SetJ(result->GetJ());

    if (ConvertReturnValue(emulated_stack_type, target_type, &local_result)) {
      emulated_stack_frame->SetReturnValue(self, local_result);
      return true;
    } else {
      DCHECK(self->IsExceptionPending());
      return false;
    }
  } else {
    return ConvertReturnValue(callsite_type, target_type, result);
  }
}

template <bool is_range>
static inline bool DoCallTransform(ArtMethod* called_method,
                                   Handle<mirror::MethodType> callsite_type,
                                   Handle<mirror::MethodType> callee_type,
                                   Thread* self,
                                   ShadowFrame& shadow_frame,
                                   Handle<mirror::MethodHandleImpl> receiver,
                                   const uint32_t (&args)[Instruction::kMaxVarArgRegs],
                                   uint32_t first_arg,
                                   JValue* result)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  // This can be fixed to two, because the method we're calling here
  // (MethodHandle.transformInternal) doesn't have any locals and the signature
  // is known :
  //
  // private MethodHandle.transformInternal(EmulatedStackFrame sf);
  //
  // This means we need only two vregs :
  // - One for the receiver object.
  // - One for the only method argument (an EmulatedStackFrame).
  static constexpr size_t kNumRegsForTransform = 2;

  const DexFile::CodeItem* code_item = called_method->GetCodeItem();
  DCHECK(code_item != nullptr);
  DCHECK_EQ(kNumRegsForTransform, code_item->registers_size_);
  DCHECK_EQ(kNumRegsForTransform, code_item->ins_size_);

  ShadowFrameAllocaUniquePtr shadow_frame_unique_ptr =
      CREATE_SHADOW_FRAME(kNumRegsForTransform, &shadow_frame, called_method, /* dex pc */ 0);
  ShadowFrame* new_shadow_frame = shadow_frame_unique_ptr.get();

  StackHandleScope<1> hs(self);
  MutableHandle<mirror::EmulatedStackFrame> sf(hs.NewHandle<mirror::EmulatedStackFrame>(nullptr));
  if (IsCallerTransformer(callsite_type)) {
    // If we're entering this transformer from another transformer, we can pass
    // through the handle directly to the callee, instead of having to
    // instantiate a new stack frame based on the shadow frame.
    size_t first_callee_register = is_range ? first_arg : args[0];
    sf.Assign(reinterpret_cast<mirror::EmulatedStackFrame*>(
        shadow_frame.GetVRegReference(first_callee_register)));
  } else {
    sf.Assign(mirror::EmulatedStackFrame::CreateFromShadowFrameAndArgs<is_range>(self,
                                                                                 callsite_type,
                                                                                 callee_type,
                                                                                 shadow_frame,
                                                                                 first_arg,
                                                                                 args));

    // Something went wrong while creating the emulated stack frame, we should
    // throw the pending exception.
    if (sf.Get() == nullptr) {
      DCHECK(self->IsExceptionPending());
      return false;
    }
  }

  new_shadow_frame->SetVRegReference(0, receiver.Get());
  new_shadow_frame->SetVRegReference(1, sf.Get());

  PerformCall(self,
              code_item,
              shadow_frame.GetMethod(),
              0 /* first destination register */,
              new_shadow_frame,
              result);
  if (self->IsExceptionPending()) {
    return false;
  }

  // If the called transformer method we called has returned a value, then we
  // need to copy it back to |result|.
  sf->GetReturnValue(self, result);
  return ConvertReturnValue(callsite_type, callee_type, result);
}

inline static ObjPtr<mirror::Class> GetAndInitializeDeclaringClass(Thread* self, ArtField* field)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  // Method handle invocations on static fields should ensure class is
  // initialized. This usually happens when an instance is constructed
  // or class members referenced, but this is not guaranteed when
  // looking up method handles.
  ObjPtr<mirror::Class> klass = field->GetDeclaringClass();
  if (UNLIKELY(!klass->IsInitialized())) {
    StackHandleScope<1> hs(self);
    HandleWrapperObjPtr<mirror::Class> h(hs.NewHandleWrapper(&klass));
    if (!Runtime::Current()->GetClassLinker()->EnsureInitialized(self, h, true, true)) {
      DCHECK(self->IsExceptionPending());
      return nullptr;
    }
  }
  return klass;
}

template <bool is_range>
bool DoInvokePolymorphicUnchecked(Thread* self,
                                  ShadowFrame& shadow_frame,
                                  Handle<mirror::MethodHandleImpl> method_handle,
                                  Handle<mirror::MethodType> callsite_type,
                                  const uint32_t (&args)[Instruction::kMaxVarArgRegs],
                                  uint32_t first_arg,
                                  JValue* result)
  REQUIRES_SHARED(Locks::mutator_lock_) {
  StackHandleScope<1> hs(self);
  Handle<mirror::MethodType> handle_type(hs.NewHandle(method_handle->GetMethodType()));
  const mirror::MethodHandle::Kind handle_kind = method_handle->GetHandleKind();
  if (IsInvoke(handle_kind)) {
    // Get the method we're actually invoking along with the kind of
    // invoke that is desired. We don't need to perform access checks at this
    // point because they would have been performed on our behalf at the point
    // of creation of the method handle.
    ArtMethod* called_method = method_handle->GetTargetMethod();
    CHECK(called_method != nullptr);

    if (handle_kind == mirror::MethodHandle::Kind::kInvokeVirtual ||
        handle_kind == mirror::MethodHandle::Kind::kInvokeInterface) {
      // TODO: Unfortunately, we have to postpone dynamic receiver based checks
      // because the receiver might be cast or might come from an emulated stack
      // frame, which means that it is unknown at this point. We perform these
      // checks inside DoCallPolymorphic right before we do the actual invoke.
    } else if (handle_kind == mirror::MethodHandle::Kind::kInvokeDirect) {
      // String constructors are a special case, they are replaced with StringFactory
      // methods.
      if (called_method->IsConstructor() && called_method->GetDeclaringClass()->IsStringClass()) {
        DCHECK(handle_type->GetRType()->IsStringClass());
        called_method = WellKnownClasses::StringInitToStringFactory(called_method);
      }
    } else if (handle_kind == mirror::MethodHandle::Kind::kInvokeSuper) {
      ObjPtr<mirror::Class> declaring_class = called_method->GetDeclaringClass();

      // Note that we're not dynamically dispatching on the type of the receiver
      // here. We use the static type of the "receiver" object that we've
      // recorded in the method handle's type, which will be the same as the
      // special caller that was specified at the point of lookup.
      ObjPtr<mirror::Class> referrer_class = handle_type->GetPTypes()->Get(0);
      if (!declaring_class->IsInterface()) {
        ObjPtr<mirror::Class> super_class = referrer_class->GetSuperClass();
        uint16_t vtable_index = called_method->GetMethodIndex();
        DCHECK(super_class != nullptr);
        DCHECK(super_class->HasVTable());
        // Note that super_class is a super of referrer_class and called_method
        // will always be declared by super_class (or one of its super classes).
        DCHECK_LT(vtable_index, super_class->GetVTableLength());
        called_method = super_class->GetVTableEntry(vtable_index, kRuntimePointerSize);
      } else {
        called_method = referrer_class->FindVirtualMethodForInterfaceSuper(
            called_method, kRuntimePointerSize);
      }
      CHECK(called_method != nullptr);
    }
    if (IsInvokeTransform(handle_kind)) {
      // There are two cases here - method handles representing regular
      // transforms and those representing call site transforms. Method
      // handles for call site transforms adapt their MethodType to match
      // the call site. For these, the |callee_type| is the same as the
      // |callsite_type|. The VarargsCollector is such a tranform, its
      // method type depends on the call site, ie. x(a) or x(a, b), or
      // x(a, b, c). The VarargsCollector invokes a variable arity method
      // with the arity arguments in an array.
      Handle<mirror::MethodType> callee_type =
          (handle_kind == mirror::MethodHandle::Kind::kInvokeCallSiteTransform) ? callsite_type
          : handle_type;
      return DoCallTransform<is_range>(called_method,
                                       callsite_type,
                                       callee_type,
                                       self,
                                       shadow_frame,
                                       method_handle /* receiver */,
                                       args,
                                       first_arg,
                                       result);

    } else {
      return DoCallPolymorphic<is_range>(called_method,
                                         callsite_type,
                                         handle_type,
                                         self,
                                         shadow_frame,
                                         args,
                                         first_arg,
                                         result,
                                         handle_kind);
    }
  } else {
    LOG(FATAL) << "Unreachable: " << handle_kind;
    UNREACHABLE();
  }
}

// Helper for getters in invoke-polymorphic.
inline static void DoFieldGetForInvokePolymorphic(Thread* self,
                                                  const ShadowFrame& shadow_frame,
                                                  ObjPtr<mirror::Object>& obj,
                                                  ArtField* field,
                                                  Primitive::Type field_type,
                                                  JValue* result)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  switch (field_type) {
    case Primitive::kPrimBoolean:
      DoFieldGetCommon<Primitive::kPrimBoolean>(self, shadow_frame, obj, field, result);
      break;
    case Primitive::kPrimByte:
      DoFieldGetCommon<Primitive::kPrimByte>(self, shadow_frame, obj, field, result);
      break;
    case Primitive::kPrimChar:
      DoFieldGetCommon<Primitive::kPrimChar>(self, shadow_frame, obj, field, result);
      break;
    case Primitive::kPrimShort:
      DoFieldGetCommon<Primitive::kPrimShort>(self, shadow_frame, obj, field, result);
      break;
    case Primitive::kPrimInt:
      DoFieldGetCommon<Primitive::kPrimInt>(self, shadow_frame, obj, field, result);
      break;
    case Primitive::kPrimLong:
      DoFieldGetCommon<Primitive::kPrimLong>(self, shadow_frame, obj, field, result);
      break;
    case Primitive::kPrimFloat:
      DoFieldGetCommon<Primitive::kPrimInt>(self, shadow_frame, obj, field, result);
      break;
    case Primitive::kPrimDouble:
      DoFieldGetCommon<Primitive::kPrimLong>(self, shadow_frame, obj, field, result);
      break;
    case Primitive::kPrimNot:
      DoFieldGetCommon<Primitive::kPrimNot>(self, shadow_frame, obj, field, result);
      break;
    case Primitive::kPrimVoid:
      LOG(FATAL) << "Unreachable: " << field_type;
      UNREACHABLE();
  }
}

// Helper for setters in invoke-polymorphic.
template <bool do_assignability_check>
inline bool DoFieldPutForInvokePolymorphic(Thread* self,
                                           ShadowFrame& shadow_frame,
                                           ObjPtr<mirror::Object>& obj,
                                           ArtField* field,
                                           Primitive::Type field_type,
                                           const JValue& value)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  static const bool kTransaction = false;
  switch (field_type) {
    case Primitive::kPrimBoolean:
      return DoFieldPutCommon<Primitive::kPrimBoolean, do_assignability_check, kTransaction>(
          self, shadow_frame, obj, field, value);
    case Primitive::kPrimByte:
      return DoFieldPutCommon<Primitive::kPrimByte, do_assignability_check, kTransaction>(
          self, shadow_frame, obj, field, value);
    case Primitive::kPrimChar:
      return DoFieldPutCommon<Primitive::kPrimChar, do_assignability_check, kTransaction>(
          self, shadow_frame, obj, field, value);
    case Primitive::kPrimShort:
      return DoFieldPutCommon<Primitive::kPrimShort, do_assignability_check, kTransaction>(
          self, shadow_frame, obj, field, value);
    case Primitive::kPrimInt:
    case Primitive::kPrimFloat:
      return DoFieldPutCommon<Primitive::kPrimInt, do_assignability_check, kTransaction>(
          self, shadow_frame, obj, field, value);
    case Primitive::kPrimLong:
    case Primitive::kPrimDouble:
      return DoFieldPutCommon<Primitive::kPrimLong, do_assignability_check, kTransaction>(
          self, shadow_frame, obj, field, value);
    case Primitive::kPrimNot:
      return DoFieldPutCommon<Primitive::kPrimNot, do_assignability_check, kTransaction>(
          self, shadow_frame, obj, field, value);
    case Primitive::kPrimVoid:
      LOG(FATAL) << "Unreachable: " << field_type;
      UNREACHABLE();
  }
}

static JValue GetValueFromShadowFrame(const ShadowFrame& shadow_frame,
                                      Primitive::Type field_type,
                                      uint32_t vreg)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  JValue field_value;
  switch (field_type) {
    case Primitive::kPrimBoolean:
      field_value.SetZ(static_cast<uint8_t>(shadow_frame.GetVReg(vreg)));
      break;
    case Primitive::kPrimByte:
      field_value.SetB(static_cast<int8_t>(shadow_frame.GetVReg(vreg)));
      break;
    case Primitive::kPrimChar:
      field_value.SetC(static_cast<uint16_t>(shadow_frame.GetVReg(vreg)));
      break;
    case Primitive::kPrimShort:
      field_value.SetS(static_cast<int16_t>(shadow_frame.GetVReg(vreg)));
      break;
    case Primitive::kPrimInt:
    case Primitive::kPrimFloat:
      field_value.SetI(shadow_frame.GetVReg(vreg));
      break;
    case Primitive::kPrimLong:
    case Primitive::kPrimDouble:
      field_value.SetJ(shadow_frame.GetVRegLong(vreg));
      break;
    case Primitive::kPrimNot:
      field_value.SetL(shadow_frame.GetVRegReference(vreg));
      break;
    case Primitive::kPrimVoid:
      LOG(FATAL) << "Unreachable: " << field_type;
      UNREACHABLE();
  }
  return field_value;
}

template <bool is_range, bool do_conversions, bool do_assignability_check>
bool DoInvokePolymorphicFieldAccess(Thread* self,
                                    ShadowFrame& shadow_frame,
                                    Handle<mirror::MethodHandleImpl> method_handle,
                                    Handle<mirror::MethodType> callsite_type,
                                    const uint32_t (&args)[Instruction::kMaxVarArgRegs],
                                    uint32_t first_arg,
                                    JValue* result)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  StackHandleScope<1> hs(self);
  Handle<mirror::MethodType> handle_type(hs.NewHandle(method_handle->GetMethodType()));
  const mirror::MethodHandle::Kind handle_kind = method_handle->GetHandleKind();
  ArtField* field = method_handle->GetTargetField();
  Primitive::Type field_type = field->GetTypeAsPrimitiveType();

  switch (handle_kind) {
    case mirror::MethodHandle::kInstanceGet: {
      size_t obj_reg = is_range ? first_arg : args[0];
      ObjPtr<mirror::Object> obj = shadow_frame.GetVRegReference(obj_reg);
      DoFieldGetForInvokePolymorphic(self, shadow_frame, obj, field, field_type, result);
      if (do_conversions && !ConvertReturnValue(callsite_type, handle_type, result)) {
        DCHECK(self->IsExceptionPending());
        return false;
      }
      return true;
    }
    case mirror::MethodHandle::kStaticGet: {
      ObjPtr<mirror::Object> obj = GetAndInitializeDeclaringClass(self, field);
      if (obj == nullptr) {
        DCHECK(self->IsExceptionPending());
        return false;
      }
      DoFieldGetForInvokePolymorphic(self, shadow_frame, obj, field, field_type, result);
      if (do_conversions && !ConvertReturnValue(callsite_type, handle_type, result)) {
        DCHECK(self->IsExceptionPending());
        return false;
      }
      return true;
    }
    case mirror::MethodHandle::kInstancePut: {
      size_t obj_reg = is_range ? first_arg : args[0];
      size_t value_reg = is_range ? (first_arg + 1) : args[1];
      JValue value = GetValueFromShadowFrame(shadow_frame, field_type, value_reg);
      if (do_conversions && !ConvertArgumentValue(callsite_type, handle_type, 1, &value)) {
        DCHECK(self->IsExceptionPending());
        return false;
      }
      ObjPtr<mirror::Object> obj = shadow_frame.GetVRegReference(obj_reg);
      return DoFieldPutForInvokePolymorphic<do_assignability_check>(self,
                                                                    shadow_frame,
                                                                    obj,
                                                                    field,
                                                                    field_type,
                                                                    value);
    }
    case mirror::MethodHandle::kStaticPut: {
      ObjPtr<mirror::Object> obj = GetAndInitializeDeclaringClass(self, field);
      if (obj == nullptr) {
        DCHECK(self->IsExceptionPending());
        return false;
      }
      size_t value_reg = is_range ? first_arg : args[0];
      JValue value = GetValueFromShadowFrame(shadow_frame, field_type, value_reg);
      if (do_conversions && !ConvertArgumentValue(callsite_type, handle_type, 0, &value)) {
        DCHECK(self->IsExceptionPending());
        return false;
      }
      return DoFieldPutForInvokePolymorphic<do_assignability_check>(self,
                                                                    shadow_frame,
                                                                    obj,
                                                                    field,
                                                                    field_type,
                                                                    value);
    }
    default:
      LOG(FATAL) << "Unreachable: " << handle_kind;
      UNREACHABLE();
  }
}

template <bool is_range, bool do_assignability_check>
static inline bool DoInvokePolymorphicNonExact(Thread* self,
                                               ShadowFrame& shadow_frame,
                                               Handle<mirror::MethodHandleImpl> method_handle,
                                               Handle<mirror::MethodType> callsite_type,
                                               const uint32_t (&args)[Instruction::kMaxVarArgRegs],
                                               uint32_t first_arg,
                                               JValue* result)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  const mirror::MethodHandle::Kind handle_kind = method_handle->GetHandleKind();
  ObjPtr<mirror::MethodType> handle_type(method_handle->GetMethodType());
  CHECK(handle_type != nullptr);

  if (!IsInvokeTransform(handle_kind)) {
    if (UNLIKELY(!IsCallerTransformer(callsite_type) &&
                 !callsite_type->IsConvertible(handle_type.Ptr()))) {
      ThrowWrongMethodTypeException(handle_type.Ptr(), callsite_type.Get());
      return false;
    }
  }

  if (IsFieldAccess(handle_kind)) {
    if (UNLIKELY(callsite_type->IsExactMatch(handle_type.Ptr()))) {
      const bool do_convert = false;
      return DoInvokePolymorphicFieldAccess<is_range, do_convert, do_assignability_check>(
          self,
          shadow_frame,
          method_handle,
          callsite_type,
          args,
          first_arg,
          result);
    } else {
      const bool do_convert = true;
      return DoInvokePolymorphicFieldAccess<is_range, do_convert, do_assignability_check>(
          self,
          shadow_frame,
          method_handle,
          callsite_type,
          args,
          first_arg,
          result);
    }
  }

  if (UNLIKELY(callsite_type->IsExactMatch(handle_type.Ptr()))) {
    return DoInvokePolymorphicUnchecked<is_range>(self,
                                                  shadow_frame,
                                                  method_handle,
                                                  callsite_type,
                                                  args,
                                                  first_arg,
                                                  result);
  } else {
    return DoInvokePolymorphicUnchecked<is_range>(self,
                                                  shadow_frame,
                                                  method_handle,
                                                  callsite_type,
                                                  args,
                                                  first_arg,
                                                  result);
  }
}

template <bool is_range, bool do_assignability_check>
bool DoInvokePolymorphicExact(Thread* self,
                              ShadowFrame& shadow_frame,
                              Handle<mirror::MethodHandleImpl> method_handle,
                              Handle<mirror::MethodType> callsite_type,
                              const uint32_t (&args)[Instruction::kMaxVarArgRegs],
                              uint32_t first_arg,
                              JValue* result)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  // We need to check the nominal type of the handle in addition to the
  // real type. The "nominal" type is present when MethodHandle.asType is
  // called any handle, and results in the declared type of the handle
  // changing.
  ObjPtr<mirror::MethodType> nominal_type(method_handle->GetNominalType());
  if (UNLIKELY(nominal_type != nullptr)) {
    if (UNLIKELY(!callsite_type->IsExactMatch(nominal_type.Ptr()))) {
      ThrowWrongMethodTypeException(nominal_type.Ptr(), callsite_type.Get());
      return false;
    }
    return DoInvokePolymorphicNonExact<is_range, do_assignability_check>(self,
                                                                         shadow_frame,
                                                                         method_handle,
                                                                         callsite_type,
                                                                         args,
                                                                         first_arg,
                                                                         result);
  }

  ObjPtr<mirror::MethodType> handle_type(method_handle->GetMethodType());
  if (UNLIKELY(!callsite_type->IsExactMatch(handle_type.Ptr()))) {
    ThrowWrongMethodTypeException(handle_type.Ptr(), callsite_type.Get());
    return false;
  }

  const mirror::MethodHandle::Kind handle_kind = method_handle->GetHandleKind();
  if (IsFieldAccess(handle_kind)) {
    const bool do_convert = false;
    return DoInvokePolymorphicFieldAccess<is_range, do_convert, do_assignability_check>(
        self,
        shadow_frame,
        method_handle,
        callsite_type,
        args,
        first_arg,
        result);
  }

  return DoInvokePolymorphicUnchecked<is_range>(self,
                                                shadow_frame,
                                                method_handle,
                                                callsite_type,
                                                args,
                                                first_arg,
                                                result);
}

}  // namespace

template <bool is_range, bool do_assignability_check>
bool DoInvokePolymorphic(Thread* self,
                         ArtMethod* invoke_method,
                         ShadowFrame& shadow_frame,
                         Handle<mirror::MethodHandleImpl> method_handle,
                         Handle<mirror::MethodType> callsite_type,
                         const uint32_t (&args)[Instruction::kMaxVarArgRegs],
                         uint32_t first_arg,
                         JValue* result)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  if (IsMethodHandleInvokeExact(invoke_method)) {
    return DoInvokePolymorphicExact<is_range, do_assignability_check>(self,
                                                                      shadow_frame,
                                                                      method_handle,
                                                                      callsite_type,
                                                                      args,
                                                                      first_arg,
                                                                      result);
  } else {
    return DoInvokePolymorphicNonExact<is_range, do_assignability_check>(self,
                                                                         shadow_frame,
                                                                         method_handle,
                                                                         callsite_type,
                                                                         args,
                                                                         first_arg,
                                                                         result);
  }
}

#define EXPLICIT_DO_INVOKE_POLYMORPHIC_TEMPLATE_DECL(_is_range, _do_assignability_check) \
template REQUIRES_SHARED(Locks::mutator_lock_)                                           \
bool DoInvokePolymorphic<_is_range, _do_assignability_check>(                            \
    Thread* self,                                                                        \
    ArtMethod* invoke_method,                                                            \
    ShadowFrame& shadow_frame,                                                           \
    Handle<mirror::MethodHandleImpl> method_handle,                                      \
    Handle<mirror::MethodType> callsite_type,                                            \
    const uint32_t (&args)[Instruction::kMaxVarArgRegs],                                 \
    uint32_t first_arg,                                                                  \
    JValue* result)

EXPLICIT_DO_INVOKE_POLYMORPHIC_TEMPLATE_DECL(true, true);
EXPLICIT_DO_INVOKE_POLYMORPHIC_TEMPLATE_DECL(true, false);
EXPLICIT_DO_INVOKE_POLYMORPHIC_TEMPLATE_DECL(false, true);
EXPLICIT_DO_INVOKE_POLYMORPHIC_TEMPLATE_DECL(false, false);
#undef EXPLICIT_DO_INVOKE_POLYMORPHIC_TEMPLATE_DECL

}  // namespace art
