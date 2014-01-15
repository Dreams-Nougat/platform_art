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

#include "entrypoints/quick/quick_alloc_entrypoints.h"

#include "callee_save_frame.h"
#include "entrypoints/entrypoint_utils.h"
#include "mirror/art_method-inl.h"
#include "mirror/class-inl.h"
#include "mirror/object_array-inl.h"
#include "mirror/object-inl.h"

namespace art {

#define GENERATE_ENTRYPOINTS_FOR_ALLOCATOR_INST(suffix, suffix2, instrumented_bool, allocator_type) \
extern "C" mirror::Object* artAllocObjectFromCode ##suffix##suffix2( \
    uint32_t type_idx, mirror::ArtMethod* method, Thread* self, \
    StackReference<mirror::ArtMethod>* sp) \
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) { \
  FinishCalleeSaveFrameSetup(self, sp, Runtime::kRefsOnly); \
  return AllocObjectFromCode<false, instrumented_bool>(type_idx, method, self, allocator_type); \
} \
extern "C" mirror::Object* artAllocObjectFromCodeResolved##suffix##suffix2( \
    mirror::Class* klass, mirror::ArtMethod* method, Thread* self, \
    StackReference<mirror::ArtMethod>* sp) \
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) { \
  FinishCalleeSaveFrameSetup(self, sp, Runtime::kRefsOnly); \
  return AllocObjectFromCodeResolved<instrumented_bool>(klass, method, self, allocator_type); \
} \
extern "C" mirror::Object* artAllocObjectFromCodeInitialized##suffix##suffix2( \
    mirror::Class* klass, mirror::ArtMethod* method, Thread* self, \
    StackReference<mirror::ArtMethod>* sp) \
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) { \
  FinishCalleeSaveFrameSetup(self, sp, Runtime::kRefsOnly); \
  return AllocObjectFromCodeInitialized<instrumented_bool>(klass, method, self, allocator_type); \
} \
extern "C" mirror::Object* artAllocObjectFromCodeWithAccessCheck##suffix##suffix2( \
    uint32_t type_idx, mirror::ArtMethod* method, Thread* self, \
    StackReference<mirror::ArtMethod>* sp) \
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) { \
  FinishCalleeSaveFrameSetup(self, sp, Runtime::kRefsOnly); \
  return AllocObjectFromCode<true, instrumented_bool>(type_idx, method, self, allocator_type); \
} \
extern "C" mirror::Array* artAllocArrayFromCode##suffix##suffix2( \
    uint32_t type_idx, mirror::ArtMethod* method, int32_t component_count, Thread* self, \
    StackReference<mirror::ArtMethod>* sp) \
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) { \
  FinishCalleeSaveFrameSetup(self, sp, Runtime::kRefsOnly); \
  return AllocArrayFromCode<false, instrumented_bool>(type_idx, method, component_count, self, \
                                                      allocator_type); \
} \
extern "C" mirror::Array* artAllocArrayFromCodeResolved##suffix##suffix2( \
    mirror::Class* klass, mirror::ArtMethod* method, int32_t component_count, Thread* self, \
    StackReference<mirror::ArtMethod>* sp) \
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) { \
  FinishCalleeSaveFrameSetup(self, sp, Runtime::kRefsOnly); \
  return AllocArrayFromCodeResolved<false, instrumented_bool>(klass, method, component_count, self, \
                                                              allocator_type); \
} \
extern "C" mirror::Array* artAllocArrayFromCodeWithAccessCheck##suffix##suffix2( \
    uint32_t type_idx, mirror::ArtMethod* method, int32_t component_count, Thread* self, \
    StackReference<mirror::ArtMethod>* sp) \
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) { \
  FinishCalleeSaveFrameSetup(self, sp, Runtime::kRefsOnly); \
  return AllocArrayFromCode<true, instrumented_bool>(type_idx, method, component_count, self, \
                                                     allocator_type); \
} \
extern "C" mirror::Array* artCheckAndAllocArrayFromCode##suffix##suffix2( \
    uint32_t type_idx, mirror::ArtMethod* method, int32_t component_count, Thread* self, \
    StackReference<mirror::ArtMethod>* sp) \
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) { \
  FinishCalleeSaveFrameSetup(self, sp, Runtime::kRefsOnly); \
  if (!instrumented_bool) { \
    return CheckAndAllocArrayFromCode(type_idx, method, component_count, self, false, allocator_type); \
  } else { \
    return CheckAndAllocArrayFromCodeInstrumented(type_idx, method, component_count, self, false, allocator_type); \
  } \
} \
extern "C" mirror::Array* artCheckAndAllocArrayFromCodeWithAccessCheck##suffix##suffix2( \
    uint32_t type_idx, mirror::ArtMethod* method, int32_t component_count, Thread* self, \
    StackReference<mirror::ArtMethod>* sp) \
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) { \
  FinishCalleeSaveFrameSetup(self, sp, Runtime::kRefsOnly); \
  if (!instrumented_bool) { \
    return CheckAndAllocArrayFromCode(type_idx, method, component_count, self, true, allocator_type); \
  } else { \
    return CheckAndAllocArrayFromCodeInstrumented(type_idx, method, component_count, self, true, allocator_type); \
  } \
} \
extern "C" mirror::String* artAllocStringFromBytesFromCode##suffix##suffix2( \
    mirror::ByteArray* byte_array, int32_t high, int32_t offset, int32_t byte_count, \
    Thread* self, StackReference<mirror::ArtMethod>* sp) \
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) { \
  FinishCalleeSaveFrameSetup(self, sp, Runtime::kRefsOnly); \
  StackHandleScope<1> hs(self); \
  Handle<mirror::ByteArray> handle_array(hs.NewHandle(byte_array)); \
  return mirror::String::AllocFromByteArray<instrumented_bool>(self, byte_count, handle_array, \
                                                               offset, high, allocator_type); \
} \
extern "C" mirror::String* artAllocStringFromCharsFromCode##suffix##suffix2( \
    int32_t offset, int32_t char_count, mirror::CharArray* char_array, Thread* self, \
    StackReference<mirror::ArtMethod>* sp) \
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) { \
  FinishCalleeSaveFrameSetup(self, sp, Runtime::kRefsOnly); \
  StackHandleScope<1> hs(self); \
  Handle<mirror::CharArray> handle_array(hs.NewHandle(char_array)); \
  return mirror::String::AllocFromCharArray<instrumented_bool>(self, char_count, handle_array, \
                                                               offset, allocator_type); \
} \
extern "C" mirror::String* artAllocStringFromStringFromCode##suffix##suffix2( \
    mirror::String* string, Thread* self, StackReference<mirror::ArtMethod>* sp) \
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) { \
  FinishCalleeSaveFrameSetup(self, sp, Runtime::kRefsOnly); \
  StackHandleScope<1> hs(self); \
  Handle<mirror::String> handle_string(hs.NewHandle(string)); \
  return mirror::String::AllocFromString<instrumented_bool>(self, handle_string->GetCount(), \
                                                            handle_string, 0, allocator_type); \
}

