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

#include "linear_order.h"

namespace art {

static bool InSameLoop(HLoopInformation* first_loop, HLoopInformation* second_loop) {
  return first_loop == second_loop;
}

static bool IsLoop(HLoopInformation* info) {
  return info != nullptr;
}

static bool IsInnerLoop(HLoopInformation* outer, HLoopInformation* inner) {
  return (inner != outer)
      && (inner != nullptr)
      && (outer != nullptr)
      && inner->IsIn(*outer);
}

// Helper method to update work list for linear order.
static void AddToListForLinearization(ArenaVector<HBasicBlock*>* worklist, HBasicBlock* block) {
  HLoopInformation* block_loop = block->GetLoopInformation();
  auto insert_pos = worklist->rbegin();  // insert_pos.base() will be the actual position.
  for (auto end = worklist->rend(); insert_pos != end; ++insert_pos) {
    HBasicBlock* current = *insert_pos;
    HLoopInformation* current_loop = current->GetLoopInformation();
    if (InSameLoop(block_loop, current_loop)
        || !IsLoop(current_loop)
        || IsInnerLoop(current_loop, block_loop)) {
      // The block can be processed immediately.
      break;
    }
  }
  worklist->insert(insert_pos.base(), block);
}

// Helper method to validate linear order.
static bool IsLinearOrderWellFormed(const HGraph* graph, ArenaVector<HBasicBlock*>* linear_order) {
  for (HBasicBlock* header : graph->GetBlocks()) {
    if (header == nullptr || !header->IsLoopHeader()) {
      continue;
    }
    HLoopInformation* loop = header->GetLoopInformation();
    size_t num_blocks = loop->GetBlocks().NumSetBits();
    size_t found_blocks = 0u;
    for (HLinearOrderIterator it(*linear_order); !it.Done(); it.Advance()) {
      HBasicBlock* current = it.Current();
      if (loop->Contains(*current)) {
        found_blocks++;
        if (found_blocks == 1u && current != header) {
          // First block is not the header.
          return false;
        } else if (found_blocks == num_blocks && !loop->IsBackEdge(*current)) {
          // Last block is not a back edge.
          return false;
        }
      } else if (found_blocks != 0u && found_blocks != num_blocks) {
        // Blocks are not adjacent.
        return false;
      }
    }
    DCHECK_EQ(found_blocks, num_blocks);
  }
  return true;
}

void LinearizeGraph(const HGraph* graph,
                    ArenaAllocator* allocator,
                    ArenaVector<HBasicBlock*>* linear_order) {
  DCHECK(linear_order->empty());
  // Create a reverse post ordering with the following properties:
  // - Blocks in a loop are consecutive,
  // - Back-edge is the last block before loop exits.
  //
  // (1): Record the number of forward predecessors for each block. This is to
  //      ensure the resulting order is reverse post order. We could use the
  //      current reverse post order in the graph, but it would require making
  //      order queries to a GrowableArray, which is not the best data structure
  //      for it.
  ArenaVector<uint32_t> forward_predecessors(graph->GetBlocks().size(),
                                             allocator->Adapter(kArenaAllocLinearOrder));
  for (HReversePostOrderIterator it(*graph); !it.Done(); it.Advance()) {
    HBasicBlock* block = it.Current();
    size_t number_of_forward_predecessors = block->GetPredecessors().size();
    if (block->IsLoopHeader()) {
      number_of_forward_predecessors -= block->GetLoopInformation()->NumberOfBackEdges();
    }
    forward_predecessors[block->GetBlockId()] = number_of_forward_predecessors;
  }
  // (2): Following a worklist approach, first start with the entry block, and
  //      iterate over the successors. When all non-back edge predecessors of a
  //      successor block are visited, the successor block is added in the worklist
  //      following an order that satisfies the requirements to build our linear graph.
  linear_order->reserve(graph->GetReversePostOrder().size());
  ArenaVector<HBasicBlock*> worklist(allocator->Adapter(kArenaAllocLinearOrder));
  worklist.push_back(graph->GetEntryBlock());
  do {
    HBasicBlock* current = worklist.back();
    worklist.pop_back();
    linear_order->push_back(current);
    for (HBasicBlock* successor : current->GetSuccessors()) {
      int block_id = successor->GetBlockId();
      size_t number_of_remaining_predecessors = forward_predecessors[block_id];
      if (number_of_remaining_predecessors == 1) {
        AddToListForLinearization(&worklist, successor);
      }
      forward_predecessors[block_id] = number_of_remaining_predecessors - 1;
    }
  } while (!worklist.empty());

  DCHECK(graph->HasIrreducibleLoops() || IsLinearOrderWellFormed(graph, linear_order));
}

}  // namespace art
