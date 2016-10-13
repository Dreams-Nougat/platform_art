/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ART_RUNTIME_ART_METHOD_H_
#define ART_RUNTIME_ART_METHOD_H_

#include <cstddef>

#include "base/bit_utils.h"
#include "base/casts.h"
#include "base/enums.h"
#include "dex_file.h"
#include "gc_root.h"
#include "invoke_type.h"
#include "method_reference.h"
#include "modifiers.h"
#include "mirror/object.h"
#include "obj_ptr.h"
#include "read_barrier_option.h"
#include "utils.h"

namespace art {

template<class T> class Handle;
class ImtConflictTable;
union JValue;
class OatQuickMethodHeader;
class ProfilingInfo;
class ScopedObjectAccessAlreadyRunnable;
class StringPiece;
class ShadowFrame;

namespace mirror {
class Array;
class Class;
class IfTable;
class PointerArray;
}  // namespace mirror

class ArtMethod FINAL {
 public:
  ArtMethod() : access_flags_(0), dex_code_item_offset_(0), dex_method_index_(0),
      method_index_(0), hotness_count_(0) { }

  ArtMethod(ArtMethod* src, PointerSize image_pointer_size) {
    CopyFrom(src, image_pointer_size);
  }

  static ArtMethod* FromReflectedMethod(const ScopedObjectAccessAlreadyRunnable& soa,
                                        jobject jlr_method)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template <ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  ALWAYS_INLINE mirror::Class* GetDeclaringClass() REQUIRES_SHARED(Locks::mutator_lock_);

  template <ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  ALWAYS_INLINE mirror::Class* GetDeclaringClassUnchecked()
      REQUIRES_SHARED(Locks::mutator_lock_);

  void SetDeclaringClass(ObjPtr<mirror::Class> new_declaring_class)
      REQUIRES_SHARED(Locks::mutator_lock_);

  bool CASDeclaringClass(mirror::Class* expected_class, mirror::Class* desired_class)
      REQUIRES_SHARED(Locks::mutator_lock_);

  static MemberOffset DeclaringClassOffset() {
    return MemberOffset(OFFSETOF_MEMBER(ArtMethod, declaring_class_));
  }

  // Note: GetAccessFlags acquires the mutator lock in debug mode to check that it is not called for
  // a proxy method.
  template <ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  ALWAYS_INLINE uint32_t GetAccessFlags();

  void SetAccessFlags(uint32_t new_access_flags) {
    // Not called within a transaction.
    access_flags_ = new_access_flags;
  }

  // Approximate what kind of method call would be used for this method.
  InvokeType GetInvokeType() REQUIRES_SHARED(Locks::mutator_lock_);

  // Returns true if the method is declared public.
  bool IsPublic() {
    return (GetAccessFlags() & kAccPublic) != 0;
  }

  // Returns true if the method is declared private.
  bool IsPrivate() {
    return (GetAccessFlags() & kAccPrivate) != 0;
  }

  // Returns true if the method is declared static.
  bool IsStatic() {
    return (GetAccessFlags() & kAccStatic) != 0;
  }

  // Returns true if the method is a constructor.
  bool IsConstructor() {
    return (GetAccessFlags() & kAccConstructor) != 0;
  }

  // Returns true if the method is a class initializer.
  bool IsClassInitializer() {
    return IsConstructor() && IsStatic();
  }

  // Returns true if the method is static, private, or a constructor.
  bool IsDirect() {
    return IsDirect(GetAccessFlags());
  }

  static bool IsDirect(uint32_t access_flags) {
    constexpr uint32_t direct = kAccStatic | kAccPrivate | kAccConstructor;
    return (access_flags & direct) != 0;
  }

  // Returns true if the method is declared synchronized.
  bool IsSynchronized() {
    constexpr uint32_t synchonized = kAccSynchronized | kAccDeclaredSynchronized;
    return (GetAccessFlags() & synchonized) != 0;
  }

  bool IsFinal() {
    return (GetAccessFlags() & kAccFinal) != 0;
  }

  bool IsIntrinsic() {
    return (GetAccessFlags() & kAccIntrinsic) != 0;
  }

