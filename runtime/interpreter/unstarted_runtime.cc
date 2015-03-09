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

#include "unstarted_runtime.h"

#include <cmath>

#include "base/logging.h"
#include "base/macros.h"
#include "class_linker.h"
#include "common_throws.h"
#include "entrypoints/entrypoint_utils-inl.h"
#include "handle_scope-inl.h"
#include "interpreter/interpreter_common.h"
#include "mirror/array-inl.h"
#include "mirror/art_method-inl.h"
#include "mirror/class.h"
#include "mirror/object-inl.h"
#include "mirror/object_array-inl.h"
#include "mirror/string-inl.h"
#include "nth_caller_visitor.h"
#include "thread.h"
#include "well_known_classes.h"

namespace art {
namespace interpreter {

// Helper function to deal with class loading in an unstarted runtime.
static void UnstartedRuntimeFindClass(Thread* self, Handle<mirror::String> className,
                                      Handle<mirror::ClassLoader> class_loader, JValue* result,
                                      const std::string& method_name, bool initialize_class,
                                      bool abort_if_not_found)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  CHECK(className.Get() != nullptr);
  std::string descriptor(DotToDescriptor(className->ToModifiedUtf8().c_str()));
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();

  mirror::Class* found = class_linker->FindClass(self, descriptor.c_str(), class_loader);
  if (found == nullptr && abort_if_not_found) {
    if (!self->IsExceptionPending()) {
      AbortTransaction(self, "%s failed in un-started runtime for class: %s",
                       method_name.c_str(), PrettyDescriptor(descriptor.c_str()).c_str());
    }
    return;
  }
  if (found != nullptr && initialize_class) {
    StackHandleScope<1> hs(self);
    Handle<mirror::Class> h_class(hs.NewHandle(found));
    if (!class_linker->EnsureInitialized(self, h_class, true, true)) {
      CHECK(self->IsExceptionPending());
      return;
    }
  }
  result->SetL(found);
}

// Common helper for class-loading cutouts in an unstarted runtime. We call Runtime methods that
// rely on Java code to wrap errors in the correct exception class (i.e., NoClassDefFoundError into
// ClassNotFoundException), so need to do the same. The only exception is if the exception is
// actually InternalError. This must not be wrapped, as it signals an initialization abort.
static void CheckExceptionGenerateClassNotFound(Thread* self)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  if (self->IsExceptionPending()) {
    // If it is not an InternalError, wrap it.
    std::string type(PrettyTypeOf(self->GetException()));
    if (type != "java.lang.InternalError") {
      self->ThrowNewWrappedException(self->GetCurrentLocationForThrow(),
                                     "Ljava/lang/ClassNotFoundException;",
                                     "ClassNotFoundException");
    }
  }
}

static void UnstartedClassForName(Thread* self, ShadowFrame* shadow_frame, JValue* result,
                                  size_t arg_offset)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  mirror::String* class_name = shadow_frame->GetVRegReference(arg_offset)->AsString();
  StackHandleScope<1> hs(self);
  Handle<mirror::String> h_class_name(hs.NewHandle(class_name));
  UnstartedRuntimeFindClass(self, h_class_name, NullHandle<mirror::ClassLoader>(), result,
                            "Class.forName", true, false);
  CheckExceptionGenerateClassNotFound(self);
}

static void UnstartedClassForNameLong(Thread* self, ShadowFrame* shadow_frame, JValue* result,
                                      size_t arg_offset)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  mirror::String* class_name = shadow_frame->GetVRegReference(arg_offset)->AsString();
  bool initialize_class = shadow_frame->GetVReg(arg_offset + 1) != 0;
  mirror::ClassLoader* class_loader =
      down_cast<mirror::ClassLoader*>(shadow_frame->GetVRegReference(arg_offset + 2));
  StackHandleScope<2> hs(self);
  Handle<mirror::String> h_class_name(hs.NewHandle(class_name));
  Handle<mirror::ClassLoader> h_class_loader(hs.NewHandle(class_loader));
  UnstartedRuntimeFindClass(self, h_class_name, h_class_loader, result, "Class.forName",
                            initialize_class, false);
  CheckExceptionGenerateClassNotFound(self);
}

