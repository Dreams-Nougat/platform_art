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

#include "code_generator.h"

#include "code_generator_arm.h"
#include "code_generator_arm64.h"
#include "code_generator_x86.h"
#include "code_generator_x86_64.h"
#include "compiled_method.h"
#include "dex/verified_method.h"
#include "driver/dex_compilation_unit.h"
#include "gc_map_builder.h"
#include "leb128.h"
#include "mapping_table.h"
#include "mirror/array-inl.h"
#include "mirror/object_array-inl.h"
#include "mirror/object_reference.h"
#include "ssa_liveness_analysis.h"
#include "utils/assembler.h"
#include "verifier/dex_gc_map.h"
#include "vmap_table.h"

namespace art {

size_t CodeGenerator::GetCacheOffset(uint32_t index) {
  return mirror::ObjectArray<mirror::Object>::OffsetOfElement(index).SizeValue();
}

static bool IsSingleGoto(HBasicBlock* block) {
  HLoopInformation* loop_info = block->GetLoopInformation();
  // TODO: Remove the null check b/19084197.
  return (block->GetFirstInstruction() != nullptr)
      && (block->GetFirstInstruction() == block->GetLastInstruction())
      && block->GetLastInstruction()->IsGoto()
      // Back edges generate the suspend check.
      && (loop_info == nullptr || !loop_info->IsBackEdge(block));
}

void CodeGenerator::CompileBaseline(CodeAllocator* allocator, bool is_leaf) {
  Initialize();
  if (!is_leaf) {
    MarkNotLeaf();
  }
  InitializeCodeGeneration(GetGraph()->GetNumberOfLocalVRegs()
                             + GetGraph()->GetTemporariesVRegSlots()
                             + 1 /* filler */,
                           0, /* the baseline compiler does not have live registers at slow path */
                           0, /* the baseline compiler does not have live registers at slow path */
                           GetGraph()->GetMaximumNumberOfOutVRegs()
                             + 1 /* current method */,
                           GetGraph()->GetBlocks());
  CompileInternal(allocator, /* is_baseline */ true);
}

bool CodeGenerator::GoesToNextBlock(HBasicBlock* current, HBasicBlock* next) const {
  DCHECK_EQ(block_order_->Get(current_block_index_), current);
  return GetNextBlockToEmit() == FirstNonEmptyBlock(next);
}

HBasicBlock* CodeGenerator::GetNextBlockToEmit() const {
  for (size_t i = current_block_index_ + 1; i < block_order_->Size(); ++i) {
    HBasicBlock* block = block_order_->Get(i);
    if (!IsSingleGoto(block)) {
      return block;
    }
  }
  return nullptr;
}

HBasicBlock* CodeGenerator::FirstNonEmptyBlock(HBasicBlock* block) const {
  while (IsSingleGoto(block)) {
    block = block->GetSuccessors().Get(0);
  }
  return block;
}

void CodeGenerator::CompileInternal(CodeAllocator* allocator, bool is_baseline) {
  HGraphVisitor* instruction_visitor = GetInstructionVisitor();
  DCHECK_EQ(current_block_index_, 0u);
  GenerateFrameEntry();
  for (size_t e = block_order_->Size(); current_block_index_ < e; ++current_block_index_) {
    HBasicBlock* block = block_order_->Get(current_block_index_);
    // Don't generate code for an empty block. Its predecessors will branch to its successor
    // directly. Also, the label of that block will not be emitted, so this helps catch
    // errors where we reference that label.
    if (IsSingleGoto(block)) continue;
    Bind(block);
    for (HInstructionIterator it(block->GetInstructions()); !it.Done(); it.Advance()) {
      HInstruction* current = it.Current();
      if (is_baseline) {
        InitLocationsBaseline(current);
      }
      current->Accept(instruction_visitor);
    }
  }

  // Generate the slow paths.
  for (size_t i = 0, e = slow_paths_.Size(); i < e; ++i) {
    slow_paths_.Get(i)->EmitNativeCode(this);
  }

  // Finalize instructions in assember;
  Finalize(allocator);
}

void CodeGenerator::CompileOptimized(CodeAllocator* allocator) {
  // The register allocator already called `InitializeCodeGeneration`,
  // where the frame size has been computed.
  DCHECK(block_order_ != nullptr);
  Initialize();
  CompileInternal(allocator, /* is_baseline */ false);
}

void CodeGenerator::Finalize(CodeAllocator* allocator) {
  size_t code_size = GetAssembler()->CodeSize();
  uint8_t* buffer = allocator->Allocate(code_size);

  MemoryRegion code(buffer, code_size);
  GetAssembler()->FinalizeInstructions(code);
}

size_t CodeGenerator::FindFreeEntry(bool* array, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    if (!array[i]) {
      array[i] = true;
      return i;
    }
  }
  LOG(FATAL) << "Could not find a register in baseline register allocator";
  UNREACHABLE();
  return -1;
}

