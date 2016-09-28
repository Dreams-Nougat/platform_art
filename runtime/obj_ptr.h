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

#ifndef ART_RUNTIME_OBJ_PTR_H_
#define ART_RUNTIME_OBJ_PTR_H_

#include "base/mutex.h"  // For Locks::mutator_lock_.
#include "globals.h"
#include "mirror/object_reference.h"

namespace art {

// Value type representing a pointer to a mirror::Object of type MirrorType
// Pass kPoison as a template boolean for testing in non-debug builds.
// Since the cookie is thread based, it is not safe to share an ObjPtr between threads.
template<class MirrorType, bool kPoison = kIsDebugBuild>
class ObjPtr {
  static constexpr size_t kCookieShift =
      sizeof(mirror::HeapReference<mirror::Object>) * kBitsPerByte - kObjectAlignmentShift;
  static constexpr size_t kCookieBits = sizeof(uintptr_t) * kBitsPerByte - kCookieShift;
  static constexpr uintptr_t kCookieMask = (static_cast<uintptr_t>(1u) << kCookieBits) - 1;

  static_assert(kCookieBits >= kObjectAlignmentShift,
                "must have a least kObjectAlignmentShift bits");

 public:
  ALWAYS_INLINE ObjPtr() REQUIRES_SHARED(Locks::mutator_lock_) : reference_(0u) {}

  ALWAYS_INLINE ObjPtr(std::nullptr_t) REQUIRES_SHARED(Locks::mutator_lock_) : reference_(0u) {}

  template <typename Type>
  ALWAYS_INLINE ObjPtr(Type* ptr) REQUIRES_SHARED(Locks::mutator_lock_)
      : reference_(Encode(static_cast<MirrorType*>(ptr))) {}

  template <typename Type>
  ALWAYS_INLINE ObjPtr(const ObjPtr<Type>& other) REQUIRES_SHARED(Locks::mutator_lock_)
      : reference_(Encode(static_cast<MirrorType*>(other.Decode()))) {}

  template <typename Type>
  ALWAYS_INLINE ObjPtr& operator=(const ObjPtr& other) {
    reference_ = Encode(static_cast<MirrorType*>(other.Decode()));
    return *this;
  }

  ALWAYS_INLINE ObjPtr& operator=(MirrorType* ptr) REQUIRES_SHARED(Locks::mutator_lock_) {
    Assign(ptr);
    return *this;
  }

  ALWAYS_INLINE void Assign(MirrorType* ptr) REQUIRES_SHARED(Locks::mutator_lock_) {
    reference_ = Encode(ptr);
  }

  ALWAYS_INLINE MirrorType* operator->() const REQUIRES_SHARED(Locks::mutator_lock_) {
    return Decode();
  }

  ALWAYS_INLINE bool IsNull() const {
    return reference_ == 0;
  }

  // Decode makes sure that the object pointer is valid.
  ALWAYS_INLINE MirrorType* Decode() const REQUIRES_SHARED(Locks::mutator_lock_) {
    AssertValid();
    return DecodeUnchecked();
  }

  ALWAYS_INLINE bool IsValid() const REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE void AssertValid() const REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE bool operator==(const ObjPtr& ptr) const REQUIRES_SHARED(Locks::mutator_lock_) {
    return Decode() == ptr.Decode();
  }

  ALWAYS_INLINE bool operator==(const MirrorType* ptr) const REQUIRES_SHARED(Locks::mutator_lock_) {
    return Decode() == ptr;
  }

  ALWAYS_INLINE bool operator==(std::nullptr_t) const REQUIRES_SHARED(Locks::mutator_lock_) {
    return IsNull();
  }

  ALWAYS_INLINE bool operator!=(const ObjPtr& ptr) const REQUIRES_SHARED(Locks::mutator_lock_) {
    return Decode() != ptr.Decode();
  }

  ALWAYS_INLINE bool operator!=(const MirrorType* ptr) const REQUIRES_SHARED(Locks::mutator_lock_) {
    return Decode() != ptr;
  }

  ALWAYS_INLINE bool operator!=(std::nullptr_t) const REQUIRES_SHARED(Locks::mutator_lock_) {
    return !IsNull();
  }

 private:
  // Trim off high bits of thread local cookie.
  ALWAYS_INLINE static uintptr_t TrimCookie(uintptr_t cookie) {
    return cookie & kCookieMask;
  }

  ALWAYS_INLINE uintptr_t GetCookie() const {
    return reference_ >> kCookieShift;
  }

  // Decode makes sure that the object pointer is valid.
  ALWAYS_INLINE MirrorType* DecodeUnchecked() const REQUIRES_SHARED(Locks::mutator_lock_) {
    if (kPoison) {
      return reinterpret_cast<MirrorType*>(
          static_cast<uintptr_t>(static_cast<uint32_t>(reference_ << kObjectAlignmentShift)));
    } else {
      return reinterpret_cast<MirrorType*>(reference_);
    }
  }

  ALWAYS_INLINE static uintptr_t Encode(MirrorType* ptr) REQUIRES_SHARED(Locks::mutator_lock_);
  // The encoded reference and cookie.
  uintptr_t reference_;
};

template<class MirrorType, bool kPoison = kIsDebugBuild>
static inline ObjPtr<MirrorType, kPoison> MakeObjPtr(MirrorType* ptr) {
  return ObjPtr<MirrorType, kPoison>(ptr);
}

}  // namespace art

#endif  // ART_RUNTIME_OBJ_PTR_H_