static void UnstartedClassClassForName(Thread* self, ShadowFrame* shadow_frame, JValue* result,
                                       size_t arg_offset)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  mirror::String* class_name = shadow_frame->GetVRegReference(arg_offset)->AsString();
  bool initialize_class = shadow_frame->GetVReg(arg_offset + 1) != 0;
  mirror::ClassLoader* class_loader =
      down_cast<mirror::ClassLoader*>(shadow_frame->GetVRegReference(arg_offset + 2));
  StackHandleScope<2> hs(self);
  Handle<mirror::String> h_class_name(hs.NewHandle(class_name));
  Handle<mirror::ClassLoader> h_class_loader(hs.NewHandle(class_loader));
  UnstartedRuntimeFindClass(self, h_class_name, h_class_loader, result, "Class.classForName",
                            initialize_class, false);
  CheckExceptionGenerateClassNotFound(self);
}

static void UnstartedClassNewInstance(Thread* self, ShadowFrame* shadow_frame, JValue* result,
                                      size_t arg_offset)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  StackHandleScope<3> hs(self);  // Class, constructor, object.
  mirror::Class* klass = shadow_frame->GetVRegReference(arg_offset)->AsClass();
  Handle<mirror::Class> h_klass(hs.NewHandle(klass));
  // There are two situations in which we'll abort this run.
  //  1) If the class isn't yet initialized and initialization fails.
  //  2) If we can't find the default constructor. We'll postpone the exception to runtime.
  // Note that 2) could likely be handled here, but for safety abort the transaction.
  bool ok = false;
  if (Runtime::Current()->GetClassLinker()->EnsureInitialized(self, h_klass, true, true)) {
    Handle<mirror::ArtMethod> h_cons(hs.NewHandle(
        h_klass->FindDeclaredDirectMethod("<init>", "()V")));
    if (h_cons.Get() != nullptr) {
      Handle<mirror::Object> h_obj(hs.NewHandle(klass->AllocObject(self)));
      CHECK(h_obj.Get() != nullptr);  // We don't expect OOM at compile-time.
      EnterInterpreterFromInvoke(self, h_cons.Get(), h_obj.Get(), nullptr, nullptr);
      if (!self->IsExceptionPending()) {
        result->SetL(h_obj.Get());
        ok = true;
      }
    } else {
      self->ThrowNewExceptionF(self->GetCurrentLocationForThrow(), "Ljava/lang/InternalError;",
                               "Could not find default constructor for '%s'",
                               PrettyClass(h_klass.Get()).c_str());
    }
  }
  if (!ok) {
    std::string error_msg = StringPrintf("Failed in Class.newInstance for '%s' with %s",
                                         PrettyClass(h_klass.Get()).c_str(),
                                         PrettyTypeOf(self->GetException()).c_str());
    Runtime::Current()->AbortTransactionAndThrowInternalError(self, error_msg);
  }
}

static void UnstartedClassGetDeclaredField(Thread* self, ShadowFrame* shadow_frame, JValue* result,
                                           size_t arg_offset)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  // Special managed code cut-out to allow field lookup in a un-started runtime that'd fail
  // going the reflective Dex way.
  mirror::Class* klass = shadow_frame->GetVRegReference(arg_offset)->AsClass();
  mirror::String* name2 = shadow_frame->GetVRegReference(arg_offset + 1)->AsString();
  mirror::ArtField* found = nullptr;
  mirror::ObjectArray<mirror::ArtField>* fields = klass->GetIFields();
  for (int32_t i = 0; i < fields->GetLength() && found == nullptr; ++i) {
    mirror::ArtField* f = fields->Get(i);
    if (name2->Equals(f->GetName())) {
      found = f;
    }
  }
  if (found == nullptr) {
    fields = klass->GetSFields();
    for (int32_t i = 0; i < fields->GetLength() && found == nullptr; ++i) {
      mirror::ArtField* f = fields->Get(i);
      if (name2->Equals(f->GetName())) {
        found = f;
      }
    }
  }
  CHECK(found != nullptr)
  << "Failed to find field in Class.getDeclaredField in un-started runtime. name="
  << name2->ToModifiedUtf8() << " class=" << PrettyDescriptor(klass);
  // TODO: getDeclaredField calls GetType once the field is found to ensure a
  //       NoClassDefFoundError is thrown if the field's type cannot be resolved.
  mirror::Class* jlr_Field = self->DecodeJObject(
      WellKnownClasses::java_lang_reflect_Field)->AsClass();
  StackHandleScope<1> hs(self);
  Handle<mirror::Object> field(hs.NewHandle(jlr_Field->AllocNonMovableObject(self)));
  CHECK(field.Get() != nullptr);
  mirror::ArtMethod* c = jlr_Field->FindDeclaredDirectMethod("<init>",
                                                             "(Ljava/lang/reflect/ArtField;)V");
  uint32_t args[1];
  args[0] = StackReference<mirror::Object>::FromMirrorPtr(found).AsVRegValue();
  EnterInterpreterFromInvoke(self, c, field.Get(), args, nullptr);
  result->SetL(field.Get());
}