size_t CodeGenerator::FindTwoFreeConsecutiveAlignedEntries(bool* array, size_t length) {
  for (size_t i = 0; i < length - 1; i += 2) {
    if (!array[i] && !array[i + 1]) {
      array[i] = true;
      array[i + 1] = true;
      return i;
    }
  }
  LOG(FATAL) << "Could not find a register in baseline register allocator";
  UNREACHABLE();
  return -1;
}

void CodeGenerator::InitializeCodeGeneration(size_t number_of_spill_slots,
                                             size_t maximum_number_of_live_core_registers,
                                             size_t maximum_number_of_live_fp_registers,
                                             size_t number_of_out_slots,
                                             const GrowableArray<HBasicBlock*>& block_order) {
  block_order_ = &block_order;
  DCHECK(block_order_->Get(0) == GetGraph()->GetEntryBlock());
  DCHECK(GoesToNextBlock(GetGraph()->GetEntryBlock(), block_order_->Get(1)));
  ComputeSpillMask();
  first_register_slot_in_slow_path_ = (number_of_out_slots + number_of_spill_slots) * kVRegSize;

  if (number_of_spill_slots == 0
      && !HasAllocatedCalleeSaveRegisters()
      && IsLeafMethod()
      && !RequiresCurrentMethod()) {
    DCHECK_EQ(maximum_number_of_live_core_registers, 0u);
    DCHECK_EQ(maximum_number_of_live_fp_registers, 0u);
    SetFrameSize(CallPushesPC() ? GetWordSize() : 0);
  } else {
    SetFrameSize(RoundUp(
        number_of_spill_slots * kVRegSize
        + number_of_out_slots * kVRegSize
        + maximum_number_of_live_core_registers * GetWordSize()
        + maximum_number_of_live_fp_registers * GetFloatingPointSpillSlotSize()
        + FrameEntrySpillSize(),
        kStackAlignment));
  }
}

Location CodeGenerator::GetTemporaryLocation(HTemporary* temp) const {
  uint16_t number_of_locals = GetGraph()->GetNumberOfLocalVRegs();
  // The type of the previous instruction tells us if we need a single or double stack slot.
  Primitive::Type type = temp->GetType();
  int32_t temp_size = (type == Primitive::kPrimLong) || (type == Primitive::kPrimDouble) ? 2 : 1;
  // Use the temporary region (right below the dex registers).
  int32_t slot = GetFrameSize() - FrameEntrySpillSize()
                                - kVRegSize  // filler
                                - (number_of_locals * kVRegSize)
                                - ((temp_size + temp->GetIndex()) * kVRegSize);
  return temp_size == 2 ? Location::DoubleStackSlot(slot) : Location::StackSlot(slot);
}

int32_t CodeGenerator::GetStackSlot(HLocal* local) const {
  uint16_t reg_number = local->GetRegNumber();
  uint16_t number_of_locals = GetGraph()->GetNumberOfLocalVRegs();
  if (reg_number >= number_of_locals) {
    // Local is a parameter of the method. It is stored in the caller's frame.
    return GetFrameSize() + kVRegSize  // ART method
                          + (reg_number - number_of_locals) * kVRegSize;
  } else {
    // Local is a temporary in this method. It is stored in this method's frame.
    return GetFrameSize() - FrameEntrySpillSize()
                          - kVRegSize  // filler.
                          - (number_of_locals * kVRegSize)
                          + (reg_number * kVRegSize);
  }
}

