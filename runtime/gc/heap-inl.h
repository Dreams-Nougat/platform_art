/*
 * Copyright (C) 2013 The Android Open Source Project
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

#ifndef ART_RUNTIME_GC_HEAP_INL_H_
#define ART_RUNTIME_GC_HEAP_INL_H_

#include "heap.h"

#include "base/time_utils.h"
#include "gc/accounting/card_table-inl.h"
#include "gc/allocation_record.h"
#include "gc/collector/semi_space.h"
#include "gc/space/bump_pointer_space-inl.h"
#include "gc/space/dlmalloc_space-inl.h"
#include "gc/space/large_object_space.h"
#include "gc/space/region_space-inl.h"
#include "gc/space/rosalloc_space-inl.h"
#include "runtime.h"
#include "handle_scope-inl.h"
#include "thread-inl.h"
#include "utils.h"
#include "verify_object-inl.h"

namespace art {
namespace gc {

template <bool kInstrumented, bool kCheckLargeObject, typename PreFenceVisitor>
inline mirror::Object* Heap::AllocObjectWithAllocator(Thread* self,
                                                      mirror::Class* klass,
                                                      size_t byte_count,
                                                      AllocatorType allocator,
                                                      const PreFenceVisitor& pre_fence_visitor) {
  if (kIsDebugBuild) {
    CheckPreconditionsForAllocObject(klass, byte_count);
    // Since allocation can cause a GC which will need to SuspendAll, make sure all allocations are
    // done in the runnable state where suspension is expected.
    CHECK_EQ(self->GetState(), kRunnable);
    self->AssertThreadSuspensionIsAllowable();
  }
  // Need to check that we arent the large object allocator since the large object allocation code
  // path this function. If we didn't check we would have an infinite loop.
  if (kCheckLargeObject && UNLIKELY(ShouldAllocLargeObject(klass, byte_count))) {
    mirror::Object* obj = AllocLargeObject<kInstrumented, PreFenceVisitor>(
        self, &klass, byte_count, pre_fence_visitor);
    if (obj != nullptr) {
      return obj;
    } else {
      // There should be an OOM exception, since we are retrying, clear it.
      self->ClearException();
    }
    // If the large object allocation failed, try to use the normal spaces (main space,
    // non moving space). This can happen if there is significant virtual address space
    // fragmentation.
  }
  // bytes allocated for the (individual) object.
  AllocationResults results;
  mirror::Object*& obj = results.object_allocated;
  size_t& bytes_allocated = results.bytes_allocated;
  size_t& usable_size = results.usable_size;
  size_t new_num_bytes_allocated = 0;
  if (allocator == kAllocatorTypeTLAB || allocator == kAllocatorTypeRegionTLAB) {
    byte_count = RoundUp(byte_count, space::BumpPointerSpace::kAlignment);
  }
  // If we have a thread local allocation we don't need to update bytes allocated.
  if ((allocator == kAllocatorTypeTLAB || allocator == kAllocatorTypeRegionTLAB) &&
      byte_count <= self->TlabSize()) {
    obj = self->AllocTlab(byte_count);
    DCHECK(obj != nullptr) << "AllocTlab can't fail";
    obj->SetClass(klass);
    if (kUseBakerOrBrooksReadBarrier) {
      if (kUseBrooksReadBarrier) {
        obj->SetReadBarrierPointer(obj);
      }
      obj->AssertReadBarrierPointer();
    }
    bytes_allocated = byte_count;
    usable_size = bytes_allocated;
    pre_fence_visitor(obj, usable_size);
    QuasiAtomic::ThreadFenceForConstructor();
  } else if (!kInstrumented && allocator == kAllocatorTypeRosAlloc &&
             (obj = rosalloc_space_->AllocThreadLocal(self, byte_count, &bytes_allocated)) &&
             LIKELY(obj != nullptr)) {
    DCHECK(!is_running_on_memory_tool_);
    obj->SetClass(klass);
    if (kUseBakerOrBrooksReadBarrier) {
      if (kUseBrooksReadBarrier) {
        obj->SetReadBarrierPointer(obj);
      }
      obj->AssertReadBarrierPointer();
    }
    usable_size = bytes_allocated;
    pre_fence_visitor(obj, usable_size);
    QuasiAtomic::ThreadFenceForConstructor();
  } else {
    // bytes allocated that takes bulk thread-local buffer allocations into account.
    size_t& bytes_tl_bulk_allocated = results.bytes_tl_bulk_allocated;
    if (!TryToAllocate<kInstrumented, false>(self, allocator, byte_count, &results)) {
      bool is_current_allocator = allocator == GetCurrentAllocator();
      if (!AllocateInternalWithGc(self, allocator, byte_count, &results, &klass)) {
        bool after_is_current_allocator = allocator == GetCurrentAllocator();
        // If there is a pending exception, fail the allocation right away since the next one
        // could cause OOM and abort the runtime.
        if (!self->IsExceptionPending() && is_current_allocator && !after_is_current_allocator) {
          // If the allocator changed, we need to restart the allocation.
          return AllocObject<kInstrumented>(self, klass, byte_count, pre_fence_visitor);
        }
        return nullptr;
      }
    }
    DCHECK(obj != nullptr);
    DCHECK_GT(bytes_allocated, 0u);
    DCHECK_GT(usable_size, 0u);

    obj->SetClass(klass);
    if (kUseBakerOrBrooksReadBarrier) {
      if (kUseBrooksReadBarrier) {
        obj->SetReadBarrierPointer(obj);
      }
      obj->AssertReadBarrierPointer();
    }
    if (collector::SemiSpace::kUseRememberedSet && UNLIKELY(allocator == kAllocatorTypeNonMoving)) {
      // (Note this if statement will be constant folded away for the
      // fast-path quick entry points.) Because SetClass() has no write
      // barrier, if a non-moving space allocation, we need a write
      // barrier as the class pointer may point to the bump pointer
      // space (where the class pointer is an "old-to-young" reference,
      // though rare) under the GSS collector with the remembered set
      // enabled. We don't need this for kAllocatorTypeRosAlloc/DlMalloc
      // cases because we don't directly allocate into the main alloc
      // space (besides promotions) under the SS/GSS collector.
      WriteBarrierField(obj, mirror::Object::ClassOffset(), klass);
    }
    pre_fence_visitor(obj, usable_size);
    QuasiAtomic::ThreadFenceForConstructor();
    new_num_bytes_allocated = static_cast<size_t>(
        num_bytes_allocated_.FetchAndAddRelaxed(bytes_tl_bulk_allocated)) + bytes_tl_bulk_allocated;
  }
  if (kIsDebugBuild && Runtime::Current()->IsStarted()) {
    CHECK_LE(obj->SizeOf(), usable_size);
  }
  // TODO: Deprecate.
  if (kInstrumented) {
    if (Runtime::Current()->HasStatsEnabled()) {
      RuntimeStats* thread_stats = self->GetStats();
      ++thread_stats->allocated_objects;
      thread_stats->allocated_bytes += bytes_allocated;
      RuntimeStats* global_stats = Runtime::Current()->GetStats();
      ++global_stats->allocated_objects;
      global_stats->allocated_bytes += bytes_allocated;
    }
  } else {
    DCHECK(!Runtime::Current()->HasStatsEnabled());
  }
  if (AllocatorHasAllocationStack(allocator)) {
    PushOnAllocationStack(self, &obj);
  }
  if (kInstrumented) {
    if (IsAllocTrackingEnabled()) {
      // Use obj->GetClass() instead of klass, because PushOnAllocationStack() could move klass
      AllocRecordObjectMap::RecordAllocation(self, obj, obj->GetClass(), bytes_allocated);
    }
  } else {
    DCHECK(!IsAllocTrackingEnabled());
  }
  if (kInstrumented) {
    if (gc_stress_mode_) {
      CheckGcStressMode(self, &obj);
    }
  } else {
    DCHECK(!gc_stress_mode_);
  }
  // IsConcurrentGc() isn't known at compile time so we can optimize by not checking it for
  // the BumpPointer or TLAB allocators. This is nice since it allows the entire if statement to be
  // optimized out. And for the other allocators, AllocatorMayHaveConcurrentGC is a constant since
  // the allocator_type should be constant propagated.
  if (AllocatorMayHaveConcurrentGC(allocator) && IsGcConcurrent()) {
    CheckConcurrentGC(self, new_num_bytes_allocated, &obj);
  }
  VerifyObject(obj);
  self->VerifyStack();
  return obj;
}

// The size of a thread-local allocation stack in the number of references.
static constexpr size_t kThreadLocalAllocationStackSize = 128;

inline void Heap::PushOnAllocationStack(Thread* self, mirror::Object** obj) {
  if (kUseThreadLocalAllocationStack) {
    if (UNLIKELY(!self->PushOnThreadLocalAllocationStack(*obj))) {
      PushOnThreadLocalAllocationStackWithInternalGC(self, obj);
    }
  } else if (UNLIKELY(!allocation_stack_->AtomicPushBack(*obj))) {
    PushOnAllocationStackWithInternalGC(self, obj);
  }
}

template <bool kInstrumented, typename PreFenceVisitor>
inline mirror::Object* Heap::AllocLargeObject(Thread* self,
                                              mirror::Class** klass,
                                              size_t byte_count,
                                              const PreFenceVisitor& pre_fence_visitor) {
  // Save and restore the class in case it moves.
  StackHandleScope<1> hs(self);
  auto klass_wrapper = hs.NewHandleWrapper(klass);
  return AllocObjectWithAllocator<kInstrumented, false, PreFenceVisitor>(self, *klass, byte_count,
                                                                         kAllocatorTypeLOS,
                                                                         pre_fence_visitor);
}

template <const bool kInstrumented, const bool kGrow>
inline bool Heap::TryToAllocate(Thread* self,
                                AllocatorType allocator_type,
                                size_t alloc_size,
                                AllocationResults* results) {
  if (allocator_type != kAllocatorTypeTLAB &&
      allocator_type != kAllocatorTypeRegionTLAB &&
      allocator_type != kAllocatorTypeRosAlloc &&
      UNLIKELY(IsOutOfMemoryOnAllocation<kGrow>(alloc_size))) {
    return false;
  }
  switch (allocator_type) {
    case kAllocatorTypeBumpPointer: {
      DCHECK(bump_pointer_space_ != nullptr);
      alloc_size = RoundUp(alloc_size, space::BumpPointerSpace::kAlignment);
      results->object_allocated = bump_pointer_space_->AllocNonvirtual(alloc_size);
      if (LIKELY(results->object_allocated != nullptr)) {
        results->bytes_allocated = alloc_size;
        results->usable_size = alloc_size;
        results->bytes_tl_bulk_allocated = alloc_size;
        return true;
      }
      return false;
    }
    case kAllocatorTypeRosAlloc: {
      if (kInstrumented && UNLIKELY(is_running_on_memory_tool_)) {
        // If running on valgrind or asan, we should be using the instrumented path.
        size_t max_bytes_tl_bulk_allocated = rosalloc_space_->MaxBytesBulkAllocatedFor(alloc_size);
        if (UNLIKELY(IsOutOfMemoryOnAllocation<kGrow>(max_bytes_tl_bulk_allocated))) {
          return false;
        }
        results->object_allocated = rosalloc_space_->Alloc(self, alloc_size,
                                              &(results->bytes_allocated),
                                              &(results->usable_size),
                                              &(results->bytes_tl_bulk_allocated));
        return results->object_allocated != nullptr;
      } else {
        DCHECK(!is_running_on_memory_tool_);
        size_t max_bytes_tl_bulk_allocated =
            rosalloc_space_->MaxBytesBulkAllocatedForNonvirtual(alloc_size);
        if (UNLIKELY(IsOutOfMemoryOnAllocation<kGrow>(max_bytes_tl_bulk_allocated))) {
          return false;
        }
        if (!kInstrumented) {
          DCHECK(!rosalloc_space_->CanAllocThreadLocal(self, alloc_size));
        }
        results->object_allocated = rosalloc_space_->AllocNonvirtual(self, alloc_size,
            &(results->bytes_allocated),
            &(results->usable_size),
            &(results->bytes_tl_bulk_allocated));
        return results->object_allocated != nullptr;
      }
    }
    case kAllocatorTypeDlMalloc: {
      if (kInstrumented && UNLIKELY(is_running_on_memory_tool_)) {
        // If running on valgrind, we should be using the instrumented path.
        results->object_allocated = dlmalloc_space_->Alloc(self, alloc_size,
            &(results->bytes_allocated),
            &(results->usable_size),
            &(results->bytes_tl_bulk_allocated));
        return results->object_allocated != nullptr;
      } else {
        DCHECK(!is_running_on_memory_tool_);
        results->object_allocated = dlmalloc_space_->AllocNonvirtual(self, alloc_size,
            &(results->bytes_allocated),
            &(results->usable_size),
            &(results->bytes_tl_bulk_allocated));
        return results->object_allocated != nullptr;
      }
    }
    case kAllocatorTypeNonMoving: {
      results->object_allocated = non_moving_space_->Alloc(self, alloc_size,
          &(results->bytes_allocated),
          &(results->usable_size),
          &(results->bytes_tl_bulk_allocated));
      return results->object_allocated != nullptr;
    }
    case kAllocatorTypeLOS: {
      results->object_allocated = large_object_space_->Alloc(self, alloc_size,
          &(results->bytes_allocated),
          &(results->usable_size),
          &(results->bytes_tl_bulk_allocated));
      // Note that the bump pointer spaces aren't necessarily next to
      // the other continuous spaces like the non-moving alloc space or
      // the zygote space.
      DCHECK(results->object_allocated == nullptr
          || large_object_space_->Contains(results->object_allocated));
      return results->object_allocated != nullptr;
    }
    case kAllocatorTypeTLAB: {
      DCHECK_ALIGNED(alloc_size, space::BumpPointerSpace::kAlignment);
      if (UNLIKELY(self->TlabSize() < alloc_size)) {
        const size_t new_tlab_size = alloc_size + kDefaultTLABSize;
        if (UNLIKELY(IsOutOfMemoryOnAllocation<kGrow>(new_tlab_size))) {
          return false;
        }
        // Try allocating a new thread local buffer, if the allocaiton fails the space must be
        // full so return null.
        if (!bump_pointer_space_->AllocNewTlab(self, new_tlab_size)) {
          return false;
        }
        results->bytes_tl_bulk_allocated = new_tlab_size;
      } else {
        results->bytes_tl_bulk_allocated = 0;
      }
      // The allocation can't fail.
      results->object_allocated = self->AllocTlab(alloc_size);
      DCHECK(results->object_allocated != nullptr);
      results->bytes_allocated = alloc_size;
      results->usable_size = alloc_size;
      return results->object_allocated != nullptr;
    }
    case kAllocatorTypeRegion: {
      DCHECK(region_space_ != nullptr);
      alloc_size = RoundUp(alloc_size, space::RegionSpace::kAlignment);
      results->object_allocated = region_space_->AllocNonvirtual<false>(alloc_size,
          &(results->bytes_allocated),
          &(results->usable_size),
          &(results->bytes_tl_bulk_allocated));
      return results->object_allocated != nullptr;
    }
    case kAllocatorTypeRegionTLAB: {
      DCHECK(region_space_ != nullptr);
      DCHECK_ALIGNED(alloc_size, space::RegionSpace::kAlignment);
      if (UNLIKELY(self->TlabSize() < alloc_size)) {
        if (space::RegionSpace::kRegionSize >= alloc_size) {
          // Non-large. Check OOME for a tlab.
          if (LIKELY(!IsOutOfMemoryOnAllocation<kGrow>(space::RegionSpace::kRegionSize))) {
            // Try to allocate a tlab.
            if (!region_space_->AllocNewTlab(self)) {
              // Failed to allocate a tlab. Try non-tlab.
              results->object_allocated = region_space_->AllocNonvirtual<false>(alloc_size,
                  &(results->bytes_allocated),
                  &(results->usable_size),
                  &(results->bytes_tl_bulk_allocated));
              return results->object_allocated != nullptr;
            }
            results->bytes_tl_bulk_allocated = space::RegionSpace::kRegionSize;
            // Fall-through.
          } else {
            // Check OOME for a non-tlab allocation.
            if (!IsOutOfMemoryOnAllocation<kGrow>(alloc_size)) {
              results->object_allocated = region_space_->AllocNonvirtual<false>(alloc_size,
                  &(results->bytes_allocated),
                  &(results->usable_size),
                  &(results->bytes_tl_bulk_allocated));
              return results->object_allocated != nullptr;
            } else {
              // Neither tlab or non-tlab works. Give up.
              return false;
            }
          }
        } else {
          // Large. Check OOME.
          if (LIKELY(!IsOutOfMemoryOnAllocation<kGrow>(alloc_size))) {
            results->object_allocated = region_space_->AllocNonvirtual<false>(alloc_size,
                &(results->bytes_allocated),
                &(results->usable_size),
                &(results->bytes_tl_bulk_allocated));
            return results->object_allocated != nullptr;
          } else {
            return false;
          }
        }
      } else {
        results->bytes_tl_bulk_allocated = 0;  // Allocated in an existing buffer.
      }
      // The allocation can't fail.
      results->object_allocated = self->AllocTlab(alloc_size);
      DCHECK(results->object_allocated != nullptr);
      results->bytes_allocated = alloc_size;
      results->usable_size = alloc_size;
      return results->object_allocated != nullptr;
    }
    case kAllocatorTypeNull: {
      return true;
    }
    default: {
      LOG(FATAL) << "Invalid allocator type";
      return false;
    }
  }
  return false;
}

inline bool Heap::ShouldAllocLargeObject(mirror::Class* c, size_t byte_count) const {
  // We need to have a zygote space or else our newly allocated large object can end up in the
  // Zygote resulting in it being prematurely freed.
  // We can only do this for primitive objects since large objects will not be within the card table
  // range. This also means that we rely on SetClass not dirtying the object's card.
  return byte_count >= large_object_threshold_ && (c->IsPrimitiveArray() || c->IsStringClass());
}

template <bool kGrow>
inline bool Heap::IsOutOfMemoryOnAllocation(size_t alloc_size) {
  size_t new_footprint = num_bytes_allocated_.LoadSequentiallyConsistent() + alloc_size;
  if (UNLIKELY(new_footprint > max_allowed_footprint_)) {
    if (UNLIKELY(new_footprint > growth_limit_)) {
      return true;
    }
    if (!IsGcConcurrent()) {
      if (!kGrow) {
        return true;
      }
      // TODO: Grow for allocation is racy, fix it.
      VLOG(heap) << "Growing heap from " << PrettySize(max_allowed_footprint_) << " to "
          << PrettySize(new_footprint) << " for a " << PrettySize(alloc_size) << " allocation";
      max_allowed_footprint_ = new_footprint;
    }
  }
  return false;
}

inline void Heap::CheckConcurrentGC(Thread* self,
                                    size_t new_num_bytes_allocated,
                                    mirror::Object** obj) {
  if (UNLIKELY(new_num_bytes_allocated >= concurrent_start_bytes_)) {
    RequestConcurrentGCAndSaveObject(self, false, obj);
  }
}

inline void Heap::CheckConcurrentGC(Thread* self,
                                    size_t new_num_bytes_allocated) {
  if (UNLIKELY(new_num_bytes_allocated >= concurrent_start_bytes_)) {
    RequestConcurrentGC(self, false);
  }
}

}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_HEAP_INL_H_