static void UnstartedVmClassLoaderFindLoadedClass(Thread* self, ShadowFrame* shadow_frame,
                                                  JValue* result, size_t arg_offset)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  mirror::String* class_name = shadow_frame->GetVRegReference(arg_offset + 1)->AsString();
  mirror::ClassLoader* class_loader =
      down_cast<mirror::ClassLoader*>(shadow_frame->GetVRegReference(arg_offset));
  StackHandleScope<2> hs(self);
  Handle<mirror::String> h_class_name(hs.NewHandle(class_name));
  Handle<mirror::ClassLoader> h_class_loader(hs.NewHandle(class_loader));
  UnstartedRuntimeFindClass(self, h_class_name, h_class_loader, result,
                            "VMClassLoader.findLoadedClass", false, false);
  // This might have an error pending. But semantics are to just return null.
  if (self->IsExceptionPending()) {
    // If it is an InternalError, keep it. See CheckExceptionGenerateClassNotFound.
    std::string type(PrettyTypeOf(self->GetException()));
    if (type != "java.lang.InternalError") {
      self->ClearException();
    }
  }
}

static void UnstartedSystemArraycopy(Thread* self, ShadowFrame* shadow_frame,
                                     JValue* result ATTRIBUTE_UNUSED, size_t arg_offset)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  // Special case array copying without initializing System.
  mirror::Class* ctype = shadow_frame->GetVRegReference(arg_offset)->GetClass()->GetComponentType();
  jint srcPos = shadow_frame->GetVReg(arg_offset + 1);
  jint dstPos = shadow_frame->GetVReg(arg_offset + 3);
  jint length = shadow_frame->GetVReg(arg_offset + 4);
  if (!ctype->IsPrimitive()) {
    mirror::ObjectArray<mirror::Object>* src = shadow_frame->GetVRegReference(arg_offset)->
        AsObjectArray<mirror::Object>();
    mirror::ObjectArray<mirror::Object>* dst = shadow_frame->GetVRegReference(arg_offset + 2)->
        AsObjectArray<mirror::Object>();
    for (jint i = 0; i < length; ++i) {
      dst->Set(dstPos + i, src->Get(srcPos + i));
    }
  } else if (ctype->IsPrimitiveChar()) {
    mirror::CharArray* src = shadow_frame->GetVRegReference(arg_offset)->AsCharArray();
    mirror::CharArray* dst = shadow_frame->GetVRegReference(arg_offset + 2)->AsCharArray();
    for (jint i = 0; i < length; ++i) {
      dst->Set(dstPos + i, src->Get(srcPos + i));
    }
  } else if (ctype->IsPrimitiveInt()) {
    mirror::IntArray* src = shadow_frame->GetVRegReference(arg_offset)->AsIntArray();
    mirror::IntArray* dst = shadow_frame->GetVRegReference(arg_offset + 2)->AsIntArray();
    for (jint i = 0; i < length; ++i) {
      dst->Set(dstPos + i, src->Get(srcPos + i));
    }
  } else {
    std::string tmp = StringPrintf("Unimplemented System.arraycopy for type '%s'",
                                   PrettyDescriptor(ctype).c_str());
    Runtime::Current()->AbortTransactionAndThrowInternalError(self, tmp);
  }
}

