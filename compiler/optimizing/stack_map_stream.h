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

#ifndef ART_COMPILER_OPTIMIZING_STACK_MAP_STREAM_H_
#define ART_COMPILER_OPTIMIZING_STACK_MAP_STREAM_H_

#include "base/bit_vector-inl.h"
#include "base/value_object.h"
#include "memory_region.h"
#include "nodes.h"
#include "stack_map.h"
#include "utils/growable_array.h"

namespace art {

/**
 * Collects and builds stack maps for a method. All the stack maps
 * for a method are placed in a CodeInfo object.
 */
class StackMapStream : public ValueObject {
 public:
  explicit StackMapStream(ArenaAllocator* allocator)
      : allocator_(allocator),
        stack_maps_(allocator, 10),
        dex_register_locations_(allocator, 10 * 4),
        inline_infos_(allocator, 2),
        stack_mask_max_(-1),
        number_of_stack_maps_with_inline_info_(0) {}

  // Compute bytes needed to encode a mask with the given maximum element.
  static uint32_t StackMaskEncodingSize(int max_element) {
    int number_of_bits = max_element + 1;  // Need room for max element too.
    return RoundUp(number_of_bits, kBitsPerByte) / kBitsPerByte;
  }

  // See runtime/stack_map.h to know what these fields contain.
  struct StackMapEntry {
    uint32_t dex_pc;
    uint32_t native_pc_offset;
    uint32_t register_mask;
    BitVector* sp_mask;
    uint32_t num_dex_registers;
    uint8_t inlining_depth;
    size_t dex_register_locations_start_index;
    size_t inline_infos_start_index;
    BitVector* live_dex_registers_mask;
  };

  struct InlineInfoEntry {
    uint32_t method_index;
  };

  void AddStackMapEntry(uint32_t dex_pc,
                        uint32_t native_pc_offset,
                        uint32_t register_mask,
                        BitVector* sp_mask,
                        uint32_t num_dex_registers,
                        uint8_t inlining_depth) {
    StackMapEntry entry;
    entry.dex_pc = dex_pc;
    entry.native_pc_offset = native_pc_offset;
    entry.register_mask = register_mask;
    entry.sp_mask = sp_mask;
    entry.num_dex_registers = num_dex_registers;
    entry.inlining_depth = inlining_depth;
    entry.dex_register_locations_start_index = dex_register_locations_.Size();
    entry.inline_infos_start_index = inline_infos_.Size();
    if (num_dex_registers != 0) {
      entry.live_dex_registers_mask =
          new (allocator_) ArenaBitVector(allocator_, num_dex_registers, true);
    } else {
      entry.live_dex_registers_mask = nullptr;
    }
    stack_maps_.Add(entry);

    if (sp_mask != nullptr) {
      stack_mask_max_ = std::max(stack_mask_max_, sp_mask->GetHighestBitSet());
    }
    if (inlining_depth > 0) {
      number_of_stack_maps_with_inline_info_++;
    }
  }

  void AddInlineInfoEntry(uint32_t method_index) {
    InlineInfoEntry entry;
    entry.method_index = method_index;
    inline_infos_.Add(entry);
  }

  size_t ComputeNeededSize() const {
    size_t size = CodeInfo::kFixedSize
        + ComputeStackMapsSize()
        + ComputeDexRegisterMapsSize()
        + ComputeInlineInfoSize();
    // Note: use RoundUp to word-size here if you want CodeInfo objects to be word aligned.
    return size;
  }

  size_t ComputeStackMaskSize() const {
    return StackMaskEncodingSize(stack_mask_max_);
  }

  size_t ComputeStackMapsSize() const {
    return stack_maps_.Size() * StackMap::ComputeStackMapSize(ComputeStackMaskSize());
  }

  // Compute the size of the Dex register map of `entry`.
  size_t ComputeDexRegisterMapSize(const StackMapEntry& entry) const {
    size_t size = DexRegisterMap::kFixedSize;
    // Add the bit mask for the dex register liveness.
    size += DexRegisterMap::LiveBitMaskSize(entry.num_dex_registers);
    for (size_t dex_register_number = 0, index_in_dex_register_locations = 0;
         dex_register_number < entry.num_dex_registers;
         ++dex_register_number) {
      if (entry.live_dex_registers_mask->IsBitSet(dex_register_number)) {
        DexRegisterLocation dex_register_location = dex_register_locations_.Get(
            entry.dex_register_locations_start_index + index_in_dex_register_locations);
        size += DexRegisterMap::EntrySize(dex_register_location);
        index_in_dex_register_locations++;
      }
    }
    return size;
  }