#define GENERATE_ENTRYPOINTS_FOR_ALLOCATOR(suffix, allocator_type) \
    GENERATE_ENTRYPOINTS_FOR_ALLOCATOR_INST(suffix, Instrumented, true, allocator_type) \
    GENERATE_ENTRYPOINTS_FOR_ALLOCATOR_INST(suffix, , false, allocator_type)

GENERATE_ENTRYPOINTS_FOR_ALLOCATOR(DlMalloc, gc::kAllocatorTypeDlMalloc)
GENERATE_ENTRYPOINTS_FOR_ALLOCATOR(RosAlloc, gc::kAllocatorTypeRosAlloc)
GENERATE_ENTRYPOINTS_FOR_ALLOCATOR(BumpPointer, gc::kAllocatorTypeBumpPointer)
GENERATE_ENTRYPOINTS_FOR_ALLOCATOR(TLAB, gc::kAllocatorTypeTLAB)

#define GENERATE_ENTRYPOINTS(suffix) \
extern "C" void* art_quick_alloc_array##suffix(uint32_t, void*, int32_t); \
extern "C" void* art_quick_alloc_array_resolved##suffix(void* klass, void*, int32_t); \
extern "C" void* art_quick_alloc_array_with_access_check##suffix(uint32_t, void*, int32_t); \
extern "C" void* art_quick_alloc_object##suffix(uint32_t type_idx, void* method); \
extern "C" void* art_quick_alloc_object_resolved##suffix(void* klass, void* method); \
extern "C" void* art_quick_alloc_object_initialized##suffix(void* klass, void* method); \
extern "C" void* art_quick_alloc_object_with_access_check##suffix(uint32_t type_idx, void* method); \
extern "C" void* art_quick_check_and_alloc_array##suffix(uint32_t, void*, int32_t); \
extern "C" void* art_quick_check_and_alloc_array_with_access_check##suffix(uint32_t, void*, int32_t); \
extern "C" void* art_quick_alloc_string_from_bytes##suffix(void*, int32_t, int32_t, int32_t); \
extern "C" void* art_quick_alloc_string_from_chars##suffix(int32_t, int32_t, void*); \
extern "C" void* art_quick_alloc_string_from_string##suffix(void*); \
extern "C" void* art_quick_alloc_array##suffix##_instrumented(uint32_t, void*, int32_t); \
extern "C" void* art_quick_alloc_array_resolved##suffix##_instrumented(void* klass, void*, int32_t); \
extern "C" void* art_quick_alloc_array_with_access_check##suffix##_instrumented(uint32_t, void*, int32_t); \
extern "C" void* art_quick_alloc_object##suffix##_instrumented(uint32_t type_idx, void* method); \
extern "C" void* art_quick_alloc_object_resolved##suffix##_instrumented(void* klass, void* method); \
extern "C" void* art_quick_alloc_object_initialized##suffix##_instrumented(void* klass, void* method); \
extern "C" void* art_quick_alloc_object_with_access_check##suffix##_instrumented(uint32_t type_idx, void* method); \
extern "C" void* art_quick_check_and_alloc_array##suffix##_instrumented(uint32_t, void*, int32_t); \
extern "C" void* art_quick_check_and_alloc_array_with_access_check##suffix##_instrumented(uint32_t, void*, int32_t); \
extern "C" void* art_quick_alloc_string_from_bytes##suffix##_instrumented(void*, int32_t, int32_t, int32_t); \
extern "C" void* art_quick_alloc_string_from_chars##suffix##_instrumented(int32_t, int32_t, void*); \
extern "C" void* art_quick_alloc_string_from_string##suffix##_instrumented(void*); \
void SetQuickAllocEntryPoints##suffix(QuickEntryPoints* qpoints, bool instrumented) { \
  if (instrumented) { \
    qpoints->pAllocArray = art_quick_alloc_array##suffix##_instrumented; \
    qpoints->pAllocArrayResolved = art_quick_alloc_array_resolved##suffix##_instrumented; \
    qpoints->pAllocArrayWithAccessCheck = art_quick_alloc_array_with_access_check##suffix##_instrumented; \
    qpoints->pAllocObject = art_quick_alloc_object##suffix##_instrumented; \
    qpoints->pAllocObjectResolved = art_quick_alloc_object_resolved##suffix##_instrumented; \
    qpoints->pAllocObjectInitialized = art_quick_alloc_object_initialized##suffix##_instrumented; \
    qpoints->pAllocObjectWithAccessCheck = art_quick_alloc_object_with_access_check##suffix##_instrumented; \
    qpoints->pCheckAndAllocArray = art_quick_check_and_alloc_array##suffix##_instrumented; \
    qpoints->pCheckAndAllocArrayWithAccessCheck = art_quick_check_and_alloc_array_with_access_check##suffix##_instrumented; \
    qpoints->pAllocStringFromBytes = art_quick_alloc_string_from_bytes##suffix##_instrumented; \
    qpoints->pAllocStringFromChars = art_quick_alloc_string_from_chars##suffix##_instrumented; \
    qpoints->pAllocStringFromString = art_quick_alloc_string_from_string##suffix##_instrumented; \
  } else { \
    qpoints->pAllocArray = art_quick_alloc_array##suffix; \
    qpoints->pAllocArrayResolved = art_quick_alloc_array_resolved##suffix; \
    qpoints->pAllocArrayWithAccessCheck = art_quick_alloc_array_with_access_check##suffix; \
    qpoints->pAllocObject = art_quick_alloc_object##suffix; \
    qpoints->pAllocObjectResolved = art_quick_alloc_object_resolved##suffix; \
    qpoints->pAllocObjectInitialized = art_quick_alloc_object_initialized##suffix; \
    qpoints->pAllocObjectWithAccessCheck = art_quick_alloc_object_with_access_check##suffix; \
    qpoints->pCheckAndAllocArray = art_quick_check_and_alloc_array##suffix; \
    qpoints->pCheckAndAllocArrayWithAccessCheck = art_quick_check_and_alloc_array_with_access_check##suffix; \
    qpoints->pAllocStringFromBytes = art_quick_alloc_string_from_bytes##suffix; \
    qpoints->pAllocStringFromChars = art_quick_alloc_string_from_chars##suffix; \
    qpoints->pAllocStringFromString = art_quick_alloc_string_from_string##suffix; \
  } \
}