static void UnstartedThreadLocalGet(Thread* self, ShadowFrame* shadow_frame, JValue* result,
                                    size_t arg_offset ATTRIBUTE_UNUSED)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  std::string caller(PrettyMethod(shadow_frame->GetLink()->GetMethod()));
  bool ok = false;
  if (caller == "java.lang.String java.lang.IntegralToString.convertInt(java.lang.AbstractStringBuilder, int)") {
    // Allocate non-threadlocal buffer.
    result->SetL(mirror::CharArray::Alloc(self, 11));
    ok = true;
  } else if (caller == "java.lang.RealToString java.lang.RealToString.getInstance()") {
    // Note: RealToString is implemented and used in a different fashion than IntegralToString.
    // Conversion is done over an actual object of RealToString (the conversion method is an
    // instance method). This means it is not as clear whether it is correct to return a new
    // object each time. The caller needs to be inspected by hand to see whether it (incorrectly)
    // stores the object for later use.
    // See also b/19548084 for a possible rewrite and bringing it in line with IntegralToString.
    if (shadow_frame->GetLink()->GetLink() != nullptr) {
      std::string caller2(PrettyMethod(shadow_frame->GetLink()->GetLink()->GetMethod()));
      if (caller2 == "java.lang.String java.lang.Double.toString(double)") {
        // Allocate new object.
        StackHandleScope<2> hs(self);
        Handle<mirror::Class> h_real_to_string_class(hs.NewHandle(
            shadow_frame->GetLink()->GetMethod()->GetDeclaringClass()));
        Handle<mirror::Object> h_real_to_string_obj(hs.NewHandle(
            h_real_to_string_class->AllocObject(self)));
        if (h_real_to_string_obj.Get() != nullptr) {
          mirror::ArtMethod* init_method =
              h_real_to_string_class->FindDirectMethod("<init>", "()V");
          if (init_method == nullptr) {
            h_real_to_string_class->DumpClass(LOG(FATAL), mirror::Class::kDumpClassFullDetail);
          } else {
            JValue invoke_result;
            EnterInterpreterFromInvoke(self, init_method, h_real_to_string_obj.Get(), nullptr,
                                       nullptr);
            if (!self->IsExceptionPending()) {
              result->SetL(h_real_to_string_obj.Get());
              ok = true;
            }
          }
        }

        if (!ok) {
          // We'll abort, so clear exception.
          self->ClearException();
        }
      }
    }
  }

  if (!ok) {
    Runtime::Current()->AbortTransactionAndThrowInternalError(self,
        "Could not create RealToString object");
  }
}

static mirror::Object* GetDexFromDexCache(Thread* self, mirror::DexCache* dex_cache)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  const DexFile* dex_file = dex_cache->GetDexFile();
  if (dex_file == nullptr) {
    return nullptr;
  }

  // Create the direct byte buffer.
  JNIEnv* env = self->GetJniEnv();
  DCHECK(env != nullptr);
  void* address = const_cast<void*>(reinterpret_cast<const void*>(dex_file->Begin()));
  jobject byte_buffer = env->NewDirectByteBuffer(address, dex_file->Size());
  if (byte_buffer == nullptr) {
    DCHECK(self->IsExceptionPending());
    return nullptr;
  }

  jvalue args[1];
  args[0].l = byte_buffer;
  return self->DecodeJObject(
      env->CallStaticObjectMethodA(WellKnownClasses::com_android_dex_Dex,
                                   WellKnownClasses::com_android_dex_Dex_create,
                                   args));
}

static void UnstartedDexCacheGetDexNative(Thread* self, ShadowFrame* shadow_frame, JValue* result,
                                          size_t arg_offset)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  // We will create the Dex object, but the image writer will release it before creating the
  // art file.
  mirror::Object* src = shadow_frame->GetVRegReference(arg_offset);
  bool ok = false;
  if (src != nullptr) {
    mirror::Object* dex = GetDexFromDexCache(self, reinterpret_cast<mirror::DexCache*>(src));
    if (dex != nullptr) {
      ok = true;
      result->SetL(dex);
    }
  }
  if (!ok) {
    self->ClearException();
    Runtime::Current()->AbortTransactionAndThrowInternalError(self, "Could not create Dex object");
  }
}