void CodeGenerator::AllocateRegistersLocally(HInstruction* instruction) const {
  LocationSummary* locations = instruction->GetLocations();
  if (locations == nullptr) return;

  for (size_t i = 0, e = GetNumberOfCoreRegisters(); i < e; ++i) {
    blocked_core_registers_[i] = false;
  }

  for (size_t i = 0, e = GetNumberOfFloatingPointRegisters(); i < e; ++i) {
    blocked_fpu_registers_[i] = false;
  }

  for (size_t i = 0, e = number_of_register_pairs_; i < e; ++i) {
    blocked_register_pairs_[i] = false;
  }

  // Mark all fixed input, temp and output registers as used.
  for (size_t i = 0, e = locations->GetInputCount(); i < e; ++i) {
    Location loc = locations->InAt(i);
    // The DCHECKS below check that a register is not specified twice in
    // the summary.
    if (loc.IsRegister()) {
      DCHECK(!blocked_core_registers_[loc.reg()]);
      blocked_core_registers_[loc.reg()] = true;
    } else if (loc.IsFpuRegister()) {
      DCHECK(!blocked_fpu_registers_[loc.reg()]);
      blocked_fpu_registers_[loc.reg()] = true;
    } else if (loc.IsFpuRegisterPair()) {
      DCHECK(!blocked_fpu_registers_[loc.AsFpuRegisterPairLow<int>()]);
      blocked_fpu_registers_[loc.AsFpuRegisterPairLow<int>()] = true;
      DCHECK(!blocked_fpu_registers_[loc.AsFpuRegisterPairHigh<int>()]);
      blocked_fpu_registers_[loc.AsFpuRegisterPairHigh<int>()] = true;
    } else if (loc.IsRegisterPair()) {
      DCHECK(!blocked_core_registers_[loc.AsRegisterPairLow<int>()]);
      blocked_core_registers_[loc.AsRegisterPairLow<int>()] = true;
      DCHECK(!blocked_core_registers_[loc.AsRegisterPairHigh<int>()]);
      blocked_core_registers_[loc.AsRegisterPairHigh<int>()] = true;
    }
  }

  for (size_t i = 0, e = locations->GetTempCount(); i < e; ++i) {
    Location loc = locations->GetTemp(i);
    // The DCHECKS below check that a register is not specified twice in
    // the summary.
    if (loc.IsRegister()) {
      DCHECK(!blocked_core_registers_[loc.reg()]);
      blocked_core_registers_[loc.reg()] = true;
    } else if (loc.IsFpuRegister()) {
      DCHECK(!blocked_fpu_registers_[loc.reg()]);
      blocked_fpu_registers_[loc.reg()] = true;
    } else {
      DCHECK(loc.GetPolicy() == Location::kRequiresRegister
             || loc.GetPolicy() == Location::kRequiresFpuRegister);
    }
  }

  static constexpr bool kBaseline = true;
  SetupBlockedRegisters(kBaseline);

  // Allocate all unallocated input locations.
  for (size_t i = 0, e = locations->GetInputCount(); i < e; ++i) {
    Location loc = locations->InAt(i);
    HInstruction* input = instruction->InputAt(i);
    if (loc.IsUnallocated()) {
      if ((loc.GetPolicy() == Location::kRequiresRegister)
          || (loc.GetPolicy() == Location::kRequiresFpuRegister)) {
        loc = AllocateFreeRegister(input->GetType());
      } else {
        DCHECK_EQ(loc.GetPolicy(), Location::kAny);
        HLoadLocal* load = input->AsLoadLocal();
        if (load != nullptr) {
          loc = GetStackLocation(load);
        } else {
          loc = AllocateFreeRegister(input->GetType());
        }
      }
      locations->SetInAt(i, loc);
    }
  }

  // Allocate all unallocated temp locations.
  for (size_t i = 0, e = locations->GetTempCount(); i < e; ++i) {
    Location loc = locations->GetTemp(i);
    if (loc.IsUnallocated()) {
      switch (loc.GetPolicy()) {
        case Location::kRequiresRegister:
          // Allocate a core register (large enough to fit a 32-bit integer).
          loc = AllocateFreeRegister(Primitive::kPrimInt);
          break;

        case Location::kRequiresFpuRegister:
          // Allocate a core register (large enough to fit a 64-bit double).
          loc = AllocateFreeRegister(Primitive::kPrimDouble);
          break;

        default:
          LOG(FATAL) << "Unexpected policy for temporary location "
                     << loc.GetPolicy();
      }
      locations->SetTempAt(i, loc);
    }
  }
  Location result_location = locations->Out();
  if (result_location.IsUnallocated()) {
    switch (result_location.GetPolicy()) {
      case Location::kAny:
      case Location::kRequiresRegister:
      case Location::kRequiresFpuRegister:
        result_location = AllocateFreeRegister(instruction->GetType());
        break;
      case Location::kSameAsFirstInput:
        result_location = locations->InAt(0);
        break;
    }
    locations->UpdateOut(result_location);
  }
}

