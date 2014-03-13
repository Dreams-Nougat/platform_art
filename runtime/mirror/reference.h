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

#ifndef ART_RUNTIME_MIRROR_REFERENCE_H_
#define ART_RUNTIME_MIRROR_REFERENCE_H_

#include "object.h"

namespace art {

struct ReferenceOffsets;
struct FinalizerReferenceOffsets;

namespace mirror {

// C++ mirror of java.lang.ref.Reference
class MANAGED Reference : public Object {
 public:
  static MemberOffset PendingNextOffset() {
    return OFFSET_OF_OBJECT_MEMBER(Reference, pending_next_);
  }
  static MemberOffset QueueOffset() {
    return OFFSET_OF_OBJECT_MEMBER(Reference, queue_);
  }
  static MemberOffset QueueNextOffset() {
    return OFFSET_OF_OBJECT_MEMBER(Reference, queue_next_);
  }
  static MemberOffset ReferentOffset() {
    return OFFSET_OF_OBJECT_MEMBER(Reference, referent_);
  }

  Object* GetReferent() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetFieldObject<Object>(ReferentOffset(), true);
  }
  template<bool kTransactionActive>
  void SetReferent(Object* referent) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    SetFieldObject<kTransactionActive>(ReferentOffset(), referent, true);
  }
  template<bool kTransactionActive>
  void ClearReferent() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    SetFieldObject<kTransactionActive>(ReferentOffset(), nullptr, true);
  }

  Reference* GetPendingNext() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetFieldObject<Reference>(PendingNextOffset(), false);
  }
  template<bool kTransactionActive>
  void SetPendingNext(Reference* pending_next) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    SetFieldObject<kTransactionActive>(PendingNextOffset(), pending_next, false);
  }

  bool IsEnqueued() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    // Since the references are stored as cyclic lists it means that once enqueued, the pending next
    // will always be non-null.
    return GetPendingNext() != nullptr;
  }

  bool IsEnqueuable() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

 private:
  // Field order required by test "ValidateFieldOrderOfJavaCppUnionClasses".
  HeapReference<Reference> pending_next_;  // Note this is Java volatile:
  HeapReference<Reference> queue_;  // Note this is Java volatile:
  HeapReference<Reference> queue_next_;  // Note this is Java volatile:
  HeapReference<Object> referent_;  // Note this is Java volatile:

  friend struct art::ReferenceOffsets;  // for verifying offset information
  DISALLOW_IMPLICIT_CONSTRUCTORS(Reference);
};

// C++ mirror of java.lang.ref.FinalizerReference
class MANAGED FinalizerReference : public Reference {
 public:
  static MemberOffset ZombieOffset() {
    return OFFSET_OF_OBJECT_MEMBER(FinalizerReference, zombie_);
  }

  template<bool kTransactionActive>
  void SetZombie(Object* zombie) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return SetFieldObject<kTransactionActive>(ZombieOffset(), zombie, true);
  }
  Object* GetZombie() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetFieldObject<Object>(ZombieOffset(), true);
  }

 private:
  HeapReference<FinalizerReference> next_;
  HeapReference<FinalizerReference> prev_;
  HeapReference<Object> zombie_;

  friend struct art::FinalizerReferenceOffsets;  // for verifying offset information
  DISALLOW_IMPLICIT_CONSTRUCTORS(FinalizerReference);
};

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_REFERENCE_H_
