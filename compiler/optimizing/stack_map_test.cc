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

#include "stack_map.h"
#include "stack_map_stream.h"
#include "utils/arena_bit_vector.h"

#include "gtest/gtest.h"

namespace art {

bool SameBits(MemoryRegion region, const BitVector& bit_vector) {
  for (size_t i = 0; i < region.size_in_bits(); ++i) {
    if (region.LoadBit(i) != bit_vector.IsBitSet(i)) {
      return false;
    }
  }
  return true;
}

size_t ComputeDexRegisterMapSize(size_t number_of_dex_registers) {
  return DexRegisterMap::kFixedSize
      + number_of_dex_registers * DexRegisterMap::SingleEntrySize();
}

size_t ComputeDexRegisterTableSize(size_t number_of_dex_registers) {
  return DexRegisterTable::kFixedSize
      + number_of_dex_registers * DexRegisterTable::SingleEntrySize();
}

size_t ComputeDexRegisterCompressedMapSize(const DexRegisterCompressedMap& dex_registers,
                                           size_t number_of_dex_registers) {
  return dex_registers.FindEntry(number_of_dex_registers);
}

TEST(StackMapTest, Test1) {
  ArenaPool pool;
  ArenaAllocator arena(&pool);
  StackMapStream stream(&arena);

  ArenaBitVector sp_mask(&arena, 0, false);
  size_t number_of_dex_registers = 2;
  stream.AddStackMapEntry(0, 64, 0x3, &sp_mask, number_of_dex_registers, 0);
  stream.AddDexRegisterEntry(DexRegisterMap::kInStack, 0);
  stream.AddDexRegisterEntry(DexRegisterMap::kConstant, -2);

  size_t size = stream.ComputeNeededSize();
  void* memory = arena.Alloc(size, kArenaAllocMisc);
  MemoryRegion region(memory, size);
  stream.FillIn(region);

  CodeInfo code_info(region);
  ASSERT_EQ(0u, code_info.GetStackMaskSize());
  ASSERT_EQ(1u, code_info.GetNumberOfStackMaps());

  if (dex_register_map_encoding == kDexRegisterLocationDictionary) {
    uint32_t number_of_dictionary_entries =
        code_info.GetNumberOfDexRegisterDictionaryEntries();
    ASSERT_EQ(2u, number_of_dictionary_entries);
    DexRegisterDictionary dictionary = code_info.GetDexRegisterDictionary();
    ASSERT_EQ(10u, dictionary.Size());
    ASSERT_EQ(10u, ComputeDexRegisterMapSize(number_of_dictionary_entries));
  }

  StackMap stack_map = code_info.GetStackMapAt(0);
  ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForDexPc(0)));
  ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForNativePcOffset(64)));
  ASSERT_EQ(0u, stack_map.GetDexPc());
  ASSERT_EQ(64u, stack_map.GetNativePcOffset());
  ASSERT_EQ(0x3u, stack_map.GetRegisterMask());

  MemoryRegion stack_mask = stack_map.GetStackMask();
  ASSERT_TRUE(SameBits(stack_mask, sp_mask));

  ASSERT_TRUE(stack_map.HasDexRegisterMap());
  switch (dex_register_map_encoding) {
    case kDexRegisterLocationList: {
      DexRegisterMap dex_registers =
          code_info.GetDexRegisterMapOf(stack_map, number_of_dex_registers);
      ASSERT_EQ(10u, dex_registers.Size());
      ASSERT_EQ(10u, ComputeDexRegisterMapSize(number_of_dex_registers));
      ASSERT_EQ(DexRegisterMap::kInStack, dex_registers.GetLocationKind(0));
      ASSERT_EQ(DexRegisterMap::kConstant, dex_registers.GetLocationKind(1));
      ASSERT_EQ(0, dex_registers.GetValue(0));
      ASSERT_EQ(-2, dex_registers.GetValue(1));
      break;
    }

    case kDexRegisterLocationDictionary: {
      DexRegisterDictionary dictionary = code_info.GetDexRegisterDictionary();
      DexRegisterTable dex_registers =
          code_info.GetDexRegisterTableOf(stack_map, number_of_dex_registers);
      ASSERT_EQ(2u, dex_registers.Size());
      ASSERT_EQ(2u, ComputeDexRegisterTableSize(number_of_dex_registers));
      DexRegisterTable::EntryIndex index0 = dex_registers.GetEntryIndex(0);
      DexRegisterTable::EntryIndex index1 = dex_registers.GetEntryIndex(1);
      ASSERT_EQ(0u, index0);
      ASSERT_EQ(1u, index1);
      ASSERT_EQ(DexRegisterMap::kInStack, dictionary.GetLocationKind(index0));
      ASSERT_EQ(DexRegisterMap::kConstant, dictionary.GetLocationKind(index1));
      ASSERT_EQ(0, dictionary.GetValue(index0));
      ASSERT_EQ(-2, dictionary.GetValue(index1));
      break;
    }

    case kDexRegisterCompressedLocationList: {
      DexRegisterCompressedMap dex_registers =
          code_info.GetDexRegisterCompressedMapOf(stack_map,
                                                  number_of_dex_registers);
      ASSERT_EQ(6u, dex_registers.Size());
      ASSERT_EQ(6u, ComputeDexRegisterCompressedMapSize(dex_registers, number_of_dex_registers));
      std::pair<DexRegisterCompressedMap::LocationKind, int32_t> location0 =
          dex_registers.GetLocationKindAndValue(0);
      std::pair<DexRegisterCompressedMap::LocationKind, int32_t> location1 =
          dex_registers.GetLocationKindAndValue(1);
      ASSERT_EQ(DexRegisterCompressedMap::kInStack, location0.first);
      ASSERT_EQ(DexRegisterCompressedMap::kConstantLong, location1.first);
      ASSERT_EQ(0, location0.second);
      ASSERT_EQ(-2, location1.second);
      break;
    }
  }

  ASSERT_FALSE(stack_map.HasInlineInfo());
}

