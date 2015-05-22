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

#include "remembered_set.h"

#include <memory>

#include "base/stl_util.h"
#include "bitmap-inl.h"
#include "card_table-inl.h"
#include "heap_bitmap.h"
#include "gc/collector/mark_sweep.h"
#include "gc/collector/mark_sweep-inl.h"
#include "gc/collector/semi_space.h"
#include "gc/heap.h"
#include "gc/space/space.h"
#include "mirror/object-inl.h"
#include "mirror/class-inl.h"
#include "mirror/object_array-inl.h"
#include "space_bitmap-inl.h"
#include "thread.h"

namespace art {
namespace gc {
namespace accounting {

class RememberedSetBitmapVisitor {
 public:
  explicit RememberedSetBitmapVisitor(RememberedSet::CardBitmap* bitmap,
                                      CardTable* card_table)
      : bitmap_(bitmap), card_table_(card_table) {}

  inline void operator()(uint8_t* card, uint8_t expected_value,
                         uint8_t new_value) const {
    UNUSED(new_value);
    if (expected_value == CardTable::kCardDirty) {
      // We want the address the card represents, not the address of the card.
      bitmap_->Set(reinterpret_cast<uintptr_t>(card_table_->AddrFromCard(card)));
    }
  }

 private:
  RememberedSet::CardBitmap* const bitmap_;
  CardTable* const card_table_;
};

void RememberedSet::ClearCards() {
  CardTable* card_table = GetHeap()->GetCardTable();
  RememberedSetBitmapVisitor card_bitmap_visitor(card_bitmap_.get(), card_table);
  // Clear dirty cards in the space and insert them into the dirty card set.
  card_table->ModifyCardsAtomic(space_->Begin(), space_->End(), AgeCardVisitor(), card_bitmap_visitor);
}

class RememberedSetReferenceVisitor {
 public:
  RememberedSetReferenceVisitor(space::ContinuousSpace* target_space,
                                bool* const contains_reference_to_target_space,
                                collector::GarbageCollector* collector)
      : collector_(collector), target_space_(target_space),
        contains_reference_to_target_space_(contains_reference_to_target_space) {}

  void operator()(mirror::Object* obj, MemberOffset offset, bool is_static ATTRIBUTE_UNUSED) const
      SHARED_REQUIRES(Locks::mutator_lock_) {
    DCHECK(obj != nullptr);
    mirror::HeapReference<mirror::Object>* ref_ptr = obj->GetFieldObjectReferenceAddr(offset);
    if (target_space_->HasAddress(ref_ptr->AsMirrorPtr())) {
      *contains_reference_to_target_space_ = true;
      collector_->MarkHeapReference(ref_ptr);
      DCHECK(!target_space_->HasAddress(ref_ptr->AsMirrorPtr()));
    }
  }

  void operator()(mirror::Class* klass, mirror::Reference* ref) const
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(Locks::heap_bitmap_lock_) {
    if (target_space_->HasAddress(ref->GetReferent())) {
      *contains_reference_to_target_space_ = true;
      collector_->DelayReferenceReferent(klass, ref);
    }
  }

  void VisitRootIfNonNull(mirror::CompressedReference<mirror::Object>* root) const
      SHARED_REQUIRES(Locks::mutator_lock_) {
    if (kIsDebugBuild && !root->IsNull()) {
      VisitRoot(root);
    }
  }

  void VisitRoot(mirror::CompressedReference<mirror::Object>* root) const
      SHARED_REQUIRES(Locks::mutator_lock_) {
    DCHECK(!target_space_->HasAddress(root->AsMirrorPtr()));
  }

 private:
  collector::GarbageCollector* const collector_;
  space::ContinuousSpace* const target_space_;
  bool* const contains_reference_to_target_space_;
};

class RememberedSetObjectVisitor {
 public:
  RememberedSetObjectVisitor(space::ContinuousSpace* target_space,
                             bool* const contains_reference_to_target_space,
                             collector::GarbageCollector* collector)
      : collector_(collector), target_space_(target_space),
        contains_reference_to_target_space_(contains_reference_to_target_space) {}

