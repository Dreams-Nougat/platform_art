
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

#ifndef ART_RUNTIME_GENERATED_ASM_SUPPORT_GEN_H_
#define ART_RUNTIME_GENERATED_ASM_SUPPORT_GEN_H_

// This file has been auto-generated by cpp-define-generator; do not edit directly.

#define STACK_REFERENCE_SIZE 0x4
DEFINE_CHECK_EQ(static_cast<size_t>(STACK_REFERENCE_SIZE), (static_cast<size_t>(sizeof(art::StackReference<art::mirror::Object>))))
#define COMPRESSED_REFERENCE_SIZE 0x4
DEFINE_CHECK_EQ(static_cast<size_t>(COMPRESSED_REFERENCE_SIZE), (static_cast<size_t>(sizeof(art::mirror::CompressedReference<art::mirror::Object>))))
#define COMPRESSED_REFERENCE_SIZE_SHIFT 0x2
DEFINE_CHECK_EQ(static_cast<size_t>(COMPRESSED_REFERENCE_SIZE_SHIFT), (static_cast<size_t>(art::WhichPowerOf2(sizeof(art::mirror::CompressedReference<art::mirror::Object>)))))
#define RUNTIME_SAVE_ALL_CALLEE_SAVES_METHOD_OFFSET 0
DEFINE_CHECK_EQ(static_cast<size_t>(RUNTIME_SAVE_ALL_CALLEE_SAVES_METHOD_OFFSET), (static_cast<size_t>(art::Runtime::GetCalleeSaveMethodOffset(art::Runtime:: kSaveAllCalleeSaves))))
#define RUNTIME_SAVE_REFS_ONLY_METHOD_OFFSET 0x8
DEFINE_CHECK_EQ(static_cast<size_t>(RUNTIME_SAVE_REFS_ONLY_METHOD_OFFSET), (static_cast<size_t>(art::Runtime::GetCalleeSaveMethodOffset(art::Runtime:: kSaveRefsOnly))))
#define RUNTIME_SAVE_REFS_AND_ARGS_METHOD_OFFSET 0x10
DEFINE_CHECK_EQ(static_cast<size_t>(RUNTIME_SAVE_REFS_AND_ARGS_METHOD_OFFSET), (static_cast<size_t>(art::Runtime::GetCalleeSaveMethodOffset(art::Runtime:: kSaveRefsAndArgs))))
#define RUNTIME_SAVE_EVERYTHING_METHOD_OFFSET 0x18
DEFINE_CHECK_EQ(static_cast<size_t>(RUNTIME_SAVE_EVERYTHING_METHOD_OFFSET), (static_cast<size_t>(art::Runtime::GetCalleeSaveMethodOffset(art::Runtime:: kSaveEverything))))
#define THREAD_FLAGS_OFFSET 0
DEFINE_CHECK_EQ(static_cast<int32_t>(THREAD_FLAGS_OFFSET), (static_cast<int32_t>(art::Thread:: ThreadFlagsOffset<art::kRuntimePointerSize>().Int32Value())))
#define THREAD_ID_OFFSET 12
DEFINE_CHECK_EQ(static_cast<int32_t>(THREAD_ID_OFFSET), (static_cast<int32_t>(art::Thread:: ThinLockIdOffset<art::kRuntimePointerSize>().Int32Value())))
#define THREAD_IS_GC_MARKING_OFFSET 52
DEFINE_CHECK_EQ(static_cast<int32_t>(THREAD_IS_GC_MARKING_OFFSET), (static_cast<int32_t>(art::Thread:: IsGcMarkingOffset<art::kRuntimePointerSize>().Int32Value())))
#define THREAD_CARD_TABLE_OFFSET 128
DEFINE_CHECK_EQ(static_cast<int32_t>(THREAD_CARD_TABLE_OFFSET), (static_cast<int32_t>(art::Thread:: CardTableOffset<art::kRuntimePointerSize>().Int32Value())))
#define CODEITEM_INSNS_OFFSET 16
DEFINE_CHECK_EQ(static_cast<int32_t>(CODEITEM_INSNS_OFFSET), (static_cast<int32_t>(__builtin_offsetof(art::DexFile::CodeItem, insns_))))
#define MIRROR_OBJECT_CLASS_OFFSET 0
DEFINE_CHECK_EQ(static_cast<int32_t>(MIRROR_OBJECT_CLASS_OFFSET), (static_cast<int32_t>(art::mirror::Object:: ClassOffset().Int32Value())))
#define MIRROR_OBJECT_LOCK_WORD_OFFSET 4
DEFINE_CHECK_EQ(static_cast<int32_t>(MIRROR_OBJECT_LOCK_WORD_OFFSET), (static_cast<int32_t>(art::mirror::Object:: MonitorOffset().Int32Value())))
#define MIRROR_CLASS_STATUS_INITIALIZED 0xa
DEFINE_CHECK_EQ(static_cast<uint32_t>(MIRROR_CLASS_STATUS_INITIALIZED), (static_cast<uint32_t>((art::mirror::Class::kStatusInitialized))))
#define ACCESS_FLAGS_CLASS_IS_FINALIZABLE 0x80000000
DEFINE_CHECK_EQ(static_cast<uint32_t>(ACCESS_FLAGS_CLASS_IS_FINALIZABLE), (static_cast<uint32_t>((art::kAccClassIsFinalizable))))
#define ACCESS_FLAGS_CLASS_IS_INTERFACE 0x200
DEFINE_CHECK_EQ(static_cast<uint32_t>(ACCESS_FLAGS_CLASS_IS_INTERFACE), (static_cast<uint32_t>((art::kAccInterface))))
#define ACCESS_FLAGS_CLASS_IS_FINALIZABLE_BIT 0x1f
DEFINE_CHECK_EQ(static_cast<uint32_t>(ACCESS_FLAGS_CLASS_IS_FINALIZABLE_BIT), (static_cast<uint32_t>((art::MostSignificantBit(art::kAccClassIsFinalizable)))))
#define ART_METHOD_DEX_CACHE_METHODS_OFFSET_32 20
DEFINE_CHECK_EQ(static_cast<int32_t>(ART_METHOD_DEX_CACHE_METHODS_OFFSET_32), (static_cast<int32_t>(art::ArtMethod:: DexCacheResolvedMethodsOffset(art::PointerSize::k32).Int32Value())))
#define ART_METHOD_DEX_CACHE_METHODS_OFFSET_64 24
DEFINE_CHECK_EQ(static_cast<int32_t>(ART_METHOD_DEX_CACHE_METHODS_OFFSET_64), (static_cast<int32_t>(art::ArtMethod:: DexCacheResolvedMethodsOffset(art::PointerSize::k64).Int32Value())))
#define ART_METHOD_DEX_CACHE_TYPES_OFFSET_32 24
DEFINE_CHECK_EQ(static_cast<int32_t>(ART_METHOD_DEX_CACHE_TYPES_OFFSET_32), (static_cast<int32_t>(art::ArtMethod:: DexCacheResolvedTypesOffset(art::PointerSize::k32).Int32Value())))
#define ART_METHOD_DEX_CACHE_TYPES_OFFSET_64 32
DEFINE_CHECK_EQ(static_cast<int32_t>(ART_METHOD_DEX_CACHE_TYPES_OFFSET_64), (static_cast<int32_t>(art::ArtMethod:: DexCacheResolvedTypesOffset(art::PointerSize::k64).Int32Value())))
#define ART_METHOD_JNI_OFFSET_32 28
DEFINE_CHECK_EQ(static_cast<int32_t>(ART_METHOD_JNI_OFFSET_32), (static_cast<int32_t>(art::ArtMethod:: EntryPointFromJniOffset(art::PointerSize::k32).Int32Value())))
#define ART_METHOD_JNI_OFFSET_64 40
DEFINE_CHECK_EQ(static_cast<int32_t>(ART_METHOD_JNI_OFFSET_64), (static_cast<int32_t>(art::ArtMethod:: EntryPointFromJniOffset(art::PointerSize::k64).Int32Value())))
#define ART_METHOD_QUICK_CODE_OFFSET_32 32
DEFINE_CHECK_EQ(static_cast<int32_t>(ART_METHOD_QUICK_CODE_OFFSET_32), (static_cast<int32_t>(art::ArtMethod:: EntryPointFromQuickCompiledCodeOffset(art::PointerSize::k32).Int32Value())))
#define ART_METHOD_QUICK_CODE_OFFSET_64 48
DEFINE_CHECK_EQ(static_cast<int32_t>(ART_METHOD_QUICK_CODE_OFFSET_64), (static_cast<int32_t>(art::ArtMethod:: EntryPointFromQuickCompiledCodeOffset(art::PointerSize::k64).Int32Value())))
#define ART_METHOD_DECLARING_CLASS_OFFSET 0
DEFINE_CHECK_EQ(static_cast<int32_t>(ART_METHOD_DECLARING_CLASS_OFFSET), (static_cast<int32_t>(art::ArtMethod:: DeclaringClassOffset().Int32Value())))
#define STRING_DEX_CACHE_ELEMENT_SIZE_SHIFT 3
DEFINE_CHECK_EQ(static_cast<int32_t>(STRING_DEX_CACHE_ELEMENT_SIZE_SHIFT), (static_cast<int32_t>(art::WhichPowerOf2(sizeof(art::mirror::StringDexCachePair)))))
#define STRING_DEX_CACHE_SIZE_MINUS_ONE 1023
DEFINE_CHECK_EQ(static_cast<int32_t>(STRING_DEX_CACHE_SIZE_MINUS_ONE), (static_cast<int32_t>(art::mirror::DexCache::kDexCacheStringCacheSize - 1)))
#define STRING_DEX_CACHE_HASH_BITS 10
DEFINE_CHECK_EQ(static_cast<int32_t>(STRING_DEX_CACHE_HASH_BITS), (static_cast<int32_t>(art::LeastSignificantBit(art::mirror::DexCache::kDexCacheStringCacheSize))))
#define STRING_DEX_CACHE_ELEMENT_SIZE 8
DEFINE_CHECK_EQ(static_cast<int32_t>(STRING_DEX_CACHE_ELEMENT_SIZE), (static_cast<int32_t>(sizeof(art::mirror::StringDexCachePair))))
#define MIN_LARGE_OBJECT_THRESHOLD 0x3000
DEFINE_CHECK_EQ(static_cast<size_t>(MIN_LARGE_OBJECT_THRESHOLD), (static_cast<size_t>(art::gc::Heap::kMinLargeObjectThreshold)))
#define LOCK_WORD_STATE_SHIFT 30
DEFINE_CHECK_EQ(static_cast<int32_t>(LOCK_WORD_STATE_SHIFT), (static_cast<int32_t>(art::LockWord::kStateShift)))
#define LOCK_WORD_STATE_MASK 0xc0000000
DEFINE_CHECK_EQ(static_cast<uint32_t>(LOCK_WORD_STATE_MASK), (static_cast<uint32_t>(art::LockWord::kStateMaskShifted)))
#define LOCK_WORD_READ_BARRIER_STATE_SHIFT 28
DEFINE_CHECK_EQ(static_cast<int32_t>(LOCK_WORD_READ_BARRIER_STATE_SHIFT), (static_cast<int32_t>(art::LockWord::kReadBarrierStateShift)))
#define LOCK_WORD_READ_BARRIER_STATE_MASK 0x10000000
DEFINE_CHECK_EQ(static_cast<uint32_t>(LOCK_WORD_READ_BARRIER_STATE_MASK), (static_cast<uint32_t>(art::LockWord::kReadBarrierStateMaskShifted)))
#define LOCK_WORD_READ_BARRIER_STATE_MASK_TOGGLED 0xefffffff
DEFINE_CHECK_EQ(static_cast<uint32_t>(LOCK_WORD_READ_BARRIER_STATE_MASK_TOGGLED), (static_cast<uint32_t>(art::LockWord::kReadBarrierStateMaskShiftedToggled)))
#define LOCK_WORD_THIN_LOCK_COUNT_ONE 65536
DEFINE_CHECK_EQ(static_cast<int32_t>(LOCK_WORD_THIN_LOCK_COUNT_ONE), (static_cast<int32_t>(art::LockWord::kThinLockCountOne)))
#define LOCK_WORD_STATE_FORWARDING_ADDRESS 0x3
DEFINE_CHECK_EQ(static_cast<uint32_t>(LOCK_WORD_STATE_FORWARDING_ADDRESS), (static_cast<uint32_t>(art::LockWord::kStateForwardingAddress)))
#define LOCK_WORD_STATE_FORWARDING_ADDRESS_OVERFLOW 0x40000000
DEFINE_CHECK_EQ(static_cast<uint32_t>(LOCK_WORD_STATE_FORWARDING_ADDRESS_OVERFLOW), (static_cast<uint32_t>(art::LockWord::kStateForwardingAddressOverflow)))
#define LOCK_WORD_STATE_FORWARDING_ADDRESS_SHIFT 0x3
DEFINE_CHECK_EQ(static_cast<uint32_t>(LOCK_WORD_STATE_FORWARDING_ADDRESS_SHIFT), (static_cast<uint32_t>(art::LockWord::kForwardingAddressShift)))
#define LOCK_WORD_GC_STATE_MASK_SHIFTED 0x30000000
DEFINE_CHECK_EQ(static_cast<uint32_t>(LOCK_WORD_GC_STATE_MASK_SHIFTED), (static_cast<uint32_t>(art::LockWord::kGCStateMaskShifted)))
#define LOCK_WORD_GC_STATE_MASK_SHIFTED_TOGGLED 0xcfffffff
DEFINE_CHECK_EQ(static_cast<uint32_t>(LOCK_WORD_GC_STATE_MASK_SHIFTED_TOGGLED), (static_cast<uint32_t>(art::LockWord::kGCStateMaskShiftedToggled)))
#define LOCK_WORD_GC_STATE_SHIFT 28
DEFINE_CHECK_EQ(static_cast<int32_t>(LOCK_WORD_GC_STATE_SHIFT), (static_cast<int32_t>(art::LockWord::kGCStateShift)))
#define LOCK_WORD_MARK_BIT_SHIFT 29
DEFINE_CHECK_EQ(static_cast<int32_t>(LOCK_WORD_MARK_BIT_SHIFT), (static_cast<int32_t>(art::LockWord::kMarkBitStateShift)))
#define LOCK_WORD_MARK_BIT_MASK_SHIFTED 0x20000000
DEFINE_CHECK_EQ(static_cast<uint32_t>(LOCK_WORD_MARK_BIT_MASK_SHIFTED), (static_cast<uint32_t>(art::LockWord::kMarkBitStateMaskShifted)))
#define OBJECT_ALIGNMENT_MASK 0x7
DEFINE_CHECK_EQ(static_cast<size_t>(OBJECT_ALIGNMENT_MASK), (static_cast<size_t>(art::kObjectAlignment - 1)))
#define OBJECT_ALIGNMENT_MASK_TOGGLED 0xfffffff8
DEFINE_CHECK_EQ(static_cast<uint32_t>(OBJECT_ALIGNMENT_MASK_TOGGLED), (static_cast<uint32_t>(~static_cast<uint32_t>(art::kObjectAlignment - 1))))
#define OBJECT_ALIGNMENT_MASK_TOGGLED64 0xfffffffffffffff8
DEFINE_CHECK_EQ(static_cast<uint64_t>(OBJECT_ALIGNMENT_MASK_TOGGLED64), (static_cast<uint64_t>(~static_cast<uint64_t>(art::kObjectAlignment - 1))))
#define ROSALLOC_MAX_THREAD_LOCAL_BRACKET_SIZE 128
DEFINE_CHECK_EQ(static_cast<int32_t>(ROSALLOC_MAX_THREAD_LOCAL_BRACKET_SIZE), (static_cast<int32_t>((art::gc::allocator::RosAlloc::kMaxThreadLocalBracketSize))))
#define ROSALLOC_BRACKET_QUANTUM_SIZE_SHIFT 3
DEFINE_CHECK_EQ(static_cast<int32_t>(ROSALLOC_BRACKET_QUANTUM_SIZE_SHIFT), (static_cast<int32_t>((art::gc::allocator::RosAlloc::kThreadLocalBracketQuantumSizeShift))))
#define ROSALLOC_BRACKET_QUANTUM_SIZE_MASK 7
DEFINE_CHECK_EQ(static_cast<int32_t>(ROSALLOC_BRACKET_QUANTUM_SIZE_MASK), (static_cast<int32_t>((static_cast<int32_t>(art::gc::allocator::RosAlloc::kThreadLocalBracketQuantumSize - 1)))))
#define ROSALLOC_BRACKET_QUANTUM_SIZE_MASK_TOGGLED32 0xfffffff8
DEFINE_CHECK_EQ(static_cast<uint32_t>(ROSALLOC_BRACKET_QUANTUM_SIZE_MASK_TOGGLED32), (static_cast<uint32_t>((~static_cast<uint32_t>(art::gc::allocator::RosAlloc::kThreadLocalBracketQuantumSize - 1)))))
#define ROSALLOC_BRACKET_QUANTUM_SIZE_MASK_TOGGLED64 0xfffffffffffffff8
DEFINE_CHECK_EQ(static_cast<uint64_t>(ROSALLOC_BRACKET_QUANTUM_SIZE_MASK_TOGGLED64), (static_cast<uint64_t>((~static_cast<uint64_t>(art::gc::allocator::RosAlloc::kThreadLocalBracketQuantumSize - 1)))))
#define ROSALLOC_RUN_FREE_LIST_OFFSET 8
DEFINE_CHECK_EQ(static_cast<int32_t>(ROSALLOC_RUN_FREE_LIST_OFFSET), (static_cast<int32_t>((art::gc::allocator::RosAlloc::RunFreeListOffset()))))
#define ROSALLOC_RUN_FREE_LIST_HEAD_OFFSET 0
DEFINE_CHECK_EQ(static_cast<int32_t>(ROSALLOC_RUN_FREE_LIST_HEAD_OFFSET), (static_cast<int32_t>((art::gc::allocator::RosAlloc::RunFreeListHeadOffset()))))
#define ROSALLOC_RUN_FREE_LIST_SIZE_OFFSET 16
DEFINE_CHECK_EQ(static_cast<int32_t>(ROSALLOC_RUN_FREE_LIST_SIZE_OFFSET), (static_cast<int32_t>((art::gc::allocator::RosAlloc::RunFreeListSizeOffset()))))
#define ROSALLOC_SLOT_NEXT_OFFSET 0
DEFINE_CHECK_EQ(static_cast<int32_t>(ROSALLOC_SLOT_NEXT_OFFSET), (static_cast<int32_t>((art::gc::allocator::RosAlloc::RunSlotNextOffset()))))
#define THREAD_SUSPEND_REQUEST 1
DEFINE_CHECK_EQ(static_cast<int32_t>(THREAD_SUSPEND_REQUEST), (static_cast<int32_t>((art::kSuspendRequest))))
#define THREAD_CHECKPOINT_REQUEST 2
DEFINE_CHECK_EQ(static_cast<int32_t>(THREAD_CHECKPOINT_REQUEST), (static_cast<int32_t>((art::kCheckpointRequest))))
#define THREAD_EMPTY_CHECKPOINT_REQUEST 4
DEFINE_CHECK_EQ(static_cast<int32_t>(THREAD_EMPTY_CHECKPOINT_REQUEST), (static_cast<int32_t>((art::kEmptyCheckpointRequest))))
#define THREAD_SUSPEND_OR_CHECKPOINT_REQUEST 7
DEFINE_CHECK_EQ(static_cast<int32_t>(THREAD_SUSPEND_OR_CHECKPOINT_REQUEST), (static_cast<int32_t>((art::kSuspendRequest | art::kCheckpointRequest | art::kEmptyCheckpointRequest))))
#define JIT_CHECK_OSR (-1)
DEFINE_CHECK_EQ(static_cast<int16_t>(JIT_CHECK_OSR), (static_cast<int16_t>((art::jit::kJitCheckForOSR))))
#define JIT_HOTNESS_DISABLE (-2)
DEFINE_CHECK_EQ(static_cast<int16_t>(JIT_HOTNESS_DISABLE), (static_cast<int16_t>((art::jit::kJitHotnessDisabled))))

#endif  // ART_RUNTIME_GENERATED_ASM_SUPPORT_GEN_H_