  // Compute the size of all the Dex register maps.
  size_t ComputeDexRegisterMapsSize() const {
    size_t size = 0;
    for (size_t i = 0; i < stack_maps_.Size(); ++i) {
      if (FindEntryWithTheSameDexMap(i) == kNoSameDexMapFound) {
        // Entries with the same dex map will have the same offset.
        size += ComputeDexRegisterMapSize(stack_maps_.Get(i));
      }
    }
    return size;
  }

  // Compute the size of all the inline information pieces.
  size_t ComputeInlineInfoSize() const {
    return inline_infos_.Size() * InlineInfo::SingleEntrySize()
      // For encoding the depth.
      + (number_of_stack_maps_with_inline_info_ * InlineInfo::kFixedSize);
  }

  size_t ComputeDexRegisterMapsStart() const {
    return CodeInfo::kFixedSize + ComputeStackMapsSize();
  }

  size_t ComputeInlineInfoStart() const {
    return ComputeDexRegisterMapsStart() + ComputeDexRegisterMapsSize();
  }

  void FillIn(MemoryRegion region) {
    CodeInfo code_info(region);
    DCHECK_EQ(region.size(), ComputeNeededSize());
    code_info.SetOverallSize(region.size());

    size_t stack_mask_size = ComputeStackMaskSize();
    uint8_t* memory_start = region.start();

    MemoryRegion dex_register_locations_region = region.Subregion(
      ComputeDexRegisterMapsStart(),
      ComputeDexRegisterMapsSize());

    MemoryRegion inline_infos_region = region.Subregion(
      ComputeInlineInfoStart(),
      ComputeInlineInfoSize());

    code_info.SetNumberOfStackMaps(stack_maps_.Size());
    code_info.SetStackMaskSize(stack_mask_size);
    DCHECK_EQ(code_info.StackMapsSize(), ComputeStackMapsSize());

    uintptr_t next_dex_register_map_offset = 0;
    uintptr_t next_inline_info_offset = 0;
    for (size_t i = 0, e = stack_maps_.Size(); i < e; ++i) {
      StackMap stack_map = code_info.GetStackMapAt(i);
      StackMapEntry entry = stack_maps_.Get(i);

      stack_map.SetDexPc(entry.dex_pc);
      stack_map.SetNativePcOffset(entry.native_pc_offset);
      stack_map.SetRegisterMask(entry.register_mask);
      if (entry.sp_mask != nullptr) {
        stack_map.SetStackMask(*entry.sp_mask);
      }

      if (entry.num_dex_registers == 0) {
        // No dex map available.
        stack_map.SetDexRegisterMapOffset(StackMap::kNoDexRegisterMap);
      } else {
        // Search for an entry with the same dex map.
        size_t entry_with_same_map = FindEntryWithTheSameDexMap(i);
        if (entry_with_same_map != kNoSameDexMapFound) {
          // If we have a hit reuse the offset.
          stack_map.SetDexRegisterMapOffset(
            code_info.GetStackMapAt(entry_with_same_map).GetDexRegisterMapOffset());
        } else {
          // If we have a completely new dex map add it to the stack map.
          MemoryRegion register_region =
              dex_register_locations_region.Subregion(
                  next_dex_register_map_offset,
                  ComputeDexRegisterMapSize(entry));
          next_dex_register_map_offset += register_region.size();
          DexRegisterMap dex_register_map(register_region);
          stack_map.SetDexRegisterMapOffset(register_region.start() - memory_start);

          // Offset in `dex_register_map` where to store the next register entry.
          size_t offset = DexRegisterMap::kFixedSize;
          dex_register_map.SetLiveBitMask(offset,
                                          entry.num_dex_registers,
                                          *entry.live_dex_registers_mask);
          offset += DexRegisterMap::LiveBitMaskSize(entry.num_dex_registers);
          for (size_t dex_register_number = 0, index_in_dex_register_locations = 0;
               dex_register_number < entry.num_dex_registers;
               ++dex_register_number) {
            if (entry.live_dex_registers_mask->IsBitSet(dex_register_number)) {
              DexRegisterLocation dex_register_location = dex_register_locations_.Get(
                  entry.dex_register_locations_start_index + index_in_dex_register_locations);
              dex_register_map.SetRegisterInfo(offset, dex_register_location);
              offset += DexRegisterMap::EntrySize(dex_register_location);
              ++index_in_dex_register_locations;
            }
          }
          // Ensure we reached the end of the Dex registers region.
          DCHECK_EQ(offset, register_region.size());
        }
      }

      // Set the inlining info.
      if (entry.inlining_depth != 0) {
        MemoryRegion inline_region = inline_infos_region.Subregion(
            next_inline_info_offset,
            InlineInfo::kFixedSize + entry.inlining_depth * InlineInfo::SingleEntrySize());
        next_inline_info_offset += inline_region.size();
        InlineInfo inline_info(inline_region);

        stack_map.SetInlineDescriptorOffset(inline_region.start() - memory_start);

        inline_info.SetDepth(entry.inlining_depth);
        for (size_t j = 0; j < entry.inlining_depth; ++j) {
          InlineInfoEntry inline_entry = inline_infos_.Get(j + entry.inline_infos_start_index);
          inline_info.SetMethodReferenceIndexAtDepth(j, inline_entry.method_index);
        }
      } else {
        stack_map.SetInlineDescriptorOffset(StackMap::kNoInlineInfo);
      }
    }
  }

