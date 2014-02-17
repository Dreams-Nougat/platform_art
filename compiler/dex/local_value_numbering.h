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

#ifndef ART_COMPILER_DEX_LOCAL_VALUE_NUMBERING_H_
#define ART_COMPILER_DEX_LOCAL_VALUE_NUMBERING_H_

#include "compiler_internals.h"

#define NO_VALUE 0xffff
#define ARRAY_REF 0xfffe

namespace art {

class DexFile;

class LocalValueNumbering {
 private:
  // Field types correspond to the ordering of GET/PUT instructions; this order is the same
  // for IGET, IPUT, SGET, SPUT, AGET and APUT:
  // op      0
  // op_WIDE 1
  static constexpr size_t kFieldTypeCount = 7;

  // FieldKey represents either a unique resolved field or all unresolved fields of a type together.
  struct FieldReference {
    const DexFile* dex_file;
    uint16_t field_idx;
  };

  struct FieldReferenceComparator {
    bool operator()(const FieldReference& lhs, const FieldReference& rhs) const {
      if (lhs.field_idx != rhs.field_idx) {
        return lhs.field_idx < rhs.field_idx;
      }
      return lhs.dex_file < rhs.dex_file;
    }
  };

  struct MemoryVersionKey {
    uint16_t base;
    uint16_t field_id;
    uint16_t type;
  };

  struct MemoryVersionKeyComparator {
    bool operator()(const MemoryVersionKey& lhs, const MemoryVersionKey& rhs) const {
      if (lhs.base != rhs.base) {
        return lhs.base < rhs.base;
      }
      if (lhs.field_id != rhs.field_id) {
        return lhs.field_id < rhs.field_id;
      }
      return lhs.type < rhs.type;
    }
  };

  // Key is s_reg, value is value name.
  typedef SafeMap<uint16_t, uint16_t> SregValueMap;
  // Key is concatenation of opcode, operand1, operand2 and modifier, value is value name.
  typedef SafeMap<uint64_t, uint16_t> ValueMap;
  // Key represents a memory address, value is generation.
  typedef SafeMap<MemoryVersionKey, uint16_t, MemoryVersionKeyComparator> MemoryVersionMap;
  // Maps field key to field id for resolved fields.
  typedef SafeMap<FieldReference, uint32_t, FieldReferenceComparator> FieldIndexMap;

 public:
  explicit LocalValueNumbering(CompilationUnit* cu)
      : cu_(cu),
        sreg_value_map_(),
        sreg_wide_value_map_(),
        value_map_(),
        next_memory_version_(1u),
        global_memory_version_(0u),
        memory_version_map_(),
        field_index_map_(),
        unique_objects_(),
        null_checked_() {
    std::fill_n(unresolved_sfield_version_, kFieldTypeCount, 0u);
    std::fill_n(unresolved_ifield_version_, kFieldTypeCount, 0u);
  }

  static uint64_t BuildKey(uint16_t op, uint16_t operand1, uint16_t operand2, uint16_t modifier) {
    return (static_cast<uint64_t>(op) << 48 | static_cast<uint64_t>(operand1) << 32 |
            static_cast<uint64_t>(operand2) << 16 | static_cast<uint64_t>(modifier));
  };

  uint16_t LookupValue(uint16_t op, uint16_t operand1, uint16_t operand2, uint16_t modifier) {
    uint16_t res;
    uint64_t key = BuildKey(op, operand1, operand2, modifier);
    ValueMap::iterator it = value_map_.find(key);
    if (it != value_map_.end()) {
      res = it->second;
    } else {
      res = value_map_.size() + 1;
      value_map_.Put(key, res);
    }
    return res;
  };

  bool ValueExists(uint16_t op, uint16_t operand1, uint16_t operand2, uint16_t modifier) const {
    uint64_t key = BuildKey(op, operand1, operand2, modifier);
    ValueMap::const_iterator it = value_map_.find(key);
    return (it != value_map_.end());
  };

  void SetOperandValue(uint16_t s_reg, uint16_t value) {
    SregValueMap::iterator it = sreg_value_map_.find(s_reg);
    if (it != sreg_value_map_.end()) {
      DCHECK_EQ(it->second, value);
    } else {
      sreg_value_map_.Put(s_reg, value);
    }
  };

  uint16_t GetOperandValue(int s_reg) {
    uint16_t res = NO_VALUE;
    SregValueMap::iterator it = sreg_value_map_.find(s_reg);
    if (it != sreg_value_map_.end()) {
      res = it->second;
    } else {
      // First use
      res = LookupValue(NO_VALUE, s_reg, NO_VALUE, NO_VALUE);
      sreg_value_map_.Put(s_reg, res);
    }
    return res;
  };

  void SetOperandValueWide(uint16_t s_reg, uint16_t value) {
    SregValueMap::iterator it = sreg_wide_value_map_.find(s_reg);
    if (it != sreg_wide_value_map_.end()) {
      DCHECK_EQ(it->second, value);
    } else {
      sreg_wide_value_map_.Put(s_reg, value);
    }
  };

  uint16_t GetOperandValueWide(int s_reg) {
    uint16_t res = NO_VALUE;
    SregValueMap::iterator it = sreg_wide_value_map_.find(s_reg);
    if (it != sreg_wide_value_map_.end()) {
      res = it->second;
    } else {
      // First use
      res = LookupValue(NO_VALUE, s_reg, NO_VALUE, NO_VALUE);
      sreg_wide_value_map_.Put(s_reg, res);
    }
    return res;
  };

  uint16_t GetValueNumber(MIR* mir);

 private:
  uint16_t GetFieldId(const DexFile* dex_file, uint16_t field_idx);
  void AdvanceGlobalMemory();
  uint16_t GetMemoryVersion(uint16_t base, uint16_t field, uint16_t type);
  uint16_t AdvanceMemoryVersion(uint16_t base, uint16_t field, uint16_t type);
  uint16_t MarkUniqueNonNull(MIR* mir);
  void MakeArgsNonUnique(MIR* mir);
  void HandleNullCheck(MIR* mir, uint16_t reg);
  void HandleRangeCheck(MIR* mir, uint16_t array, uint16_t index);
  void HandlePutObject(MIR* mir);

  CompilationUnit* const cu_;
  SregValueMap sreg_value_map_;
  SregValueMap sreg_wide_value_map_;
  ValueMap value_map_;
  uint16_t next_memory_version_;
  uint16_t global_memory_version_;
  uint16_t unresolved_sfield_version_[kFieldTypeCount];
  uint16_t unresolved_ifield_version_[kFieldTypeCount];
  MemoryVersionMap memory_version_map_;
  FieldIndexMap field_index_map_;
  std::set<uint16_t> unique_objects_;
  std::set<uint16_t> null_checked_;
};

}  // namespace art

#endif  // ART_COMPILER_DEX_LOCAL_VALUE_NUMBERING_H_
