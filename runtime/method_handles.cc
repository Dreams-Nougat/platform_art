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

#include "method_handles.h"

#include "method_handles-inl.h"
#include "jvalue.h"
#include "jvalue-inl.h"
#include "reflection.h"
#include "reflection-inl.h"

namespace art {

namespace {

static const char* kBoxedBooleanClass = "Ljava/lang/Boolean;";
static const char* kBoxedByteClass = "Ljava/lang/Byte;";
static const char* kBoxedCharacterClass = "Ljava/lang/Character;";
static const char* kBoxedDoubleClass = "Ljava/lang/Double;";
static const char* kBoxedFloatClass = "Ljava/lang/Float;";
static const char* kBoxedIntegerClass = "Ljava/lang/Integer;";
static const char* kBoxedLongClass = "Ljava/lang/Long;";
static const char* kBoxedShortClass = "Ljava/lang/Short;";

// Assigns |type| to the primitive type associated with |klass|. Returns
// true iff. |klass| was a boxed type (Integer, Long etc.), false otherwise.
bool GetUnboxedPrimitiveType(ObjPtr<mirror::Class> klass, Primitive::Type* type)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ScopedAssertNoThreadSuspension ants(__FUNCTION__);
  if (klass->DescriptorEquals(kBoxedBooleanClass)) {
    (*type) = Primitive::kPrimBoolean;
    return true;
  } else if (klass->DescriptorEquals(kBoxedByteClass)) {
    (*type) = Primitive::kPrimByte;
    return true;
  } else if (klass->DescriptorEquals(kBoxedCharacterClass)) {
    (*type) = Primitive::kPrimChar;
    return true;
  } else if (klass->DescriptorEquals(kBoxedFloatClass)) {
    (*type) = Primitive::kPrimFloat;
    return true;
  } else if (klass->DescriptorEquals(kBoxedDoubleClass)) {
    (*type) = Primitive::kPrimDouble;
    return true;
  } else if (klass->DescriptorEquals(kBoxedIntegerClass)) {
    (*type) = Primitive::kPrimInt;
    return true;
  } else if (klass->DescriptorEquals(kBoxedLongClass)) {
    (*type) = Primitive::kPrimLong;
    return true;
  } else if (klass->DescriptorEquals(kBoxedShortClass)) {
    (*type) = Primitive::kPrimShort;
    return true;
  } else {
    return false;
  }
}

// Returns the class corresponding to the boxed type for the primitive |type|.
ObjPtr<mirror::Class> GetBoxedPrimitiveClass(Primitive::Type type)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ScopedAssertNoThreadSuspension ants(__FUNCTION__);
  ClassLinker* const class_linker = Runtime::Current()->GetClassLinker();
  switch (type) {
    case Primitive::kPrimBoolean:
      return class_linker->FindSystemClass(Thread::Current(), kBoxedBooleanClass);
    case Primitive::kPrimByte:
      return class_linker->FindSystemClass(Thread::Current(), kBoxedByteClass);
    case Primitive::kPrimChar:
      return class_linker->FindSystemClass(Thread::Current(), kBoxedCharacterClass);
    case Primitive::kPrimShort:
      return class_linker->FindSystemClass(Thread::Current(), kBoxedShortClass);
    case Primitive::kPrimInt:
      return class_linker->FindSystemClass(Thread::Current(), kBoxedIntegerClass);
    case Primitive::kPrimLong:
      return class_linker->FindSystemClass(Thread::Current(), kBoxedLongClass);
    case Primitive::kPrimFloat:
      return class_linker->FindSystemClass(Thread::Current(), kBoxedFloatClass);
    case Primitive::kPrimDouble:
      return class_linker->FindSystemClass(Thread::Current(), kBoxedDoubleClass);
    case Primitive::kPrimNot:
    case Primitive::kPrimVoid:
      LOG(FATAL) << "Unreachable";
      return nullptr;
  }
}

