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

#ifndef ART_COMPILER_OPTIMIZING_GRAPH_CHECKER_H_
#define ART_COMPILER_OPTIMIZING_GRAPH_CHECKER_H_

#include "nodes.h"

namespace art {

// A control-flow graph visitor performing various checks.
class GraphChecker : public HGraphVisitor {
 public:
  GraphChecker(ArenaAllocator* arena, HGraph* graph)
    : HGraphVisitor(graph),
      arena_(arena),
      errors_(arena, 0) {}

  // Check `block`.
  virtual void VisitBasicBlock(HBasicBlock* block);

  // Check `instruction`.
  virtual void VisitInstruction(HInstruction* instruction);

  // Was the last visit of the graph valid?
  bool IsValid() const;

  // Get the list of detected errors.
  const GrowableArray<std::string>& GetErrors() const;

 protected:
  ArenaAllocator* const arena_;
  // The block currently visited.
  HBasicBlock* current_block_ = nullptr;
  // Errors encountered while checking the graph.
  GrowableArray<std::string> errors_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GraphChecker);
};


// An SSA graph visitor performing various checks.
class SSAChecker : public GraphChecker {
 public:
  typedef GraphChecker super_type;

  SSAChecker(ArenaAllocator* arena, HGraph* graph)
    : GraphChecker(arena, graph) {}

  // Perform SSA form checks on `block`.
  virtual void VisitBasicBlock(HBasicBlock* block);
  // Perform SSA form checks on `instruction`.
  virtual void VisitInstruction(HInstruction* instruction);

 private:
  DISALLOW_COPY_AND_ASSIGN(SSAChecker);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_GRAPH_CHECKER_H_