static void UnstartedMathCeil(Thread* self ATTRIBUTE_UNUSED, ShadowFrame* shadow_frame,
                              JValue* result, size_t arg_offset) {
  double in = shadow_frame->GetVRegDouble(arg_offset);
  double out;
  // Special cases:
  // 1) NaN, infinity, +0, -0 -> out := in. All are guaranteed by cmath.
  // -1 < in < 0 -> out := -0.
  if (-1.0 < in && in < 0) {
    out = -0.0;
  } else {
    out = ceil(in);
  }
  result->SetD(out);
}

static void UnstartedArtMethodGetMethodName(Thread* self, ShadowFrame* shadow_frame,
                                            JValue* result, size_t arg_offset)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  mirror::ArtMethod* method = shadow_frame->GetVRegReference(arg_offset)->AsArtMethod();
  result->SetL(method->GetNameAsString(self));
}

static void UnstartedObjectHashCode(Thread* self ATTRIBUTE_UNUSED, ShadowFrame* shadow_frame,
                                    JValue* result, size_t arg_offset)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  mirror::Object* obj = shadow_frame->GetVRegReference(arg_offset);
  result->SetI(obj->IdentityHashCode());
}

static void UnstartedDoubleDoubleToRawLongBits(Thread* self ATTRIBUTE_UNUSED,
                                               ShadowFrame* shadow_frame, JValue* result,
                                               size_t arg_offset) {
  double in = shadow_frame->GetVRegDouble(arg_offset);
  result->SetJ(bit_cast<int64_t>(in));
}

static void UnstartedMemoryPeek(Primitive::Type type, ShadowFrame* shadow_frame,
                                JValue* result, size_t arg_offset) {
  switch (type) {
    case Primitive::kPrimByte:
    {
      int64_t address = shadow_frame->GetVRegLong(arg_offset);
      // TODO: Check that this is in the heap somewhere. Otherwise we will segfault instead of
      //       aborting the transaction.
      result->SetB(*reinterpret_cast<int8_t*>(static_cast<intptr_t>(address)));
      return;
    }

    case Primitive::kPrimShort:
    {
      int64_t address = shadow_frame->GetVRegLong(arg_offset);
      // TODO: Check that this is in the heap somewhere. Otherwise we will segfault instead of
      //       aborting the transaction.
      result->SetS(*reinterpret_cast<int16_t*>(static_cast<intptr_t>(address)));
      return;
    }

    case Primitive::kPrimInt:
    {
      int64_t address = shadow_frame->GetVRegLong(arg_offset);
      // TODO: Check that this is in the heap somewhere. Otherwise we will segfault instead of
      //       aborting the transaction.
      result->SetI(*reinterpret_cast<int32_t*>(static_cast<intptr_t>(address)));
      return;
    }

    case Primitive::kPrimLong:
    {
      int64_t address = shadow_frame->GetVRegLong(arg_offset);
      // TODO: Check that this is in the heap somewhere. Otherwise we will segfault instead of
      //       aborting the transaction.
      result->SetJ(*reinterpret_cast<int64_t*>(static_cast<intptr_t>(address)));
      return;
    }

    case Primitive::kPrimBoolean:
    case Primitive::kPrimChar:
    case Primitive::kPrimFloat:
    case Primitive::kPrimDouble:
    case Primitive::kPrimVoid:
    case Primitive::kPrimNot:
      LOG(FATAL) << "Not in the Memory API: " << type;
      UNREACHABLE();
  }
  LOG(FATAL) << "Should not reach here";
  UNREACHABLE();
}