void CodeGenerator::InitLocationsBaseline(HInstruction* instruction) {
  AllocateLocations(instruction);
  if (instruction->GetLocations() == nullptr) {
    if (instruction->IsTemporary()) {
      HInstruction* previous = instruction->GetPrevious();
      Location temp_location = GetTemporaryLocation(instruction->AsTemporary());
      Move(previous, temp_location, instruction);
    }
    return;
  }
  AllocateRegistersLocally(instruction);
  for (size_t i = 0, e = instruction->InputCount(); i < e; ++i) {
    Location location = instruction->GetLocations()->InAt(i);
    HInstruction* input = instruction->InputAt(i);
    if (location.IsValid()) {
      // Move the input to the desired location.
      if (input->GetNext()->IsTemporary()) {
        // If the input was stored in a temporary, use that temporary to
        // perform the move.
        Move(input->GetNext(), location, instruction);
      } else {
        Move(input, location, instruction);
      }
    }
  }
}

void CodeGenerator::AllocateLocations(HInstruction* instruction) {
  instruction->Accept(GetLocationBuilder());
  LocationSummary* locations = instruction->GetLocations();
  if (!instruction->IsSuspendCheckEntry()) {
    if (locations != nullptr && locations->CanCall()) {
      MarkNotLeaf();
    }
    if (instruction->NeedsCurrentMethod()) {
      SetRequiresCurrentMethod();
    }
  }
}

CodeGenerator* CodeGenerator::Create(HGraph* graph,
                                     InstructionSet instruction_set,
                                     const InstructionSetFeatures& isa_features,
                                     const CompilerOptions& compiler_options) {
  switch (instruction_set) {
    case kArm:
    case kThumb2: {
      return new arm::CodeGeneratorARM(graph,
          *isa_features.AsArmInstructionSetFeatures(),
          compiler_options);
    }
    case kArm64: {
      return new arm64::CodeGeneratorARM64(graph,
          *isa_features.AsArm64InstructionSetFeatures(),
          compiler_options);
    }
    case kMips:
      return nullptr;
    case kX86: {
      return new x86::CodeGeneratorX86(graph, compiler_options);
    }
    case kX86_64: {
      return new x86_64::CodeGeneratorX86_64(graph, compiler_options);
    }
    default:
      return nullptr;
  }
}

void CodeGenerator::BuildNativeGCMap(
    std::vector<uint8_t>* data, const DexCompilationUnit& dex_compilation_unit) const {
  const std::vector<uint8_t>& gc_map_raw =
      dex_compilation_unit.GetVerifiedMethod()->GetDexGcMap();
  verifier::DexPcToReferenceMap dex_gc_map(&(gc_map_raw)[0]);

  uint32_t max_native_offset = 0;
  for (size_t i = 0; i < pc_infos_.Size(); i++) {
    uint32_t native_offset = pc_infos_.Get(i).native_pc;
    if (native_offset > max_native_offset) {
      max_native_offset = native_offset;
    }
  }

  GcMapBuilder builder(data, pc_infos_.Size(), max_native_offset, dex_gc_map.RegWidth());
  for (size_t i = 0; i < pc_infos_.Size(); i++) {
    struct PcInfo pc_info = pc_infos_.Get(i);
    uint32_t native_offset = pc_info.native_pc;
    uint32_t dex_pc = pc_info.dex_pc;
    const uint8_t* references = dex_gc_map.FindBitMap(dex_pc, false);
    CHECK(references != nullptr) << "Missing ref for dex pc 0x" << std::hex << dex_pc;
    builder.AddEntry(native_offset, references);
  }
}