  void operator()(mirror::Object* obj) const REQUIRES(Locks::heap_bitmap_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_) {
    RememberedSetReferenceVisitor visitor(target_space_, contains_reference_to_target_space_,
                                          collector_);
    obj->VisitReferences<kMovingClasses>(visitor, visitor);
  }

 private:
  collector::GarbageCollector* const collector_;
  space::ContinuousSpace* const target_space_;
  bool* const contains_reference_to_target_space_;
};

RememberedSet::RememberedSet(const std::string& name, Heap* heap,
                             space::ContinuousSpace* space)
  : name_(name), heap_(heap), space_(space) {
  // Normally here we could use End() instead of Limit(), but for testing we may want to have a
  // remembered set for a space which can still grow.
  card_bitmap_.reset(CardBitmap::Create(
      "remembered set bitmap", reinterpret_cast<uintptr_t>(space->Begin()),
      RoundUp(reinterpret_cast<uintptr_t>(space->Limit()), CardTable::kCardSize)));
}

class RememberedSetCardBitVisitor {
 public:
  RememberedSetCardBitVisitor(collector::GarbageCollector* collector,
                              space::ContinuousSpace* target_space,
                              space::ContinuousSpace* space,
                              RememberedSet::CardBitmap* card_bitmap)
      : collector_(collector), space_(space), target_space_(target_space),
        bitmap_(space->GetLiveBitmap()), card_bitmap_(card_bitmap) {
  }

  void operator()(size_t bit_index) const {
    const uintptr_t start = card_bitmap_->AddrFromBitIndex(bit_index);
    DCHECK(space_->HasAddress(reinterpret_cast<mirror::Object*>(start)))
        << start << " " << *space_;
    bool contains_reference_to_target_space = false;
    RememberedSetObjectVisitor visitor(target_space_,
                                       &contains_reference_to_target_space,
                                       collector_);
    bitmap_->VisitMarkedRange(start, start + CardTable::kCardSize, visitor);

     if (!contains_reference_to_target_space) {
      // Clear the bit that doesn't contain a reference to the target space.
      card_bitmap_->ClearBit(bit_index);
    }
  }

 private:
  collector::GarbageCollector* const collector_;
  space::ContinuousSpace* const space_;
  space::ContinuousSpace* const target_space_;
  ContinuousSpaceBitmap* const bitmap_;
  RememberedSet::CardBitmap* const card_bitmap_;
};

void RememberedSet::UpdateAndMarkReferences(space::ContinuousSpace* target_space,
                                            collector::GarbageCollector* collector) {
  RememberedSetCardBitVisitor visitor(collector, target_space, space_, card_bitmap_.get());
  card_bitmap_->VisitSetBits(0, RoundUp(space_->Size(), CardTable::kCardSize) / CardTable::kCardSize, visitor);
}

void RememberedSet::Dump(std::ostream& os) {
  os << "RememberedSet dirty cards: [";
  for (uint8_t* addr = space_->Begin(); addr < AlignUp(space_->End(), CardTable::kCardSize);
      addr += CardTable::kCardSize) {
    if (card_bitmap_->Test(reinterpret_cast<uintptr_t>(addr))) {
      os << reinterpret_cast<void*>(addr) << "-"
         << reinterpret_cast<void*>(addr + CardTable::kCardSize) << "\n";
    }
  }
  os << "]";
}

void RememberedSet::SetCards() {
  // Only clean up to the end since there cannot be any objects past the End() of the space.
  for (uint8_t* addr = space_->Begin(); addr < AlignUp(space_->End(), CardTable::kCardSize);
       addr += CardTable::kCardSize) {
    card_bitmap_->Set(reinterpret_cast<uintptr_t>(addr));
  }
}

bool RememberedSet::ContainsCardFor(uintptr_t addr) {
  return card_bitmap_->Test(addr);
}

}  // namespace accounting
}  // namespace gc
}  // namespace art