  void SetIntrinsic(uint32_t intrinsic) {
    DCHECK(IsUint<8>(intrinsic));
    uint32_t new_value = (GetAccessFlags() & kAccFlagsNotUsedByIntrinsic) |
        kAccIntrinsic |
        (intrinsic << POPCOUNT(kAccFlagsNotUsedByIntrinsic));
    if (kIsDebugBuild) {
      uint32_t java_flags = (GetAccessFlags() & kAccJavaFlagsMask);
      bool is_constructor = IsConstructor();
      bool is_synchronized = IsSynchronized();
      bool skip_access_checks = SkipAccessChecks();
      bool is_fast_native = IsFastNative();
      bool is_copied = IsCopied();
      bool is_miranda = IsMiranda();
      bool is_default = IsDefault();
      bool is_default_conflict = IsDefaultConflicting();
      bool is_compilable = IsCompilable();
      bool must_count_locks = MustCountLocks();
      SetAccessFlags(new_value);
      DCHECK_EQ(java_flags, (GetAccessFlags() & kAccJavaFlagsMask));
      DCHECK_EQ(is_constructor, IsConstructor());
      DCHECK_EQ(is_synchronized, IsSynchronized());
      DCHECK_EQ(skip_access_checks, SkipAccessChecks());
      DCHECK_EQ(is_fast_native, IsFastNative());
      DCHECK_EQ(is_copied, IsCopied());
      DCHECK_EQ(is_miranda, IsMiranda());
      DCHECK_EQ(is_default, IsDefault());
      DCHECK_EQ(is_default_conflict, IsDefaultConflicting());
      DCHECK_EQ(is_compilable, IsCompilable());
      DCHECK_EQ(must_count_locks, MustCountLocks());
    } else {
      SetAccessFlags(new_value);
    }
  }

  uint32_t GetIntrinsic() {
    DCHECK(IsIntrinsic());
    return (GetAccessFlags() >> POPCOUNT(kAccFlagsNotUsedByIntrinsic)) & kAccMaxIntrinsic;
  }

  bool IsCopied() {
    static_assert((kAccCopied & kAccFlagsNotUsedByIntrinsic) == kAccCopied,
                  "kAccCopied conflicts with intrinsic modifier");
    const bool copied = (GetAccessFlags() & kAccCopied) != 0;
    // (IsMiranda() || IsDefaultConflicting()) implies copied
    DCHECK(!(IsMiranda() || IsDefaultConflicting()) || copied)
        << "Miranda or default-conflict methods must always be copied.";
    return copied;
  }

  bool IsMiranda() {
    static_assert((kAccMiranda & kAccFlagsNotUsedByIntrinsic) == kAccMiranda,
                  "kAccMiranda conflicts with intrinsic modifier");
    return (GetAccessFlags() & kAccMiranda) != 0;
  }

  // Returns true if invoking this method will not throw an AbstractMethodError or
  // IncompatibleClassChangeError.
  bool IsInvokable() {
    return !IsAbstract() && !IsDefaultConflicting();
  }

  bool IsCompilable() {
    if (IsIntrinsic()) {
      return true;
    }
    return (GetAccessFlags() & kAccCompileDontBother) == 0;
  }

  // A default conflict method is a special sentinel method that stands for a conflict between
  // multiple default methods. It cannot be invoked, throwing an IncompatibleClassChangeError if one
  // attempts to do so.
  bool IsDefaultConflicting() {
    if (IsIntrinsic()) {
      return false;
    }
    return (GetAccessFlags() & kAccDefaultConflict) != 0u;
  }

  // This is set by the class linker.
  bool IsDefault() {
    static_assert((kAccDefault & kAccFlagsNotUsedByIntrinsic) == kAccDefault,
                  "kAccDefault conflicts with intrinsic modifier");
    return (GetAccessFlags() & kAccDefault) != 0;
  }

  template <ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  bool IsNative() {
    return (GetAccessFlags<kReadBarrierOption>() & kAccNative) != 0;
  }

  bool IsFastNative() {
    constexpr uint32_t mask = kAccFastNative | kAccNative;
    return (GetAccessFlags() & mask) == mask;
  }

  bool IsAbstract() {
    return (GetAccessFlags() & kAccAbstract) != 0;
  }

  bool IsSynthetic() {
    return (GetAccessFlags() & kAccSynthetic) != 0;
  }

  bool IsVarargs() {
    return (GetAccessFlags() & kAccVarargs) != 0;
  }

