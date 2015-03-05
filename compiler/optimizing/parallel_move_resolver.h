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

#ifndef ART_COMPILER_OPTIMIZING_PARALLEL_MOVE_RESOLVER_H_
#define ART_COMPILER_OPTIMIZING_PARALLEL_MOVE_RESOLVER_H_

#include "base/value_object.h"
#include "utils/growable_array.h"
#include "locations.h"

namespace art {

class HParallelMove;
class MoveOperands;

// Helper classes to resolve a set of parallel moves. Architecture dependent code generator must
// have their own subclass that implements corresponding virtual functions.
class ParallelMoveResolver : public ValueObject {
 public:
  explicit ParallelMoveResolver(ArenaAllocator* allocator) : moves_(allocator, 32) {}
  virtual ~ParallelMoveResolver() {}

  // Resolve a set of parallel moves, emitting assembler instructions.
  virtual void EmitNativeCode(HParallelMove* parallel_move) = 0;

 protected:
  // Build the initial list of moves.
  void BuildInitialMoveList(HParallelMove* parallel_move);

  GrowableArray<MoveOperands*> moves_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ParallelMoveResolver);
};

// This helper class uses swap to resolve dependencies and may emit swap.
class ParallelMoveResolverWithSwap : public ParallelMoveResolver {
 public:
  explicit ParallelMoveResolverWithSwap(ArenaAllocator* allocator)
      : ParallelMoveResolver(allocator) {}
  virtual ~ParallelMoveResolverWithSwap() {}

  // Resolve a set of parallel moves, emitting assembler instructions.
  void EmitNativeCode(HParallelMove* parallel_move) OVERRIDE;

 protected:
  class ScratchRegisterScope : public ValueObject {
   public:
    ScratchRegisterScope(ParallelMoveResolverWithSwap* resolver,
                         int blocked,
                         int if_scratch,
                         int number_of_registers);
    ~ScratchRegisterScope();

    int GetRegister() const { return reg_; }
    bool IsSpilled() const { return spilled_; }

   private:
    ParallelMoveResolverWithSwap* resolver_;
    int reg_;
    bool spilled_;
  };

  // Return true if the location can be scratched.
  bool IsScratchLocation(Location loc);

  // Allocate a scratch register for performing a move. The method will try to use
  // a register that is the destination of a move, but that move has not been emitted yet.
  int AllocateScratchRegister(int blocked, int if_scratch, int register_count, bool* spilled);

  // Emit a move.
  virtual void EmitMove(size_t index) = 0;

  // Execute a move by emitting a swap of two operands.
  virtual void EmitSwap(size_t index) = 0;

  virtual void SpillScratch(int reg) = 0;
  virtual void RestoreScratch(int reg) = 0;

  static constexpr int kNoRegister = -1;

 private:
  // Perform the move at the moves_ index in question (possibly requiring
  // other moves to satisfy dependencies).
  //
  // Return whether another move in the dependency cycle needs to swap. This
  // is to handle pair swaps, where we want the pair to swap first to avoid
  // building pairs that are unexpected by the code generator. For example, if
  // we were to swap R1 with R2, we would need to update all locations using
  // R2 to R1. So a (R2,R3) pair register could become (R1,R3). We could make
  // the code generator understand such pairs, but it's easier and cleaner to
  // just not create such pairs and exchange pairs in priority.
  MoveOperands* PerformMove(size_t index);

  DISALLOW_COPY_AND_ASSIGN(ParallelMoveResolverWithSwap);
};

// This helper class uses additional scratch register to resolve dependencies. It supports all kind
// of dependency circles. Useful tips to simplify the subclass implementation :
//   One scratch location is enough if one location can not be blocked by multiple moves. On ARM32,
//   D0 is (S0,S1) pair which might cause the issue, i.e. S2->S0, S3->S1, D0->D2. In this case,
//   GetScratchLocationFor() should have the ability to allocate new scratch register. Also some
//   cleanup code might be needed in FinishEmitNativeCode().
class ParallelMoveResolverNoSwap : public ParallelMoveResolver {
 public:
  explicit ParallelMoveResolverNoSwap(ArenaAllocator* allocator)
      : ParallelMoveResolver(allocator), scratches_(allocator, 32),
        pending_moves_(allocator, 8), allocator_(allocator) {}
  virtual ~ParallelMoveResolverNoSwap() {}

  // Resolve a set of parallel moves, emitting assembler instructions.
  void EmitNativeCode(HParallelMove* parallel_move) OVERRIDE;

 protected:
  // Called at the beginning of EmitNativeCode(). Subclass may put some architecture dependent
  // initialization here.
  virtual void PrepareForEmitNativeCode() = 0;

  // Called at the end of EmitNativeCode(). Subclass may put some architecture dependent cleanup
  // here. All scratch locations will be removed after this call.
  virtual void FinishEmitNativeCode() = 0;

  // Allocate a scratch location for performing a move from input location. Subclass should
  // implement this to get the best fit location. If there is no suitable physical register, it can
  // also returns a stack slot. In this case, backend need to make sure it won't pollute current
  // frame.
  virtual Location AllocateScratchLocation(Location loc) = 0;

  // Called when a move has been performed which takes a scratch location as source. Subclass
  // can defer the cleanup to FinishEmitNativeCode().
  virtual void FreeScratchLocation(Location loc) = 0;

  // Emit a move.
  virtual void EmitMove(size_t index) = 0;

  // Return a scratch location from the moves which exactly matches the kind.
  // Return Location::NoLocation() if no match scratch location can be found.
  Location GetScratchLocation(Location::Kind kind);

  // Add a location to scratch list which can be returned from GetScratchLocation() to resolve
  // dependency cycles.
  void AddScratchLocation(Location loc);

  // Remove a location from scratch list.
  void RemoveScratchLocation(Location loc);

  // List of scratch locations.
  GrowableArray<Location> scratches_;

 private:
  // Perform the move at the moves_ index in question (possibly requiring
  // other moves to satisfy dependencies).
  void PerformMove(size_t index);

  // Change source location of the moves.
  void UpdateMoveSource(Location from, Location to);

  // Add one pending move which is used to resolve dependency cycle.
  void AddPendingMove(Location source, Location destination);

  // Delete pending move from pending move list.
  void DeletePendingMove(MoveOperands* move);

  // Get move which might be unblocked after we had performed move (loc->XXX).
  MoveOperands* GetUnblockedPendingMove(Location loc);

  // Return true if the location is blocked by unperformed moves.
  bool IsBlockedByMoves(Location loc);

  // Return number of pending moves.
  size_t GetNumberOfPendingMoves();

  // Additional pending moves which might be added to resolve dependency circle.
  GrowableArray<MoveOperands*> pending_moves_;

  // Used to allocate pending MoveOperands.
  ArenaAllocator* const allocator_;

  DISALLOW_COPY_AND_ASSIGN(ParallelMoveResolverNoSwap);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_PARALLEL_MOVE_RESOLVER_H_
