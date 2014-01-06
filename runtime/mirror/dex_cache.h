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

#ifndef ART_RUNTIME_MIRROR_DEX_CACHE_H_
#define ART_RUNTIME_MIRROR_DEX_CACHE_H_

#include "art_method.h"
#include "class.h"
#include "object.h"
#include "object_array.h"
#include "string.h"

namespace art {

struct DexCacheOffsets;
class DexFile;
class ImageWriter;
union JValue;

namespace mirror {

class ArtField;
class Class;

class MANAGED DexCacheClass : public Class {
 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(DexCacheClass);
};

class MANAGED DexCache : public Object {
 public:
  void Init(const DexFile* dex_file,
            String* location,
            ObjectArray<String>* strings,
            ObjectArray<Class>* types,
            ObjectArray<ArtMethod>* methods,
            ObjectArray<ArtField>* fields)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void Fixup(ArtMethod* trampoline) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  String* GetLocation() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetFieldObject<String>(OFFSET_OF_OBJECT_MEMBER(DexCache, location_), false);
  }

  static MemberOffset StringsOffset() {
    return OFFSET_OF_OBJECT_MEMBER(DexCache, strings_);
  }

  static MemberOffset ResolvedFieldsOffset() {
    return OFFSET_OF_OBJECT_MEMBER(DexCache, resolved_fields_);
  }

  static MemberOffset ResolvedMethodsOffset() {
    return OFFSET_OF_OBJECT_MEMBER(DexCache, resolved_methods_);
  }

  size_t NumStrings() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetStrings()->GetLength();
  }

  size_t NumResolvedTypes() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetResolvedTypes()->GetLength();
  }

  size_t NumResolvedMethods() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetResolvedMethods()->GetLength();
  }

  size_t NumResolvedFields() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetResolvedFields()->GetLength();
  }

  String* GetResolvedString(uint32_t string_idx) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetStrings()->Get(string_idx);
  }

  void SetResolvedString(uint32_t string_idx, String* resolved)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    GetStrings()->Set(string_idx, resolved);
  }

  Class* GetResolvedType(uint32_t type_idx) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetResolvedTypes()->Get(type_idx);
  }

  void SetResolvedType(uint32_t type_idx, Class* resolved)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    GetResolvedTypes()->Set(type_idx, resolved);
  }

  ArtMethod* GetResolvedMethod(uint32_t method_idx) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void SetResolvedMethod(uint32_t method_idx, ArtMethod* resolved)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    GetResolvedMethods()->Set(method_idx, resolved);
  }

  ArtField* GetResolvedField(uint32_t field_idx) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetResolvedFields()->Get(field_idx);
  }

  void SetResolvedField(uint32_t field_idx, ArtField* resolved)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    GetResolvedFields()->Set(field_idx, resolved);
  }

  ObjectArray<String>* GetStrings() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetFieldObject< ObjectArray<String> >(StringsOffset(), false);
  }

  ObjectArray<Class>* GetResolvedTypes() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetFieldObject<ObjectArray<Class> >(
        OFFSET_OF_OBJECT_MEMBER(DexCache, resolved_types_), false);
  }

  ObjectArray<ArtMethod>* GetResolvedMethods() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetFieldObject< ObjectArray<ArtMethod> >(ResolvedMethodsOffset(), false);
  }

  ObjectArray<ArtField>* GetResolvedFields() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetFieldObject<ObjectArray<ArtField> >(ResolvedFieldsOffset(), false);
  }

  const DexFile* GetDexFile() {
    return GetFieldPtr<const DexFile*>(OFFSET_OF_OBJECT_MEMBER(DexCache, dex_file_), false);
  }

  void SetDexFile(const DexFile* dex_file) {
    return SetFieldPtr(OFFSET_OF_OBJECT_MEMBER(DexCache, dex_file_), dex_file, false);
  }

 private:
  HeapReference<Object> dex_;
  HeapReference<String> location_;
  HeapReference<ObjectArray<ArtField> > resolved_fields_;
  HeapReference<ObjectArray<ArtMethod> > resolved_methods_;
  HeapReference<ObjectArray<Class> > resolved_types_;
  HeapReference<ObjectArray<String> > strings_;
  uint64_t dex_file_;

  friend struct art::DexCacheOffsets;  // for verifying offset information
  DISALLOW_IMPLICIT_CONSTRUCTORS(DexCache);
};

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_DEX_CACHE_H_
