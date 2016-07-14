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

#ifndef ART_COMPILER_OPTIMIZING_REGISTER_ALLOCATOR_LINEAR_SCAN_H_
#define ART_COMPILER_OPTIMIZING_REGISTER_ALLOCATOR_LINEAR_SCAN_H_

#include "arch/instruction_set.h"
#include "base/arena_containers.h"
#include "base/macros.h"
#include "primitive.h"
#include "register_allocator.h"

namespace art {

class CodeGenerator;
class HBasicBlock;
class HGraph;
class HInstruction;
class HParallelMove;
class HPhi;
class LiveInterval;
class Location;
class SsaLivenessAnalysis;

/**
 * An implementation of a linear scan register allocator on an `HGraph` with SSA form.
 */
class RegisterAllocatorLinearScan : public RegisterAllocator {
 public:
  RegisterAllocatorLinearScan(ArenaAllocator* allocator,
                              CodeGenerator* codegen,
                              const SsaLivenessAnalysis& analysis);

  void AllocateRegisters() OVERRIDE;

  bool Validate(bool log_fatal_on_failure) OVERRIDE {
    processing_core_registers_ = true;
    if (!ValidateInternal(log_fatal_on_failure)) {
      return false;
    }
    processing_core_registers_ = false;
    return ValidateInternal(log_fatal_on_failure);
  }

  size_t GetNumberOfSpillSlots() const {
    return int_spill_slots_.size()
        + long_spill_slots_.size()
        + float_spill_slots_.size()
        + double_spill_slots_.size()
        + catch_phi_spill_slots_;
  }

 private:
  // Main methods of the allocator.
  void LinearScan();
  bool TryAllocateFreeReg(LiveInterval* interval);
  bool AllocateBlockedReg(LiveInterval* interval);

  // Add `interval` in the given sorted list.
  static void AddSorted(ArenaVector<LiveInterval*>* array, LiveInterval* interval);

  // Returns whether `reg` is blocked by the code generator.
  bool IsBlocked(int reg) const;

  // Update the interval for the register in `location` to cover [start, end).
  void BlockRegister(Location location, size_t start, size_t end);
  void BlockRegisters(size_t start, size_t end, bool caller_save_only = false);

  // Allocate a spill slot for the given interval. Should be called in linear
  // order of interval starting positions.
  void AllocateSpillSlotFor(LiveInterval* interval);

  // Allocate a spill slot for the given catch phi. Will allocate the same slot
  // for phis which share the same vreg. Must be called in reverse linear order
  // of lifetime positions and ascending vreg numbers for correctness.
  void AllocateSpillSlotForCatchPhi(HPhi* phi);

  // Helper methods.
  void AllocateRegistersInternal();
  void ProcessInstruction(HInstruction* instruction);
  bool ValidateInternal(bool log_fatal_on_failure) const;
  void DumpInterval(std::ostream& stream, LiveInterval* interval) const;
  void DumpAllIntervals(std::ostream& stream) const;
  int FindAvailableRegisterPair(size_t* next_use, size_t starting_at) const;
  int FindAvailableRegister(size_t* next_use, LiveInterval* current) const;
  bool IsCallerSaveRegister(int reg) const;

  // Try splitting an active non-pair or unaligned pair interval at the given `position`.
  // Returns whether it was successful at finding such an interval.
  bool TrySplitNonPairOrUnalignedPairIntervalAt(size_t position,
                                                size_t first_register_use,
                                                size_t* next_use);

  // List of intervals for core registers that must be processed, ordered by start
  // position. Last entry is the interval that has the lowest start position.
  // This list is initially populated before doing the linear scan.
  ArenaVector<LiveInterval*> unhandled_core_intervals_;

  // List of intervals for floating-point registers. Same comments as above.
  ArenaVector<LiveInterval*> unhandled_fp_intervals_;

  // Currently processed list of unhandled intervals. Either `unhandled_core_intervals_`
  // or `unhandled_fp_intervals_`.
  ArenaVector<LiveInterval*>* unhandled_;

  // List of intervals that have been processed.
  ArenaVector<LiveInterval*> handled_;

  // List of intervals that are currently active when processing a new live interval.
  // That is, they have a live range that spans the start of the new interval.
  ArenaVector<LiveInterval*> active_;

  // List of intervals that are currently inactive when processing a new live interval.
  // That is, they have a lifetime hole that spans the start of the new interval.
  ArenaVector<LiveInterval*> inactive_;

  // Fixed intervals for physical registers. Such intervals cover the positions
  // where an instruction requires a specific register.
  ArenaVector<LiveInterval*> physical_core_register_intervals_;
  ArenaVector<LiveInterval*> physical_fp_register_intervals_;

  // Intervals for temporaries. Such intervals cover the positions
  // where an instruction requires a temporary.
  ArenaVector<LiveInterval*> temp_intervals_;

  // The spill slots allocated for live intervals. We ensure spill slots
  // are typed to avoid (1) doing moves and swaps between two different kinds
  // of registers, and (2) swapping between a single stack slot and a double
  // stack slot. This simplifies the parallel move resolver.
  ArenaVector<size_t> int_spill_slots_;
  ArenaVector<size_t> long_spill_slots_;
  ArenaVector<size_t> float_spill_slots_;
  ArenaVector<size_t> double_spill_slots_;

  // Spill slots allocated to catch phis. This category is special-cased because
  // (1) slots are allocated prior to linear scan and in reverse linear order,
  // (2) equivalent phis need to share slots despite having different types.
  size_t catch_phi_spill_slots_;

  // Instructions that need a safepoint.
  ArenaVector<HInstruction*> safepoints_;

  // True if processing core registers. False if processing floating
  // point registers.
  bool processing_core_registers_;

  // Number of registers for the current register kind (core or floating point).
  size_t number_of_registers_;

  // Temporary array, allocated ahead of time for simplicity.
  size_t* registers_array_;

  // Blocked registers, as decided by the code generator.
  bool* const blocked_core_registers_;
  bool* const blocked_fp_registers_;

  // Slots reserved for out arguments.
  size_t reserved_out_slots_;

  // The maximum live core registers at safepoints.
  size_t maximum_number_of_live_core_registers_;

  // The maximum live FP registers at safepoints.
  size_t maximum_number_of_live_fp_registers_;

  ART_FRIEND_TEST(RegisterAllocatorTest, FreeUntil);
  ART_FRIEND_TEST(RegisterAllocatorTest, SpillInactive);

  DISALLOW_COPY_AND_ASSIGN(RegisterAllocatorLinearScan);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_REGISTER_ALLOCATOR_LINEAR_SCAN_H_