  void AddDexRegisterEntry(uint16_t dex_register, DexRegisterLocation::Kind kind, int32_t value) {
    if (kind != DexRegisterLocation::Kind::kNone) {
      // Ensure we only use non-compressed location kind at this stage.
      DCHECK(DexRegisterLocation::IsShortLocationKind(kind))
          << DexRegisterLocation::PrettyDescriptor(kind);
      dex_register_locations_.Add(DexRegisterLocation(kind, value));
      stack_maps_.Get(stack_maps_.Size() - 1).live_dex_registers_mask->SetBit(dex_register);
    }
  }

 private:
  // Returns the index of an entry with the same dex register map
  // or kNoSameDexMapFound if no such entry exists.
  size_t FindEntryWithTheSameDexMap(size_t entry_index) const {
    if (entry_index == 0) {
      return kNoSameDexMapFound;
    }
    StackMapEntry entry = stack_maps_.Get(entry_index);
    // It's a higher chance to find the same dex map in an adjacent entry.
    for (size_t i = entry_index - 1; i != 0 ; i--) {
      if (HaveTheSameDexMaps(stack_maps_.Get(i), entry)) {
        return i;
      }
    }
    return kNoSameDexMapFound;
  }

  bool HaveTheSameDexMaps(const StackMapEntry& a, const StackMapEntry& b) const {
    if (a.live_dex_registers_mask == nullptr && b.live_dex_registers_mask == nullptr) {
      return true;
    }
    if (a.live_dex_registers_mask == nullptr || b.live_dex_registers_mask == nullptr) {
      return false;
    }
    if (a.num_dex_registers != b.num_dex_registers) {
      return false;
    }

    int index_in_dex_register_locations = 0;
    for (uint32_t i = 0; i < a.num_dex_registers; i++) {
      if (a.live_dex_registers_mask->IsBitSet(i) != b.live_dex_registers_mask->IsBitSet(i)) {
        return false;
      }
      if (a.live_dex_registers_mask->IsBitSet(i)) {
        DexRegisterLocation a_loc = dex_register_locations_.Get(
            a.dex_register_locations_start_index + index_in_dex_register_locations);
        DexRegisterLocation b_loc = dex_register_locations_.Get(
            b.dex_register_locations_start_index + index_in_dex_register_locations);
        if (!a_loc.Equal(b_loc)) {
          return false;
        }
        ++index_in_dex_register_locations;
      }
    }
    return true;
  }

  ArenaAllocator* allocator_;
  GrowableArray<StackMapEntry> stack_maps_;
  GrowableArray<DexRegisterLocation> dex_register_locations_;
  GrowableArray<InlineInfoEntry> inline_infos_;
  int stack_mask_max_;
  size_t number_of_stack_maps_with_inline_info_;

  static constexpr uint32_t kNoSameDexMapFound = -1;

  ART_FRIEND_TEST(StackMapTest, Test1);
  ART_FRIEND_TEST(StackMapTest, Test2);
  ART_FRIEND_TEST(StackMapTest, TestNonLiveDexRegisters);

  DISALLOW_COPY_AND_ASSIGN(StackMapStream);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_STACK_MAP_STREAM_H_