  template<ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  bool IsProxyMethod() REQUIRES_SHARED(Locks::mutator_lock_);

  bool SkipAccessChecks() {
    return (GetAccessFlags() & kAccSkipAccessChecks) != 0;
  }

  void SetSkipAccessChecks() {
    DCHECK(!SkipAccessChecks());
    SetAccessFlags(GetAccessFlags() | kAccSkipAccessChecks);
  }

  // Should this method be run in the interpreter and count locks (e.g., failed structured-
  // locking verification)?
  bool MustCountLocks() {
    if (IsIntrinsic()) {
      return false;
    }
    return (GetAccessFlags() & kAccMustCountLocks) != 0;
  }

  // Checks to see if the method was annotated with @dalvik.annotation.optimization.FastNative
  // -- Independent of kAccFastNative access flags.
  bool IsAnnotatedWithFastNative();

  // Checks to see if the method was annotated with @dalvik.annotation.optimization.CriticalNative
  // -- Unrelated to the GC notion of "critical".
  bool IsAnnotatedWithCriticalNative();

  // Returns true if this method could be overridden by a default method.
  bool IsOverridableByDefaultMethod() REQUIRES_SHARED(Locks::mutator_lock_);

  bool CheckIncompatibleClassChange(InvokeType type) REQUIRES_SHARED(Locks::mutator_lock_);

  // Throws the error that would result from trying to invoke this method (i.e.
  // IncompatibleClassChangeError or AbstractMethodError). Only call if !IsInvokable();
  void ThrowInvocationTimeError() REQUIRES_SHARED(Locks::mutator_lock_);

  uint16_t GetMethodIndex() REQUIRES_SHARED(Locks::mutator_lock_);

  // Doesn't do erroneous / unresolved class checks.
  uint16_t GetMethodIndexDuringLinking() REQUIRES_SHARED(Locks::mutator_lock_);

  size_t GetVtableIndex() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetMethodIndex();
  }

  void SetMethodIndex(uint16_t new_method_index) REQUIRES_SHARED(Locks::mutator_lock_) {
    // Not called within a transaction.
    method_index_ = new_method_index;
  }

  static MemberOffset DexMethodIndexOffset() {
    return OFFSET_OF_OBJECT_MEMBER(ArtMethod, dex_method_index_);
  }

  static MemberOffset MethodIndexOffset() {
    return OFFSET_OF_OBJECT_MEMBER(ArtMethod, method_index_);
  }

  uint32_t GetCodeItemOffset() {
    return dex_code_item_offset_;
  }

  void SetCodeItemOffset(uint32_t new_code_off) {
    // Not called within a transaction.
    dex_code_item_offset_ = new_code_off;
  }

  // Number of 32bit registers that would be required to hold all the arguments
  static size_t NumArgRegisters(const StringPiece& shorty);

  ALWAYS_INLINE uint32_t GetDexMethodIndex() REQUIRES_SHARED(Locks::mutator_lock_);

  void SetDexMethodIndex(uint32_t new_idx) {
    // Not called within a transaction.
    dex_method_index_ = new_idx;
  }