// Returns true if |klass| is a boxed primitive type or a sub-class of a boxed primitive type.
bool IsSubClassOfBoxedPrimitive(ObjPtr<mirror::Class> klass) REQUIRES_SHARED(Locks::mutator_lock_) {
  ScopedAssertNoThreadSuspension ants(__FUNCTION__);
  do {
    Primitive::Type type;
    if (GetUnboxedPrimitiveType(klass, &type)) {
      return true;
    }
    klass = klass->GetSuperClass();
  } while (klass != nullptr);
  return false;
}

bool UnboxPrimitiveForMethodHandles(ObjPtr<mirror::Object> o,
                                    ObjPtr<mirror::Class> dst_class,
                                    JValue* unboxed_value)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  // This is derived from UnboxPrimitive() in reflection.cc, but with
  // exceptions appropriate to method handles.
  if (!dst_class->IsPrimitive()) {
    if (UNLIKELY(o != nullptr && !o->InstanceOf(dst_class))) {
      ThrowClassCastException(o->GetClass(), dst_class);
      return false;
    }
    unboxed_value->SetL(o);
    return true;
  }
  if (UNLIKELY(dst_class->GetPrimitiveType() == Primitive::kPrimVoid)) {
    ThrowClassCastException(o->GetClass(), dst_class);
    return false;
  }
  if (UNLIKELY(o == nullptr)) {
    ThrowNullPointerException(
        StringPrintf("Expected to unbox a '%s' primitive type but was returned null",
                     dst_class->PrettyDescriptor().c_str()).c_str());
    return false;
  }

  JValue boxed_value;
  ObjPtr<mirror::Class> klass = o->GetClass();
  ObjPtr<mirror::Class> src_class = nullptr;
  ClassLinker* const class_linker = Runtime::Current()->GetClassLinker();
  ArtField* primitive_field = &klass->GetIFieldsPtr()->At(0);
  if (klass->DescriptorEquals(kBoxedBooleanClass)) {
    src_class = class_linker->FindPrimitiveClass('Z');
    boxed_value.SetZ(primitive_field->GetBoolean(o));
  } else if (klass->DescriptorEquals(kBoxedByteClass)) {
    src_class = class_linker->FindPrimitiveClass('B');
    boxed_value.SetB(primitive_field->GetByte(o));
  } else if (klass->DescriptorEquals(kBoxedCharacterClass)) {
    src_class = class_linker->FindPrimitiveClass('C');
    boxed_value.SetC(primitive_field->GetChar(o));
  } else if (klass->DescriptorEquals(kBoxedFloatClass)) {
    src_class = class_linker->FindPrimitiveClass('F');
    boxed_value.SetF(primitive_field->GetFloat(o));
  } else if (klass->DescriptorEquals(kBoxedDoubleClass)) {
    src_class = class_linker->FindPrimitiveClass('D');
    boxed_value.SetD(primitive_field->GetDouble(o));
  } else if (klass->DescriptorEquals(kBoxedIntegerClass)) {
    src_class = class_linker->FindPrimitiveClass('I');
    boxed_value.SetI(primitive_field->GetInt(o));
  } else if (klass->DescriptorEquals(kBoxedLongClass)) {
    src_class = class_linker->FindPrimitiveClass('J');
    boxed_value.SetJ(primitive_field->GetLong(o));
  } else if (klass->DescriptorEquals(kBoxedShortClass)) {
    src_class = class_linker->FindPrimitiveClass('S');
    boxed_value.SetS(primitive_field->GetShort(o));
  } else {
    std::string temp;
    ThrowIllegalArgumentException(
        StringPrintf("result has type %s, got %s",
                     dst_class->PrettyDescriptor().c_str(),
                     PrettyDescriptor(o->GetClass()->GetDescriptor(&temp)).c_str()).c_str());
    return false;
  }

  if (!ConvertPrimitiveValueNoThrow(src_class->GetPrimitiveType(),
                                    dst_class->GetPrimitiveType(),
                                    boxed_value,
                                    unboxed_value)) {
    ThrowClassCastException(src_class, dst_class);
    return false;
  }
  return true;
}