static void UnstartedMemoryPeekArray(Primitive::Type type, Thread* self, ShadowFrame* shadow_frame,
                                     size_t arg_offset)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  int64_t address_long = shadow_frame->GetVRegLong(arg_offset);
  mirror::Object* obj = shadow_frame->GetVRegReference(arg_offset + 2);
  if (obj == nullptr) {
    Runtime::Current()->AbortTransactionAndThrowInternalError(self, "Null pointer in peekArray");
    return;
  }
  mirror::Array* array = obj->AsArray();

  int offset = shadow_frame->GetVReg(arg_offset + 3);
  int count = shadow_frame->GetVReg(arg_offset + 4);
  if (offset < 0 || offset + count > array->GetLength()) {
    std::string error_msg(StringPrintf("Array out of bounds in peekArray: %d/%d vs %d",
                                       offset, count, array->GetLength()));
    Runtime::Current()->AbortTransactionAndThrowInternalError(self, error_msg.c_str());
    return;
  }

  switch (type) {
    case Primitive::kPrimByte:
    {
      int8_t* address = reinterpret_cast<int8_t*>(static_cast<intptr_t>(address_long));
      mirror::ByteArray* byte_array = array->AsByteArray();
      for (int32_t i = 0; i < count; ++i, ++address) {
        byte_array->SetWithoutChecks<true>(i + offset, *address);
      }
      return;
    }

    case Primitive::kPrimShort:
    case Primitive::kPrimInt:
    case Primitive::kPrimLong:
      LOG(FATAL) << "Type unimplemented for Memory Array API, should not reach here: " << type;
      UNREACHABLE();

    case Primitive::kPrimBoolean:
    case Primitive::kPrimChar:
    case Primitive::kPrimFloat:
    case Primitive::kPrimDouble:
    case Primitive::kPrimVoid:
    case Primitive::kPrimNot:
      LOG(FATAL) << "Not in the Memory API: " << type;
      UNREACHABLE();
  }
  LOG(FATAL) << "Should not reach here";
  UNREACHABLE();
}

void UnstartedRuntimeInvoke(Thread* self,  const DexFile::CodeItem* code_item,
                            ShadowFrame* shadow_frame,
                            JValue* result, size_t arg_offset) {
  // In a runtime that's not started we intercept certain methods to avoid complicated dependency
  // problems in core libraries.
  std::string name(PrettyMethod(shadow_frame->GetMethod()));
  if (name == "java.lang.Class java.lang.Class.forName(java.lang.String)") {
    UnstartedClassForName(self, shadow_frame, result, arg_offset);
  } else if (name == "java.lang.Class java.lang.Class.forName(java.lang.String, boolean, java.lang.ClassLoader)") {
    UnstartedClassForNameLong(self, shadow_frame, result, arg_offset);
  } else if (name == "java.lang.Class java.lang.Class.classForName(java.lang.String, boolean, java.lang.ClassLoader)") {
    UnstartedClassClassForName(self, shadow_frame, result, arg_offset);
  } else if (name == "java.lang.Class java.lang.VMClassLoader.findLoadedClass(java.lang.ClassLoader, java.lang.String)") {
    UnstartedVmClassLoaderFindLoadedClass(self, shadow_frame, result, arg_offset);
  } else if (name == "java.lang.Class java.lang.Void.lookupType()") {
    result->SetL(Runtime::Current()->GetClassLinker()->FindPrimitiveClass('V'));
  } else if (name == "java.lang.Object java.lang.Class.newInstance()") {
    UnstartedClassNewInstance(self, shadow_frame, result, arg_offset);
  } else if (name == "java.lang.reflect.Field java.lang.Class.getDeclaredField(java.lang.String)") {
    UnstartedClassGetDeclaredField(self, shadow_frame, result, arg_offset);
  } else if (name == "int java.lang.Object.hashCode()") {
    UnstartedObjectHashCode(self, shadow_frame, result, arg_offset);
  } else if (name == "java.lang.String java.lang.reflect.ArtMethod.getMethodName(java.lang.reflect.ArtMethod)") {
    UnstartedArtMethodGetMethodName(self, shadow_frame, result, arg_offset);
  } else if (name == "void java.lang.System.arraycopy(java.lang.Object, int, java.lang.Object, int, int)" ||
             name == "void java.lang.System.arraycopy(char[], int, char[], int, int)" ||
             name == "void java.lang.System.arraycopy(int[], int, int[], int, int)") {
    UnstartedSystemArraycopy(self, shadow_frame, result, arg_offset);
  } else if (name == "long java.lang.Double.doubleToRawLongBits(double)") {
    UnstartedDoubleDoubleToRawLongBits(self, shadow_frame, result, arg_offset);
  } else if (name == "double java.lang.Math.ceil(double)") {
    UnstartedMathCeil(self, shadow_frame, result, arg_offset);
  } else if (name == "java.lang.Object java.lang.ThreadLocal.get()") {
    UnstartedThreadLocalGet(self, shadow_frame, result, arg_offset);
  } else if (name == "com.android.dex.Dex java.lang.DexCache.getDexNative()") {
    UnstartedDexCacheGetDexNative(self, shadow_frame, result, arg_offset);
  } else if (name == "byte libcore.io.Memory.peekByte(long)") {
    UnstartedMemoryPeek(Primitive::kPrimByte, shadow_frame, result, arg_offset);
  } else if (name == "short libcore.io.Memory.peekShortNative(long)") {
    UnstartedMemoryPeek(Primitive::kPrimShort, shadow_frame, result, arg_offset);
  } else if (name == "int libcore.io.Memory.peekIntNative(long)") {
    UnstartedMemoryPeek(Primitive::kPrimInt, shadow_frame, result, arg_offset);
  } else if (name == "long libcore.io.Memory.peekLongNative(long)") {
    UnstartedMemoryPeek(Primitive::kPrimLong, shadow_frame, result, arg_offset);
  } else if (name == "void libcore.io.Memory.peekByteArray(long, byte[], int, int)") {
    UnstartedMemoryPeekArray(Primitive::kPrimByte, self, shadow_frame, arg_offset);
  } else {
    // Not special, continue with regular interpreter execution.
    artInterpreterToInterpreterBridge(self, code_item, shadow_frame, result);
  }
}