  ALWAYS_INLINE ArtMethod** GetDexCacheResolvedMethods(PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);
  ALWAYS_INLINE ArtMethod* GetDexCacheResolvedMethod(uint16_t method_index,
                                                     PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);
  ALWAYS_INLINE void SetDexCacheResolvedMethod(uint16_t method_index,
                                               ArtMethod* new_method,
                                               PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);
  ALWAYS_INLINE void SetDexCacheResolvedMethods(ArtMethod** new_dex_cache_methods,
                                                PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);
  bool HasDexCacheResolvedMethods(PointerSize pointer_size) REQUIRES_SHARED(Locks::mutator_lock_);
  bool HasSameDexCacheResolvedMethods(ArtMethod* other, PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);
  bool HasSameDexCacheResolvedMethods(ArtMethod** other_cache, PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template <bool kWithCheck = true>
  mirror::Class* GetDexCacheResolvedType(dex::TypeIndex type_idx, PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);
  void SetDexCacheResolvedTypes(GcRoot<mirror::Class>* new_dex_cache_types,
                                PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);
  bool HasDexCacheResolvedTypes(PointerSize pointer_size) REQUIRES_SHARED(Locks::mutator_lock_);
  bool HasSameDexCacheResolvedTypes(ArtMethod* other, PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);
  bool HasSameDexCacheResolvedTypes(GcRoot<mirror::Class>* other_cache, PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Get the Class* from the type index into this method's dex cache.
  mirror::Class* GetClassFromTypeIndex(dex::TypeIndex type_idx,
                                       bool resolve,
                                       PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Returns true if this method has the same name and signature of the other method.
  bool HasSameNameAndSignature(ArtMethod* other) REQUIRES_SHARED(Locks::mutator_lock_);

  // Find the method that this method overrides.
  ArtMethod* FindOverriddenMethod(PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Find the method index for this method within other_dexfile. If this method isn't present then
  // return DexFile::kDexNoIndex. The name_and_signature_idx MUST refer to a MethodId with the same
  // name and signature in the other_dexfile, such as the method index used to resolve this method
  // in the other_dexfile.
  uint32_t FindDexMethodIndexInOtherDexFile(const DexFile& other_dexfile,
                                            uint32_t name_and_signature_idx)
      REQUIRES_SHARED(Locks::mutator_lock_);

  void Invoke(Thread* self, uint32_t* args, uint32_t args_size, JValue* result, const char* shorty)
      REQUIRES_SHARED(Locks::mutator_lock_);

  const void* GetEntryPointFromQuickCompiledCode() {
    return GetEntryPointFromQuickCompiledCodePtrSize(kRuntimePointerSize);
  }
  ALWAYS_INLINE const void* GetEntryPointFromQuickCompiledCodePtrSize(PointerSize pointer_size) {
    return GetNativePointer<const void*>(
        EntryPointFromQuickCompiledCodeOffset(pointer_size), pointer_size);
  }

  void SetEntryPointFromQuickCompiledCode(const void* entry_point_from_quick_compiled_code) {
    SetEntryPointFromQuickCompiledCodePtrSize(entry_point_from_quick_compiled_code,
                                              kRuntimePointerSize);
  }
  ALWAYS_INLINE void SetEntryPointFromQuickCompiledCodePtrSize(
      const void* entry_point_from_quick_compiled_code, PointerSize pointer_size) {
    SetNativePointer(EntryPointFromQuickCompiledCodeOffset(pointer_size),
                     entry_point_from_quick_compiled_code,
                     pointer_size);
  }

  void RegisterNative(const void* native_method, bool is_fast)
      REQUIRES_SHARED(Locks::mutator_lock_);

  void UnregisterNative() REQUIRES_SHARED(Locks::mutator_lock_);

  static MemberOffset DexCacheResolvedMethodsOffset(PointerSize pointer_size) {
    return MemberOffset(PtrSizedFieldsOffset(pointer_size) + OFFSETOF_MEMBER(
        PtrSizedFields, dex_cache_resolved_methods_) / sizeof(void*)
            * static_cast<size_t>(pointer_size));
  }

  static MemberOffset DexCacheResolvedTypesOffset(PointerSize pointer_size) {
    return MemberOffset(PtrSizedFieldsOffset(pointer_size) + OFFSETOF_MEMBER(
        PtrSizedFields, dex_cache_resolved_types_) / sizeof(void*)
            * static_cast<size_t>(pointer_size));
  }

  static MemberOffset DataOffset(PointerSize pointer_size) {
    return MemberOffset(PtrSizedFieldsOffset(pointer_size) + OFFSETOF_MEMBER(
        PtrSizedFields, data_) / sizeof(void*) * static_cast<size_t>(pointer_size));
  }

  static MemberOffset EntryPointFromJniOffset(PointerSize pointer_size) {
    return DataOffset(pointer_size);
  }

  static MemberOffset EntryPointFromQuickCompiledCodeOffset(PointerSize pointer_size) {
    return MemberOffset(PtrSizedFieldsOffset(pointer_size) + OFFSETOF_MEMBER(
        PtrSizedFields, entry_point_from_quick_compiled_code_) / sizeof(void*)
            * static_cast<size_t>(pointer_size));
  }

  ImtConflictTable* GetImtConflictTable(PointerSize pointer_size) {
    DCHECK(IsRuntimeMethod());
    return reinterpret_cast<ImtConflictTable*>(GetDataPtrSize(pointer_size));
  }

  ALWAYS_INLINE void SetImtConflictTable(ImtConflictTable* table, PointerSize pointer_size) {
    DCHECK(IsRuntimeMethod());
    SetDataPtrSize(table, pointer_size);
  }

  ProfilingInfo* GetProfilingInfo(PointerSize pointer_size) {
    return reinterpret_cast<ProfilingInfo*>(GetDataPtrSize(pointer_size));
  }

  ALWAYS_INLINE void SetProfilingInfo(ProfilingInfo* info) {
    SetDataPtrSize(info, kRuntimePointerSize);
  }

  ALWAYS_INLINE void SetProfilingInfoPtrSize(ProfilingInfo* info, PointerSize pointer_size) {
    SetDataPtrSize(info, pointer_size);
  }

  static MemberOffset ProfilingInfoOffset() {
    DCHECK(IsImagePointerSize(kRuntimePointerSize));
    return DataOffset(kRuntimePointerSize);
  }

  void* GetEntryPointFromJni() {
    DCHECK(IsNative());
    return GetEntryPointFromJniPtrSize(kRuntimePointerSize);
  }

  ALWAYS_INLINE void* GetEntryPointFromJniPtrSize(PointerSize pointer_size) {
    return GetDataPtrSize(pointer_size);
  }

  void SetEntryPointFromJni(const void* entrypoint) {
    DCHECK(IsNative());
    SetEntryPointFromJniPtrSize(entrypoint, kRuntimePointerSize);
  }

  ALWAYS_INLINE void SetEntryPointFromJniPtrSize(const void* entrypoint, PointerSize pointer_size) {
    SetDataPtrSize(entrypoint, pointer_size);
  }

  ALWAYS_INLINE void* GetDataPtrSize(PointerSize pointer_size) {
    DCHECK(IsImagePointerSize(pointer_size));
    return GetNativePointer<void*>(DataOffset(pointer_size), pointer_size);
  }

  ALWAYS_INLINE void SetDataPtrSize(const void* data, PointerSize pointer_size) {
    DCHECK(IsImagePointerSize(pointer_size));
    SetNativePointer(DataOffset(pointer_size), data, pointer_size);
  }

  // Is this a CalleSaveMethod or ResolutionMethod and therefore doesn't adhere to normal
  // conventions for a method of managed code. Returns false for Proxy methods.
  ALWAYS_INLINE bool IsRuntimeMethod();

  // Is this a hand crafted method used for something like describing callee saves?
  bool IsCalleeSaveMethod() REQUIRES_SHARED(Locks::mutator_lock_);

  bool IsResolutionMethod() REQUIRES_SHARED(Locks::mutator_lock_);

  bool IsImtUnimplementedMethod() REQUIRES_SHARED(Locks::mutator_lock_);

  MethodReference ToMethodReference() REQUIRES_SHARED(Locks::mutator_lock_) {
    return MethodReference(GetDexFile(), GetDexMethodIndex());
  }

  // Find the catch block for the given exception type and dex_pc. When a catch block is found,
  // indicates whether the found catch block is responsible for clearing the exception or whether
  // a move-exception instruction is present.
  uint32_t FindCatchBlock(Handle<mirror::Class> exception_type, uint32_t dex_pc,
                          bool* has_no_move_exception)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // NO_THREAD_SAFETY_ANALYSIS since we don't know what the callback requires.
  template<ReadBarrierOption kReadBarrierOption = kWithReadBarrier, typename RootVisitorType>
  void VisitRoots(RootVisitorType& visitor, PointerSize pointer_size) NO_THREAD_SAFETY_ANALYSIS;

  const DexFile* GetDexFile() REQUIRES_SHARED(Locks::mutator_lock_);

  const char* GetDeclaringClassDescriptor() REQUIRES_SHARED(Locks::mutator_lock_);

  const char* GetShorty() REQUIRES_SHARED(Locks::mutator_lock_) {
    uint32_t unused_length;
    return GetShorty(&unused_length);
  }

  const char* GetShorty(uint32_t* out_length) REQUIRES_SHARED(Locks::mutator_lock_);

  const Signature GetSignature() REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE const char* GetName() REQUIRES_SHARED(Locks::mutator_lock_);

  mirror::String* GetNameAsString(Thread* self) REQUIRES_SHARED(Locks::mutator_lock_);

  const DexFile::CodeItem* GetCodeItem() REQUIRES_SHARED(Locks::mutator_lock_);

  bool IsResolvedTypeIdx(dex::TypeIndex type_idx, PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  int32_t GetLineNumFromDexPC(uint32_t dex_pc) REQUIRES_SHARED(Locks::mutator_lock_);

  const DexFile::ProtoId& GetPrototype() REQUIRES_SHARED(Locks::mutator_lock_);

  const DexFile::TypeList* GetParameterTypeList() REQUIRES_SHARED(Locks::mutator_lock_);

  const char* GetDeclaringClassSourceFile() REQUIRES_SHARED(Locks::mutator_lock_);

  uint16_t GetClassDefIndex() REQUIRES_SHARED(Locks::mutator_lock_);

  const DexFile::ClassDef& GetClassDef() REQUIRES_SHARED(Locks::mutator_lock_);

  const char* GetReturnTypeDescriptor() REQUIRES_SHARED(Locks::mutator_lock_);

  const char* GetTypeDescriptorFromTypeIdx(dex::TypeIndex type_idx)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // May cause thread suspension due to GetClassFromTypeIdx calling ResolveType this caused a large
  // number of bugs at call sites.
  mirror::Class* GetReturnType(bool resolve, PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  mirror::ClassLoader* GetClassLoader() REQUIRES_SHARED(Locks::mutator_lock_);

  mirror::DexCache* GetDexCache() REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE ArtMethod* GetInterfaceMethodIfProxy(PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // May cause thread suspension due to class resolution.
  bool EqualParameters(Handle<mirror::ObjectArray<mirror::Class>> params)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Size of an instance of this native class.
  static size_t Size(PointerSize pointer_size) {
    return PtrSizedFieldsOffset(pointer_size) +
        (sizeof(PtrSizedFields) / sizeof(void*)) * static_cast<size_t>(pointer_size);
  }

  // Alignment of an instance of this native class.
  static size_t Alignment(PointerSize pointer_size) {
    // The ArtMethod alignment is the same as image pointer size. This differs from
    // alignof(ArtMethod) if cross-compiling with pointer_size != sizeof(void*).
    return static_cast<size_t>(pointer_size);
  }

  void CopyFrom(ArtMethod* src, PointerSize image_pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE GcRoot<mirror::Class>* GetDexCacheResolvedTypes(PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Note, hotness_counter_ updates are non-atomic but it doesn't need to be precise.  Also,
  // given that the counter is only 16 bits wide we can expect wrap-around in some
  // situations.  Consumers of hotness_count_ must be able to deal with that.
  uint16_t IncrementCounter() {
    return ++hotness_count_;
  }

  void ClearCounter() {
    hotness_count_ = 0;
  }

  void SetCounter(int16_t hotness_count) {
    hotness_count_ = hotness_count;
  }

  uint16_t GetCounter() const {
    return hotness_count_;
  }

  const uint8_t* GetQuickenedInfo(PointerSize pointer_size) REQUIRES_SHARED(Locks::mutator_lock_);

  // Returns the method header for the compiled code containing 'pc'. Note that runtime
  // methods will return null for this method, as they are not oat based.
  const OatQuickMethodHeader* GetOatQuickMethodHeader(uintptr_t pc)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Get compiled code for the method, return null if no code exists.
  const void* GetOatMethodQuickCode(PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Returns whether the method has any compiled code, JIT or AOT.
  bool HasAnyCompiledCode() REQUIRES_SHARED(Locks::mutator_lock_);

  // Returns a human-readable signature for 'm'. Something like "a.b.C.m" or
  // "a.b.C.m(II)V" (depending on the value of 'with_signature').
  static std::string PrettyMethod(ArtMethod* m, bool with_signature = true)
      REQUIRES_SHARED(Locks::mutator_lock_);
  std::string PrettyMethod(bool with_signature = true)
      REQUIRES_SHARED(Locks::mutator_lock_);
  // Returns the JNI native function name for the non-overloaded method 'm'.
  std::string JniShortName()
      REQUIRES_SHARED(Locks::mutator_lock_);
  // Returns the JNI native function name for the overloaded method 'm'.
  std::string JniLongName()
      REQUIRES_SHARED(Locks::mutator_lock_);



  // Update heap objects and non-entrypoint pointers by the passed in visitor for image relocation.
  // Does not use read barrier.
  template <typename Visitor>
  ALWAYS_INLINE void UpdateObjectsForImageRelocation(const Visitor& visitor,
                                                     PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Update entry points by passing them through the visitor.
  template <ReadBarrierOption kReadBarrierOption = kWithReadBarrier, typename Visitor>
  ALWAYS_INLINE void UpdateEntrypoints(const Visitor& visitor, PointerSize pointer_size);

 protected:
  // Field order required by test "ValidateFieldOrderOfJavaCppUnionClasses".
  // The class we are a part of.
  GcRoot<mirror::Class> declaring_class_;

  // Access flags; low 16 bits are defined by spec.
  uint32_t access_flags_;

  /* Dex file fields. The defining dex file is available via declaring_class_->dex_cache_ */

  // Offset to the CodeItem.
  uint32_t dex_code_item_offset_;

  // Index into method_ids of the dex file associated with this method.
  uint32_t dex_method_index_;

  /* End of dex file fields. */

  // Entry within a dispatch table for this method. For static/direct methods the index is into
  // the declaringClass.directMethods, for virtual methods the vtable and for interface methods the
  // ifTable.
  uint16_t method_index_;

  // The hotness we measure for this method. Managed by the interpreter. Not atomic, as we allow
  // missing increments: if the method is hot, we will see it eventually.
  uint16_t hotness_count_;

  // Fake padding field gets inserted here.

  // Must be the last fields in the method.
  struct PtrSizedFields {
    // Short cuts to declaring_class_->dex_cache_ member for fast compiled code access.
    ArtMethod** dex_cache_resolved_methods_;

    // Short cuts to declaring_class_->dex_cache_ member for fast compiled code access.
    GcRoot<mirror::Class>* dex_cache_resolved_types_;

    // Pointer to JNI function registered to this method, or a function to resolve the JNI function,
    // or the profiling data for non-native methods, or an ImtConflictTable.
    void* data_;

    // Method dispatch from quick compiled code invokes this pointer which may cause bridging into
    // the interpreter.
    void* entry_point_from_quick_compiled_code_;
  } ptr_sized_fields_;

 private:
  bool IsAnnotatedWith(jclass klass, uint32_t visibility);

  static constexpr size_t PtrSizedFieldsOffset(PointerSize pointer_size) {
    // Round up to pointer size for padding field. Tested in art_method.cc.
    return RoundUp(offsetof(ArtMethod, hotness_count_) + sizeof(hotness_count_),
                   static_cast<size_t>(pointer_size));
  }

  // Compare given pointer size to the image pointer size.
  static bool IsImagePointerSize(PointerSize pointer_size);

  template<typename T>
  ALWAYS_INLINE T GetNativePointer(MemberOffset offset, PointerSize pointer_size) const {
    static_assert(std::is_pointer<T>::value, "T must be a pointer type");
    const auto addr = reinterpret_cast<uintptr_t>(this) + offset.Uint32Value();
    if (pointer_size == PointerSize::k32) {
      return reinterpret_cast<T>(*reinterpret_cast<const uint32_t*>(addr));
    } else {
      auto v = *reinterpret_cast<const uint64_t*>(addr);
      return reinterpret_cast<T>(dchecked_integral_cast<uintptr_t>(v));
    }
  }

  template<typename T>
  ALWAYS_INLINE void SetNativePointer(MemberOffset offset, T new_value, PointerSize pointer_size) {
    static_assert(std::is_pointer<T>::value, "T must be a pointer type");
    const auto addr = reinterpret_cast<uintptr_t>(this) + offset.Uint32Value();
    if (pointer_size == PointerSize::k32) {
      uintptr_t ptr = reinterpret_cast<uintptr_t>(new_value);
      *reinterpret_cast<uint32_t*>(addr) = dchecked_integral_cast<uint32_t>(ptr);
    } else {
      *reinterpret_cast<uint64_t*>(addr) = reinterpret_cast<uintptr_t>(new_value);
    }
  }

  DISALLOW_COPY_AND_ASSIGN(ArtMethod);  // Need to use CopyFrom to deal with 32 vs 64 bits.
};

}  // namespace art

#endif  // ART_RUNTIME_ART_METHOD_H_