void CodeGenerator::BuildMappingTable(std::vector<uint8_t>* data, DefaultSrcMap* src_map) const {
  uint32_t pc2dex_data_size = 0u;
  uint32_t pc2dex_entries = pc_infos_.Size();
  uint32_t pc2dex_offset = 0u;
  int32_t pc2dex_dalvik_offset = 0;
  uint32_t dex2pc_data_size = 0u;
  uint32_t dex2pc_entries = 0u;
  uint32_t dex2pc_offset = 0u;
  int32_t dex2pc_dalvik_offset = 0;

  if (src_map != nullptr) {
    src_map->reserve(pc2dex_entries);
  }

  for (size_t i = 0; i < pc2dex_entries; i++) {
    struct PcInfo pc_info = pc_infos_.Get(i);
    pc2dex_data_size += UnsignedLeb128Size(pc_info.native_pc - pc2dex_offset);
    pc2dex_data_size += SignedLeb128Size(pc_info.dex_pc - pc2dex_dalvik_offset);
    pc2dex_offset = pc_info.native_pc;
    pc2dex_dalvik_offset = pc_info.dex_pc;
    if (src_map != nullptr) {
      src_map->push_back(SrcMapElem({pc2dex_offset, pc2dex_dalvik_offset}));
    }
  }

  // Walk over the blocks and find which ones correspond to catch block entries.
  for (size_t i = 0; i < graph_->GetBlocks().Size(); ++i) {
    HBasicBlock* block = graph_->GetBlocks().Get(i);
    if (block->IsCatchBlock()) {
      intptr_t native_pc = GetAddressOf(block);
      ++dex2pc_entries;
      dex2pc_data_size += UnsignedLeb128Size(native_pc - dex2pc_offset);
      dex2pc_data_size += SignedLeb128Size(block->GetDexPc() - dex2pc_dalvik_offset);
      dex2pc_offset = native_pc;
      dex2pc_dalvik_offset = block->GetDexPc();
    }
  }

  uint32_t total_entries = pc2dex_entries + dex2pc_entries;
  uint32_t hdr_data_size = UnsignedLeb128Size(total_entries) + UnsignedLeb128Size(pc2dex_entries);
  uint32_t data_size = hdr_data_size + pc2dex_data_size + dex2pc_data_size;
  data->resize(data_size);

  uint8_t* data_ptr = &(*data)[0];
  uint8_t* write_pos = data_ptr;

  write_pos = EncodeUnsignedLeb128(write_pos, total_entries);
  write_pos = EncodeUnsignedLeb128(write_pos, pc2dex_entries);
  DCHECK_EQ(static_cast<size_t>(write_pos - data_ptr), hdr_data_size);
  uint8_t* write_pos2 = write_pos + pc2dex_data_size;

  pc2dex_offset = 0u;
  pc2dex_dalvik_offset = 0u;
  dex2pc_offset = 0u;
  dex2pc_dalvik_offset = 0u;

  for (size_t i = 0; i < pc2dex_entries; i++) {
    struct PcInfo pc_info = pc_infos_.Get(i);
    DCHECK(pc2dex_offset <= pc_info.native_pc);
    write_pos = EncodeUnsignedLeb128(write_pos, pc_info.native_pc - pc2dex_offset);
    write_pos = EncodeSignedLeb128(write_pos, pc_info.dex_pc - pc2dex_dalvik_offset);
    pc2dex_offset = pc_info.native_pc;
    pc2dex_dalvik_offset = pc_info.dex_pc;
  }

  for (size_t i = 0; i < graph_->GetBlocks().Size(); ++i) {
    HBasicBlock* block = graph_->GetBlocks().Get(i);
    if (block->IsCatchBlock()) {
      intptr_t native_pc = GetAddressOf(block);
      write_pos2 = EncodeUnsignedLeb128(write_pos2, native_pc - dex2pc_offset);
      write_pos2 = EncodeSignedLeb128(write_pos2, block->GetDexPc() - dex2pc_dalvik_offset);
      dex2pc_offset = native_pc;
      dex2pc_dalvik_offset = block->GetDexPc();
    }
  }


  DCHECK_EQ(static_cast<size_t>(write_pos - data_ptr), hdr_data_size + pc2dex_data_size);
  DCHECK_EQ(static_cast<size_t>(write_pos2 - data_ptr), data_size);

  if (kIsDebugBuild) {
    // Verify the encoded table holds the expected data.
    MappingTable table(data_ptr);
    CHECK_EQ(table.TotalSize(), total_entries);
    CHECK_EQ(table.PcToDexSize(), pc2dex_entries);
    auto it = table.PcToDexBegin();
    auto it2 = table.DexToPcBegin();
    for (size_t i = 0; i < pc2dex_entries; i++) {
      struct PcInfo pc_info = pc_infos_.Get(i);
      CHECK_EQ(pc_info.native_pc, it.NativePcOffset());
      CHECK_EQ(pc_info.dex_pc, it.DexPc());
      ++it;
    }
    for (size_t i = 0; i < graph_->GetBlocks().Size(); ++i) {
      HBasicBlock* block = graph_->GetBlocks().Get(i);
      if (block->IsCatchBlock()) {
        CHECK_EQ(GetAddressOf(block), it2.NativePcOffset());
        CHECK_EQ(block->GetDexPc(), it2.DexPc());
        ++it2;
      }
    }
    CHECK(it == table.PcToDexEnd());
    CHECK(it2 == table.DexToPcEnd());
  }
}