// Hand select a number of methods to be run in a not yet started runtime without using JNI.
void UnstartedRuntimeJni(Thread* self, mirror::ArtMethod* method, mirror::Object* receiver,
                         uint32_t* args, JValue* result) {
  std::string name(PrettyMethod(method));
  if (name == "java.lang.Object dalvik.system.VMRuntime.newUnpaddedArray(java.lang.Class, int)") {
    int32_t length = args[1];
    DCHECK_GE(length, 0);
    mirror::Class* element_class = reinterpret_cast<mirror::Object*>(args[0])->AsClass();
    Runtime* runtime = Runtime::Current();
    mirror::Class* array_class = runtime->GetClassLinker()->FindArrayClass(self, &element_class);
    DCHECK(array_class != nullptr);
    gc::AllocatorType allocator = runtime->GetHeap()->GetCurrentAllocator();
    result->SetL(mirror::Array::Alloc<true, true>(self, array_class, length,
                                                  array_class->GetComponentSizeShift(), allocator));
  } else if (name == "java.lang.ClassLoader dalvik.system.VMStack.getCallingClassLoader()") {
    result->SetL(NULL);
  } else if (name == "java.lang.Class dalvik.system.VMStack.getStackClass2()") {
    NthCallerVisitor visitor(self, 3);
    visitor.WalkStack();
    result->SetL(visitor.caller->GetDeclaringClass());
  } else if (name == "double java.lang.Math.log(double)") {
    JValue value;
    value.SetJ((static_cast<uint64_t>(args[1]) << 32) | args[0]);
    result->SetD(log(value.GetD()));
  } else if (name == "java.lang.String java.lang.Class.getNameNative()") {
    StackHandleScope<1> hs(self);
    result->SetL(mirror::Class::ComputeName(hs.NewHandle(receiver->AsClass())));
  } else if (name == "int java.lang.Float.floatToRawIntBits(float)") {
    result->SetI(args[0]);
  } else if (name == "float java.lang.Float.intBitsToFloat(int)") {
    result->SetI(args[0]);
  } else if (name == "double java.lang.Math.exp(double)") {
    JValue value;
    value.SetJ((static_cast<uint64_t>(args[1]) << 32) | args[0]);
    result->SetD(exp(value.GetD()));
  } else if (name == "java.lang.Object java.lang.Object.internalClone()") {
    result->SetL(receiver->Clone(self));
  } else if (name == "void java.lang.Object.notifyAll()") {
    receiver->NotifyAll(self);
  } else if (name == "int java.lang.String.compareTo(java.lang.String)") {
    mirror::String* rhs = reinterpret_cast<mirror::Object*>(args[0])->AsString();
    CHECK(rhs != NULL);
    result->SetI(receiver->AsString()->CompareTo(rhs));
  } else if (name == "java.lang.String java.lang.String.intern()") {
    result->SetL(receiver->AsString()->Intern());
  } else if (name == "int java.lang.String.fastIndexOf(int, int)") {
    result->SetI(receiver->AsString()->FastIndexOf(args[0], args[1]));
  } else if (name == "java.lang.Object java.lang.reflect.Array.createMultiArray(java.lang.Class, int[])") {
    StackHandleScope<2> hs(self);
    auto h_class(hs.NewHandle(reinterpret_cast<mirror::Class*>(args[0])->AsClass()));
    auto h_dimensions(hs.NewHandle(reinterpret_cast<mirror::IntArray*>(args[1])->AsIntArray()));
    result->SetL(mirror::Array::CreateMultiArray(self, h_class, h_dimensions));
  } else if (name == "java.lang.Object java.lang.Throwable.nativeFillInStackTrace()") {
    ScopedObjectAccessUnchecked soa(self);
    if (Runtime::Current()->IsActiveTransaction()) {
      result->SetL(soa.Decode<mirror::Object*>(self->CreateInternalStackTrace<true>(soa)));
    } else {
      result->SetL(soa.Decode<mirror::Object*>(self->CreateInternalStackTrace<false>(soa)));
    }
  } else if (name == "int java.lang.System.identityHashCode(java.lang.Object)") {
    mirror::Object* obj = reinterpret_cast<mirror::Object*>(args[0]);
    result->SetI((obj != nullptr) ? obj->IdentityHashCode() : 0);
  } else if (name == "boolean java.nio.ByteOrder.isLittleEndian()") {
    result->SetZ(JNI_TRUE);
  } else if (name == "boolean sun.misc.Unsafe.compareAndSwapInt(java.lang.Object, long, int, int)") {
    mirror::Object* obj = reinterpret_cast<mirror::Object*>(args[0]);
    jlong offset = (static_cast<uint64_t>(args[2]) << 32) | args[1];
    jint expectedValue = args[3];
    jint newValue = args[4];
    bool success;
    if (Runtime::Current()->IsActiveTransaction()) {
      success = obj->CasFieldStrongSequentiallyConsistent32<true>(MemberOffset(offset),
                                                                  expectedValue, newValue);
    } else {
      success = obj->CasFieldStrongSequentiallyConsistent32<false>(MemberOffset(offset),
                                                                   expectedValue, newValue);
    }
    result->SetZ(success ? JNI_TRUE : JNI_FALSE);
  } else if (name == "void sun.misc.Unsafe.putObject(java.lang.Object, long, java.lang.Object)") {
    mirror::Object* obj = reinterpret_cast<mirror::Object*>(args[0]);
    jlong offset = (static_cast<uint64_t>(args[2]) << 32) | args[1];
    mirror::Object* newValue = reinterpret_cast<mirror::Object*>(args[3]);
    if (Runtime::Current()->IsActiveTransaction()) {
      obj->SetFieldObject<true>(MemberOffset(offset), newValue);
    } else {
      obj->SetFieldObject<false>(MemberOffset(offset), newValue);
    }
  } else if (name == "int sun.misc.Unsafe.getArrayBaseOffsetForComponentType(java.lang.Class)") {
    mirror::Class* component = reinterpret_cast<mirror::Object*>(args[0])->AsClass();
    Primitive::Type primitive_type = component->GetPrimitiveType();
    result->SetI(mirror::Array::DataOffset(Primitive::ComponentSize(primitive_type)).Int32Value());
  } else if (name == "int sun.misc.Unsafe.getArrayIndexScaleForComponentType(java.lang.Class)") {
    mirror::Class* component = reinterpret_cast<mirror::Object*>(args[0])->AsClass();
    Primitive::Type primitive_type = component->GetPrimitiveType();
    result->SetI(Primitive::ComponentSize(primitive_type));
  } else if (Runtime::Current()->IsActiveTransaction()) {
    AbortTransaction(self, "Attempt to invoke native method in non-started runtime: %s",
                     name.c_str());

  } else {
    LOG(FATAL) << "Calling native method " << PrettyMethod(method) << " in an unstarted "
        "non-transactional runtime";
  }
}

}  // namespace interpreter
}  // namespace art