// Generate the entrypoint functions.
GENERATE_ENTRYPOINTS(_dlmalloc);
GENERATE_ENTRYPOINTS(_rosalloc);
GENERATE_ENTRYPOINTS(_bump_pointer);
GENERATE_ENTRYPOINTS(_tlab);

static bool entry_points_instrumented = false;
static gc::AllocatorType entry_points_allocator = gc::kAllocatorTypeDlMalloc;

void SetQuickAllocEntryPointsAllocator(gc::AllocatorType allocator) {
  entry_points_allocator = allocator;
}

void SetQuickAllocEntryPointsInstrumented(bool instrumented) {
  entry_points_instrumented = instrumented;
}

void ResetQuickAllocEntryPoints(QuickEntryPoints* qpoints) {
  switch (entry_points_allocator) {
    case gc::kAllocatorTypeDlMalloc: {
      SetQuickAllocEntryPoints_dlmalloc(qpoints, entry_points_instrumented);
      break;
    }
    case gc::kAllocatorTypeRosAlloc: {
      SetQuickAllocEntryPoints_rosalloc(qpoints, entry_points_instrumented);
      break;
    }
    case gc::kAllocatorTypeBumpPointer: {
      CHECK(kMovingCollector);
      SetQuickAllocEntryPoints_bump_pointer(qpoints, entry_points_instrumented);
      break;
    }
    case gc::kAllocatorTypeTLAB: {
      CHECK(kMovingCollector);
      SetQuickAllocEntryPoints_tlab(qpoints, entry_points_instrumented);
      break;
    }
    default: {
      LOG(FATAL) << "Unimplemented";
    }
  }
}

}  // namespace art