void CodeGenerator::BuildVMapTable(std::vector<uint8_t>* data) const {
  Leb128EncodingVector vmap_encoder;
  // We currently don't use callee-saved registers.
  size_t size = 0 + 1 /* marker */ + 0;
  vmap_encoder.Reserve(size + 1u);  // All values are likely to be one byte in ULEB128 (<128).
  vmap_encoder.PushBackUnsigned(size);
  vmap_encoder.PushBackUnsigned(VmapTable::kAdjustedFpMarker);

  *data = vmap_encoder.GetData();
}

void CodeGenerator::BuildStackMaps(std::vector<uint8_t>* data) {
  uint32_t size = stack_map_stream_.ComputeNeededSize();
  data->resize(size);
  MemoryRegion region(data->data(), size);
  stack_map_stream_.FillIn(region);
}

void CodeGenerator::RecordPcInfo(HInstruction* instruction, uint32_t dex_pc) {
  if (instruction != nullptr) {
    // The code generated for some type conversions may call the
    // runtime, thus normally requiring a subsequent call to this
    // method.  However, the method verifier does not produce PC
    // information for certain instructions, which are considered "atomic"
    // (they cannot join a GC).
    // Therefore we do not currently record PC information for such
    // instructions.  As this may change later, we added this special
    // case so that code generators may nevertheless call
    // CodeGenerator::RecordPcInfo without triggering an error in
    // CodeGenerator::BuildNativeGCMap ("Missing ref for dex pc 0x")
    // thereafter.
    if (instruction->IsTypeConversion()) {
      return;
    }
    if (instruction->IsRem()) {
      Primitive::Type type = instruction->AsRem()->GetResultType();
      if ((type == Primitive::kPrimFloat) || (type == Primitive::kPrimDouble)) {
        return;
      }
    }
  }

  // Collect PC infos for the mapping table.
  struct PcInfo pc_info;
  pc_info.dex_pc = dex_pc;
  pc_info.native_pc = GetAssembler()->CodeSize();
  pc_infos_.Add(pc_info);

  if (instruction == nullptr) {
    stack_map_stream_.RecordEnvironment(
       nullptr, 0, nullptr, 0, dex_pc, pc_info.native_pc);
  } else {
    LocationSummary* locations = instruction->GetLocations();
    HEnvironment* environment = instruction->GetEnvironment();
    size_t environment_size = instruction->EnvironmentSize();

    uint32_t register_mask = locations->GetRegisterMask();
    if (locations->OnlyCallsOnSlowPath()) {
      // In case of slow path, we currently set the location of caller-save registers
      // to register (instead of their stack location when pushed before the slow-path
      // call). Therefore register_mask contains both callee-save and caller-save
      // registers that hold objects. We must remove the caller-save from the mask, since
      // they will be overwritten by the callee.
      register_mask &= core_callee_save_mask_;
    }
    // The register mask must be a subset of callee-save registers.
    DCHECK_EQ(register_mask & core_callee_save_mask_, register_mask);

    // Populate stack map information.
    stack_map_stream_.RecordEnvironment(
        environment, environment_size, locations, dex_pc, pc_info.native_pc, register_mask);
  }
}

bool CodeGenerator::CanMoveNullCheckToUser(HNullCheck* null_check) {
  HInstruction* first_next_not_move = null_check->GetNextDisregardingMoves();
  return (first_next_not_move != nullptr) && first_next_not_move->CanDoImplicitNullCheck();
}