inline bool IsPrimitiveType(Primitive::Type type) {
  return type != Primitive::kPrimNot;
}

inline bool IsReferenceType(Primitive::Type type) {
  return type == Primitive::kPrimNot;
}

}  // namespace

bool ConvertJValueCommon(
    Handle<mirror::MethodType> callsite_type,
    Handle<mirror::MethodType> callee_type,
    Handle<mirror::Class> from,
    Handle<mirror::Class> to,
    JValue* value) {
  // The reader maybe concerned about the safety of the heap object
  // that may be in |value|. There is only one case where allocation
  // is obviously needed and that's for boxing. However, in the case
  // of boxing |value| contains a non-reference type.

  const Primitive::Type from_type = from->GetPrimitiveType();
  const Primitive::Type to_type = to->GetPrimitiveType();

  // This method must be called only when the types don't match.
  DCHECK(from.Get() != to.Get());

  if (IsPrimitiveType(from_type) && IsPrimitiveType(to_type)) {
    // The source and target types are both primitives.
    if (UNLIKELY(!ConvertPrimitiveValueNoThrow(from_type, to_type, *value, value))) {
      ThrowWrongMethodTypeException(callee_type.Get(), callsite_type.Get());
      return false;
    }
    return true;
  } else if (IsReferenceType(from_type) && IsReferenceType(to_type)) {
    // They're both reference types. If "from" is null, we can pass it
    // through unchanged. If not, we must generate a cast exception if
    // |to| is not assignable from the dynamic type of |ref|.
    mirror::Object* const ref = value->GetL();
    if (ref == nullptr || to->IsAssignableFrom(ref->GetClass())) {
      value->SetL(ref);
      return true;
    } else {
      ThrowClassCastException(to.Get(), ref->GetClass());
      return false;
    }
  } else if (IsReferenceType(to_type)) {
    DCHECK(IsPrimitiveType(from_type));

    // The source type is a primitive and the target type is a reference, so we must box.
    Primitive::Type type;

    // The target type maybe a super class of the boxed source type, for example,
    // if the source type is int, it's boxed type is java.lang.Integer, and the target
    // type could be java.lang.Number.
    if (!GetUnboxedPrimitiveType(to.Get(), &type)) {
      ObjPtr<mirror::Class> boxed_from_class = GetBoxedPrimitiveClass(from_type);
      if (boxed_from_class->IsSubClass(ObjPtr<mirror::Class>(to.Get()))) {
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

    if (!ConvertPrimitiveValueNoThrow(from_type, type, *value, value)) {
      ThrowWrongMethodTypeException(callee_type.Get(), callsite_type.Get());
      return false;
    }

    // Then perform the actual boxing, and then set the reference.
    ObjPtr<mirror::Object> boxed = BoxPrimitive(type, *value);
    value->SetL(boxed.Ptr());
    return true;
  } else {
    // The source type is a reference and the target type is a primitive, so we must unbox.
    DCHECK(IsReferenceType(from_type));
    DCHECK(IsPrimitiveType(to_type));

    // Check source type is a boxed primitive or has a boxed primitive super-class.
    ObjPtr<mirror::Class> boxed_to_class = GetBoxedPrimitiveClass(to_type);
    if (!IsSubClassOfBoxedPrimitive(from.Get()) && !boxed_to_class->IsSubClass(from.Get())) {
      ThrowWrongMethodTypeException(callee_type.Get(), callsite_type.Get());
      return false;
    }

    ObjPtr<mirror::Object> ref(value->GetL());
    if (ref == nullptr) {
      ThrowNullPointerException(
        StringPrintf("Expected to unbox a '%s' but instance was null",
                     from->PrettyDescriptor().c_str()).c_str());
      return false;
    }

    return UnboxPrimitiveForMethodHandles(ref, to.Get(), value);
  }
}

}  // namespace art