TEST(StackMapTest, Test2) {
  ArenaPool pool;
  ArenaAllocator arena(&pool);
  StackMapStream stream(&arena);

  ArenaBitVector sp_mask1(&arena, 0, true);
  sp_mask1.SetBit(2);
  sp_mask1.SetBit(4);
  size_t number_of_dex_registers = 2;
  stream.AddStackMapEntry(0, 64, 0x3, &sp_mask1, number_of_dex_registers, 2);
  stream.AddDexRegisterEntry(DexRegisterMap::kInStack, 0);
  stream.AddDexRegisterEntry(DexRegisterMap::kConstant, -2);
  stream.AddInlineInfoEntry(42);
  stream.AddInlineInfoEntry(82);

  ArenaBitVector sp_mask2(&arena, 0, true);
  sp_mask2.SetBit(3);
  sp_mask1.SetBit(8);
  stream.AddStackMapEntry(1, 128, 0xFF, &sp_mask2, number_of_dex_registers, 0);
  stream.AddDexRegisterEntry(DexRegisterMap::kInRegister, 18);
  stream.AddDexRegisterEntry(DexRegisterMap::kInFpuRegister, 3);

  size_t size = stream.ComputeNeededSize();
  void* memory = arena.Alloc(size, kArenaAllocMisc);
  MemoryRegion region(memory, size);
  stream.FillIn(region);

  CodeInfo code_info(region);
  ASSERT_EQ(1u, code_info.GetStackMaskSize());
  ASSERT_EQ(2u, code_info.GetNumberOfStackMaps());

  if (dex_register_map_encoding == kDexRegisterLocationDictionary) {
    uint32_t number_of_dictionary_entries =
        code_info.GetNumberOfDexRegisterDictionaryEntries();
    ASSERT_EQ(4u, number_of_dictionary_entries);
    DexRegisterDictionary dictionary = code_info.GetDexRegisterDictionary();
    ASSERT_EQ(20u, dictionary.Size());
    ASSERT_EQ(20u, ComputeDexRegisterMapSize(number_of_dictionary_entries));
  }

  // First stack map.
  StackMap stack_map = code_info.GetStackMapAt(0);
  ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForDexPc(0)));
  ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForNativePcOffset(64)));
  ASSERT_EQ(0u, stack_map.GetDexPc());
  ASSERT_EQ(64u, stack_map.GetNativePcOffset());
  ASSERT_EQ(0x3u, stack_map.GetRegisterMask());

  MemoryRegion stack_mask = stack_map.GetStackMask();
  ASSERT_TRUE(SameBits(stack_mask, sp_mask1));

  ASSERT_TRUE(stack_map.HasDexRegisterMap());
  switch (dex_register_map_encoding) {
    case kDexRegisterLocationList: {
      DexRegisterMap dex_registers =
          code_info.GetDexRegisterMapOf(stack_map, number_of_dex_registers);
      ASSERT_EQ(10u, dex_registers.Size());
      ASSERT_EQ(10u, ComputeDexRegisterMapSize(number_of_dex_registers));
      ASSERT_EQ(DexRegisterMap::kInStack, dex_registers.GetLocationKind(0));
      ASSERT_EQ(DexRegisterMap::kConstant, dex_registers.GetLocationKind(1));
      ASSERT_EQ(0, dex_registers.GetValue(0));
      ASSERT_EQ(-2, dex_registers.GetValue(1));
      break;
    }

    case kDexRegisterLocationDictionary: {
      DexRegisterDictionary dictionary = code_info.GetDexRegisterDictionary();
      DexRegisterTable dex_registers =
          code_info.GetDexRegisterTableOf(stack_map, number_of_dex_registers);
      ASSERT_EQ(2u, dex_registers.Size());
      ASSERT_EQ(2u, ComputeDexRegisterTableSize(number_of_dex_registers));
      DexRegisterTable::EntryIndex index0 = dex_registers.GetEntryIndex(0);
      DexRegisterTable::EntryIndex index1 = dex_registers.GetEntryIndex(1);
      ASSERT_EQ(0u, index0);
      ASSERT_EQ(1u, index1);
      ASSERT_EQ(DexRegisterMap::kInStack, dictionary.GetLocationKind(index0));
      ASSERT_EQ(DexRegisterMap::kConstant, dictionary.GetLocationKind(index1));
      ASSERT_EQ(0, dictionary.GetValue(index0));
      ASSERT_EQ(-2, dictionary.GetValue(index1));
      break;
    }

    case kDexRegisterCompressedLocationList: {
      DexRegisterCompressedMap dex_registers =
          code_info.GetDexRegisterCompressedMapOf(stack_map,
                                                  number_of_dex_registers);
      ASSERT_EQ(6u, dex_registers.Size());
      ASSERT_EQ(6u, ComputeDexRegisterCompressedMapSize(dex_registers, number_of_dex_registers));
      // Check the pair accessor.
      std::pair<DexRegisterCompressedMap::LocationKind, int32_t> location0 =
          dex_registers.GetLocationKindAndValue(0);
      std::pair<DexRegisterCompressedMap::LocationKind, int32_t> location1 =
          dex_registers.GetLocationKindAndValue(1);
      ASSERT_EQ(DexRegisterCompressedMap::kInStack, location0.first);
      ASSERT_EQ(DexRegisterCompressedMap::kConstantLong, location1.first);
      ASSERT_EQ(0, location0.second);
      ASSERT_EQ(-2, location1.second);
      // Also check the single accessors.
      ASSERT_EQ(DexRegisterCompressedMap::kInStack, dex_registers.GetLocationKind(0));
      ASSERT_EQ(DexRegisterCompressedMap::kConstantLong, dex_registers.GetLocationKind(1));
      ASSERT_EQ(0, dex_registers.GetValue(0));
      ASSERT_EQ(-2, dex_registers.GetValue(1));
      break;
    }
  }

  ASSERT_TRUE(stack_map.HasInlineInfo());
  InlineInfo inline_info = code_info.GetInlineInfoOf(stack_map);
  ASSERT_EQ(2u, inline_info.GetDepth());
  ASSERT_EQ(42u, inline_info.GetMethodReferenceIndexAtDepth(0));
  ASSERT_EQ(82u, inline_info.GetMethodReferenceIndexAtDepth(1));

  // Second stack map.
  stack_map = code_info.GetStackMapAt(1);
  ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForDexPc(1u)));
  ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForNativePcOffset(128u)));
  ASSERT_EQ(1u, stack_map.GetDexPc());
  ASSERT_EQ(128u, stack_map.GetNativePcOffset());
  ASSERT_EQ(0xFFu, stack_map.GetRegisterMask());

  stack_mask = stack_map.GetStackMask();
  ASSERT_TRUE(SameBits(stack_mask, sp_mask2));

  ASSERT_TRUE(stack_map.HasDexRegisterMap());
  switch (dex_register_map_encoding) {
    case kDexRegisterLocationList: {
      DexRegisterMap dex_registers =
          code_info.GetDexRegisterMapOf(stack_map, number_of_dex_registers);
      ASSERT_EQ(10u, dex_registers.Size());
      ASSERT_EQ(10u, ComputeDexRegisterMapSize(number_of_dex_registers));
      ASSERT_EQ(DexRegisterMap::kInRegister, dex_registers.GetLocationKind(0));
      ASSERT_EQ(DexRegisterMap::kInFpuRegister, dex_registers.GetLocationKind(1));
      ASSERT_EQ(18, dex_registers.GetValue(0));
      ASSERT_EQ(3, dex_registers.GetValue(1));
      break;
    }

    case kDexRegisterLocationDictionary: {
      DexRegisterDictionary dictionary = code_info.GetDexRegisterDictionary();
      DexRegisterTable dex_registers =
          code_info.GetDexRegisterTableOf(stack_map, number_of_dex_registers);
      ASSERT_EQ(2u, dex_registers.Size());
      ASSERT_EQ(2u, ComputeDexRegisterTableSize(number_of_dex_registers));
      DexRegisterTable::EntryIndex index0 = dex_registers.GetEntryIndex(0);
      DexRegisterTable::EntryIndex index1 = dex_registers.GetEntryIndex(1);
      ASSERT_EQ(2u, index0);
      ASSERT_EQ(3u, index1);
      ASSERT_EQ(DexRegisterMap::kInRegister, dictionary.GetLocationKind(index0));
      ASSERT_EQ(DexRegisterMap::kInFpuRegister, dictionary.GetLocationKind(index1));
      ASSERT_EQ(18, dictionary.GetValue(index0));
      ASSERT_EQ(3, dictionary.GetValue(index1));
      break;
    }

    case kDexRegisterCompressedLocationList: {
      DexRegisterCompressedMap dex_registers =
          code_info.GetDexRegisterCompressedMapOf(stack_map,
                                                  number_of_dex_registers);
      ASSERT_EQ(2u, dex_registers.Size());
      ASSERT_EQ(2u, ComputeDexRegisterCompressedMapSize(dex_registers, number_of_dex_registers));
      // Check the pair accessor.
      std::pair<DexRegisterCompressedMap::LocationKind, int32_t> location0 =
          dex_registers.GetLocationKindAndValue(0);
      std::pair<DexRegisterCompressedMap::LocationKind, int32_t> location1 =
          dex_registers.GetLocationKindAndValue(1);
      ASSERT_EQ(DexRegisterCompressedMap::kInRegister, location0.first);
      ASSERT_EQ(DexRegisterCompressedMap::kInFpuRegister, location1.first);
      ASSERT_EQ(18, location0.second);
      ASSERT_EQ(3, location1.second);
      // Also check the single accessors.
      ASSERT_EQ(DexRegisterCompressedMap::kInRegister, dex_registers.GetLocationKind(0));
      ASSERT_EQ(DexRegisterCompressedMap::kInFpuRegister, dex_registers.GetLocationKind(1));
      ASSERT_EQ(18, dex_registers.GetValue(0));
      ASSERT_EQ(3, dex_registers.GetValue(1));
      break;
    }
  }

  ASSERT_FALSE(stack_map.HasInlineInfo());
}

}  // namespace art