void CodeGenerator::MaybeRecordImplicitNullCheck(HInstruction* instr) {
  // If we are from a static path don't record the pc as we can't throw NPE.
  // NB: having the checks here makes the code much less verbose in the arch
  // specific code generators.
  if (instr->IsStaticFieldSet() || instr->IsStaticFieldGet()) {
    return;
  }

  if (!compiler_options_.GetImplicitNullChecks()) {
    return;
  }

  if (!instr->CanDoImplicitNullCheck()) {
    return;
  }

  // Find the first previous instruction which is not a move.
  HInstruction* first_prev_not_move = instr->GetPreviousDisregardingMoves();

  // If the instruction is a null check it means that `instr` is the first user
  // and needs to record the pc.
  if (first_prev_not_move != nullptr && first_prev_not_move->IsNullCheck()) {
    HNullCheck* null_check = first_prev_not_move->AsNullCheck();
    // TODO: The parallel moves modify the environment. Their changes need to be reverted
    // otherwise the stack maps at the throw point will not be correct.
    RecordPcInfo(null_check, null_check->GetDexPc());
  }
}

void CodeGenerator::SaveLiveRegisters(LocationSummary* locations) {
  RegisterSet* register_set = locations->GetLiveRegisters();
  size_t stack_offset = first_register_slot_in_slow_path_;
  for (size_t i = 0, e = GetNumberOfCoreRegisters(); i < e; ++i) {
    if (!IsCoreCalleeSaveRegister(i)) {
      if (register_set->ContainsCoreRegister(i)) {
        // If the register holds an object, update the stack mask.
        if (locations->RegisterContainsObject(i)) {
          locations->SetStackBit(stack_offset / kVRegSize);
        }
        DCHECK_LT(stack_offset, GetFrameSize() - FrameEntrySpillSize());
        stack_offset += SaveCoreRegister(stack_offset, i);
      }
    }
  }

  for (size_t i = 0, e = GetNumberOfFloatingPointRegisters(); i < e; ++i) {
    if (!IsFloatingPointCalleeSaveRegister(i)) {
      if (register_set->ContainsFloatingPointRegister(i)) {
        DCHECK_LT(stack_offset, GetFrameSize() - FrameEntrySpillSize());
        stack_offset += SaveFloatingPointRegister(stack_offset, i);
      }
    }
  }
}

void CodeGenerator::RestoreLiveRegisters(LocationSummary* locations) {
  RegisterSet* register_set = locations->GetLiveRegisters();
  size_t stack_offset = first_register_slot_in_slow_path_;
  for (size_t i = 0, e = GetNumberOfCoreRegisters(); i < e; ++i) {
    if (!IsCoreCalleeSaveRegister(i)) {
      if (register_set->ContainsCoreRegister(i)) {
        DCHECK_LT(stack_offset, GetFrameSize() - FrameEntrySpillSize());
        stack_offset += RestoreCoreRegister(stack_offset, i);
      }
    }
  }

  for (size_t i = 0, e = GetNumberOfFloatingPointRegisters(); i < e; ++i) {
    if (!IsFloatingPointCalleeSaveRegister(i)) {
      if (register_set->ContainsFloatingPointRegister(i)) {
        DCHECK_LT(stack_offset, GetFrameSize() - FrameEntrySpillSize());
        stack_offset += RestoreFloatingPointRegister(stack_offset, i);
      }
    }
  }
}

void CodeGenerator::ClearSpillSlotsFromLoopPhisInStackMap(HSuspendCheck* suspend_check) const {
  LocationSummary* locations = suspend_check->GetLocations();
  HBasicBlock* block = suspend_check->GetBlock();
  DCHECK(block->GetLoopInformation()->GetSuspendCheck() == suspend_check);
  DCHECK(block->IsLoopHeader());

  for (HInstructionIterator it(block->GetPhis()); !it.Done(); it.Advance()) {
    HInstruction* current = it.Current();
    LiveInterval* interval = current->GetLiveInterval();
    // We only need to clear bits of loop phis containing objects and allocated in register.
    // Loop phis allocated on stack already have the object in the stack.
    if (current->GetType() == Primitive::kPrimNot
        && interval->HasRegister()
        && interval->HasSpillSlot()) {
      locations->ClearStackBit(interval->GetSpillSlot() / kVRegSize);
    }
  }
}

void CodeGenerator::EmitParallelMoves(Location from1, Location to1, Location from2, Location to2) {
  HParallelMove parallel_move(GetGraph()->GetArena());
  parallel_move.AddMove(from1, to1, nullptr);
  parallel_move.AddMove(from2, to2, nullptr);
  GetMoveResolver()->EmitNativeCode(&parallel_move);
}

}  // namespace art
