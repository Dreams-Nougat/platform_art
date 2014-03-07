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

/* This file contains codegen for the Mips ISA */

#include "codegen_mips.h"
#include "dex/quick/mir_to_lir-inl.h"
#include "entrypoints/quick/quick_entrypoints.h"
#include "mips_lir.h"

namespace art {

bool MipsMir2Lir::GenSpecialCase(BasicBlock* bb, MIR* mir,
                                 const InlineMethod& special) {
  // TODO
  return false;
}

/*
 * The lack of pc-relative loads on Mips presents somewhat of a challenge
 * for our PIC switch table strategy.  To materialize the current location
 * we'll do a dummy JAL and reference our tables using r_RA as the
 * base register.  Note that r_RA will be used both as the base to
 * locate the switch table data and as the reference base for the switch
 * target offsets stored in the table.  We'll use a special pseudo-instruction
 * to represent the jal and trigger the construction of the
 * switch table offsets (which will happen after final assembly and all
 * labels are fixed).
 *
 * The test loop will look something like:
 *
 *   ori   r_end, r_ZERO, #table_size  ; size in bytes
 *   jal   BaseLabel         ; stores "return address" (BaseLabel) in r_RA
 *   nop                     ; opportunistically fill
 * BaseLabel:
 *   addiu r_base, r_RA, <table> - <BaseLabel>    ; table relative to BaseLabel
     addu  r_end, r_end, r_base                   ; end of table
 *   lw    r_val, [rSP, v_reg_off]                ; Test Value
 * loop:
 *   beq   r_base, r_end, done
 *   lw    r_key, 0(r_base)
 *   addu  r_base, 8
 *   bne   r_val, r_key, loop
 *   lw    r_disp, -4(r_base)
 *   addu  r_RA, r_disp
 *   jr    r_RA
 * done:
 *
 */
void MipsMir2Lir::GenSparseSwitch(MIR* mir, DexOffset table_offset,
                                  RegLocation rl_src) {
  const uint16_t* table = cu_->insns + current_dalvik_offset_ + table_offset;
  if (cu_->verbose) {
    DumpSparseSwitchTable(table);
  }
  // Add the table to the list - we'll process it later
  SwitchTable* tab_rec =
      static_cast<SwitchTable*>(arena_->Alloc(sizeof(SwitchTable), kArenaAllocData));
  tab_rec->table = table;
  tab_rec->vaddr = current_dalvik_offset_;
  int elements = table[1];
  tab_rec->targets =
      static_cast<LIR**>(arena_->Alloc(elements * sizeof(LIR*), kArenaAllocLIR));
  switch_tables_.Insert(tab_rec);

  // The table is composed of 8-byte key/disp pairs
  int byte_size = elements * 8;

  int size_hi = byte_size >> 16;
  int size_lo = byte_size & 0xffff;

  RegStorage r_end = AllocTemp();
  if (size_hi) {
    NewLIR2(kMipsLui, r_end.GetReg(), size_hi);
  }
  // Must prevent code motion for the curr pc pair
  GenBarrier();  // Scheduling barrier
  NewLIR0(kMipsCurrPC);  // Really a jal to .+8
  // Now, fill the branch delay slot
  if (size_hi) {
    NewLIR3(kMipsOri, r_end.GetReg(), r_end.GetReg(), size_lo);
  } else {
    NewLIR3(kMipsOri, r_end.GetReg(), r_ZERO, size_lo);
  }
  GenBarrier();  // Scheduling barrier

  // Construct BaseLabel and set up table base register
  LIR* base_label = NewLIR0(kPseudoTargetLabel);
  // Remember base label so offsets can be computed later
  tab_rec->anchor = base_label;
  RegStorage r_base = AllocTemp();
  NewLIR4(kMipsDelta, r_base.GetReg(), 0, WrapPointer(base_label), WrapPointer(tab_rec));
  OpRegRegReg(kOpAdd, r_end, r_end, r_base);

  // Grab switch test value
  rl_src = LoadValue(rl_src, kCoreReg);

  // Test loop
  RegStorage r_key = AllocTemp();
  LIR* loop_label = NewLIR0(kPseudoTargetLabel);
  LIR* exit_branch = OpCmpBranch(kCondEq, r_base, r_end, NULL);
  LoadWordDisp(r_base, 0, r_key);
  OpRegImm(kOpAdd, r_base, 8);
  OpCmpBranch(kCondNe, rl_src.reg, r_key, loop_label);
  RegStorage r_disp = AllocTemp();
  LoadWordDisp(r_base, -4, r_disp);
  OpRegRegReg(kOpAdd, rs_RA, rs_RA, r_disp);
  OpReg(kOpBx, rs_RA);

  // Loop exit
  LIR* exit_label = NewLIR0(kPseudoTargetLabel);
  exit_branch->target = exit_label;
}

/*
 * Code pattern will look something like:
 *
 *   lw    r_val
 *   jal   BaseLabel         ; stores "return address" (BaseLabel) in r_RA
 *   nop                     ; opportunistically fill
 *   [subiu r_val, bias]      ; Remove bias if low_val != 0
 *   bound check -> done
 *   lw    r_disp, [r_RA, r_val]
 *   addu  r_RA, r_disp
 *   jr    r_RA
 * done:
 */
void MipsMir2Lir::GenPackedSwitch(MIR* mir, DexOffset table_offset,
                                  RegLocation rl_src) {
  const uint16_t* table = cu_->insns + current_dalvik_offset_ + table_offset;
  if (cu_->verbose) {
    DumpPackedSwitchTable(table);
  }
  // Add the table to the list - we'll process it later
  SwitchTable* tab_rec =
      static_cast<SwitchTable*>(arena_->Alloc(sizeof(SwitchTable), kArenaAllocData));
  tab_rec->table = table;
  tab_rec->vaddr = current_dalvik_offset_;
  int size = table[1];
  tab_rec->targets = static_cast<LIR**>(arena_->Alloc(size * sizeof(LIR*),
                                                      kArenaAllocLIR));
  switch_tables_.Insert(tab_rec);

  // Get the switch value
  rl_src = LoadValue(rl_src, kCoreReg);

  // Prepare the bias.  If too big, handle 1st stage here
  int low_key = s4FromSwitchData(&table[2]);
  bool large_bias = false;
  RegStorage r_key;
  if (low_key == 0) {
    r_key = rl_src.reg;
  } else if ((low_key & 0xffff) != low_key) {
    r_key = AllocTemp();
    LoadConstant(r_key, low_key);
    large_bias = true;
  } else {
    r_key = AllocTemp();
  }

  // Must prevent code motion for the curr pc pair
  GenBarrier();
  NewLIR0(kMipsCurrPC);  // Really a jal to .+8
  // Now, fill the branch delay slot with bias strip
  if (low_key == 0) {
    NewLIR0(kMipsNop);
  } else {
    if (large_bias) {
      OpRegRegReg(kOpSub, r_key, rl_src.reg, r_key);
    } else {
      OpRegRegImm(kOpSub, r_key, rl_src.reg, low_key);
    }
  }
  GenBarrier();  // Scheduling barrier

  // Construct BaseLabel and set up table base register
  LIR* base_label = NewLIR0(kPseudoTargetLabel);
  // Remember base label so offsets can be computed later
  tab_rec->anchor = base_label;

  // Bounds check - if < 0 or >= size continue following switch
  LIR* branch_over = OpCmpImmBranch(kCondHi, r_key, size-1, NULL);

  // Materialize the table base pointer
  RegStorage r_base = AllocTemp();
  NewLIR4(kMipsDelta, r_base.GetReg(), 0, WrapPointer(base_label), WrapPointer(tab_rec));

  // Load the displacement from the switch table
  RegStorage r_disp = AllocTemp();
  LoadBaseIndexed(r_base.GetReg(), r_key.GetReg(), r_disp.GetReg(), 2, kWord);

  // Add to r_AP and go
  OpRegRegReg(kOpAdd, rs_RA, rs_RA, r_disp);
  OpReg(kOpBx, rs_RA);

  /* branch_over target here */
  LIR* target = NewLIR0(kPseudoTargetLabel);
  branch_over->target = target;
}

/*
 * Array data table format:
 *  ushort ident = 0x0300   magic value
 *  ushort width            width of each element in the table
 *  uint   size             number of elements in the table
 *  ubyte  data[size*width] table of data values (may contain a single-byte
 *                          padding at the end)
 *
 * Total size is 4+(width * size + 1)/2 16-bit code units.
 */
void MipsMir2Lir::GenFillArrayData(DexOffset table_offset, RegLocation rl_src) {
  const uint16_t* table = cu_->insns + current_dalvik_offset_ + table_offset;
  // Add the table to the list - we'll process it later
  FillArrayData* tab_rec =
      reinterpret_cast<FillArrayData*>(arena_->Alloc(sizeof(FillArrayData),
                                                     kArenaAllocData));
  tab_rec->table = table;
  tab_rec->vaddr = current_dalvik_offset_;
  uint16_t width = tab_rec->table[1];
  uint32_t size = tab_rec->table[2] | ((static_cast<uint32_t>(tab_rec->table[3])) << 16);
  tab_rec->size = (size * width) + 8;

  fill_array_data_.Insert(tab_rec);

  // Making a call - use explicit registers
  FlushAllRegs();   /* Everything to home location */
  LockCallTemps();
  LoadValueDirectFixed(rl_src, RegStorage(RegStorage::k32BitSolo, rMIPS_ARG0));

  // Must prevent code motion for the curr pc pair
  GenBarrier();
  NewLIR0(kMipsCurrPC);  // Really a jal to .+8
  // Now, fill the branch delay slot with the helper load
  RegStorage r_tgt(RegStorage::k32BitSolo,
                   LoadHelper(QUICK_ENTRYPOINT_OFFSET(pHandleFillArrayData)));
  GenBarrier();  // Scheduling barrier

  // Construct BaseLabel and set up table base register
  LIR* base_label = NewLIR0(kPseudoTargetLabel);

  // Materialize a pointer to the fill data image
  NewLIR4(kMipsDelta, rMIPS_ARG1, 0, WrapPointer(base_label), WrapPointer(tab_rec));

  // And go...
  ClobberCallerSave();
  LIR* call_inst = OpReg(kOpBlx, r_tgt);  // ( array*, fill_data* )
  MarkSafepointPC(call_inst);
}

void MipsMir2Lir::GenMoveException(RegLocation rl_dest) {
  int ex_offset = Thread::ExceptionOffset().Int32Value();
  RegLocation rl_result = EvalLoc(rl_dest, kCoreReg, true);
  RegStorage reset_reg = AllocTemp();
  LoadWordDisp(rs_MIPS_SELF, ex_offset, rl_result.reg);
  LoadConstant(reset_reg, 0);
  StoreWordDisp(rs_MIPS_SELF, ex_offset, reset_reg);
  FreeTemp(reset_reg);
  StoreValue(rl_dest, rl_result);
}

/*
 * Mark garbage collection card. Skip if the value we're storing is null.
 */
void MipsMir2Lir::MarkGCCard(int val_reg, int tgt_addr_reg) {
  RegStorage reg_card_base = AllocTemp();
  RegStorage reg_card_no = AllocTemp();
  LIR* branch_over = OpCmpImmBranch(kCondEq, RegStorage(RegStorage::k32BitSolo, val_reg), 0, NULL);
  LoadWordDisp(rs_MIPS_SELF, Thread::CardTableOffset().Int32Value(), reg_card_base);
  OpRegRegImm(kOpLsr, reg_card_no, RegStorage(RegStorage::k32BitSolo, tgt_addr_reg),
              gc::accounting::CardTable::kCardShift);
  StoreBaseIndexed(reg_card_base.GetReg(), reg_card_no.GetReg(), reg_card_base.GetReg(), 0,
                   kUnsignedByte);
  LIR* target = NewLIR0(kPseudoTargetLabel);
  branch_over->target = target;
  FreeTemp(reg_card_base);
  FreeTemp(reg_card_no);
}
// FIXME: temp.
void MipsMir2Lir::MarkGCCard(RegStorage val_reg, RegStorage tgt_addr_reg) {
  MarkGCCard(val_reg.GetReg(), tgt_addr_reg.GetReg());
}

void MipsMir2Lir::GenEntrySequence(RegLocation* ArgLocs, RegLocation rl_method) {
  int spill_count = num_core_spills_ + num_fp_spills_;
  /*
   * On entry, rMIPS_ARG0, rMIPS_ARG1, rMIPS_ARG2 & rMIPS_ARG3 are live.  Let the register
   * allocation mechanism know so it doesn't try to use any of them when
   * expanding the frame or flushing.  This leaves the utility
   * code with a single temp: r12.  This should be enough.
   */
  LockTemp(rMIPS_ARG0);
  LockTemp(rMIPS_ARG1);
  LockTemp(rMIPS_ARG2);
  LockTemp(rMIPS_ARG3);

  /*
   * We can safely skip the stack overflow check if we're
   * a leaf *and* our frame size < fudge factor.
   */
  bool skip_overflow_check = (mir_graph_->MethodIsLeaf() &&
      (static_cast<size_t>(frame_size_) < Thread::kStackOverflowReservedBytes));
  NewLIR0(kPseudoMethodEntry);
  RegStorage check_reg = AllocTemp();
  RegStorage new_sp = AllocTemp();
  if (!skip_overflow_check) {
    /* Load stack limit */
    LoadWordDisp(rs_MIPS_SELF, Thread::StackEndOffset().Int32Value(), check_reg);
  }
  /* Spill core callee saves */
  SpillCoreRegs();
  /* NOTE: promotion of FP regs currently unsupported, thus no FP spill */
  DCHECK_EQ(num_fp_spills_, 0);
  if (!skip_overflow_check) {
    OpRegRegImm(kOpSub, new_sp, rs_MIPS_SP, frame_size_ - (spill_count * 4));
    GenRegRegCheck(kCondUlt, new_sp, check_reg, kThrowStackOverflow);
    OpRegCopy(RegStorage(RegStorage::k32BitSolo, rMIPS_SP), new_sp);     // Establish stack
  } else {
    OpRegImm(kOpSub, rs_SP, frame_size_ - (spill_count * 4));
  }

  FlushIns(ArgLocs, rl_method);

  FreeTemp(rMIPS_ARG0);
  FreeTemp(rMIPS_ARG1);
  FreeTemp(rMIPS_ARG2);
  FreeTemp(rMIPS_ARG3);
}

void MipsMir2Lir::GenExitSequence() {
  /*
   * In the exit path, rMIPS_RET0/rMIPS_RET1 are live - make sure they aren't
   * allocated by the register utilities as temps.
   */
  LockTemp(rMIPS_RET0);
  LockTemp(rMIPS_RET1);

  NewLIR0(kPseudoMethodExit);
  UnSpillCoreRegs();
  OpReg(kOpBx, rs_RA);
}

void MipsMir2Lir::GenSpecialExitSequence() {
  OpReg(kOpBx, rs_RA);
}

}  // namespace art
