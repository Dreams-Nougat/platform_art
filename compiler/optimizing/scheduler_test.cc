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

#include "base/arena_allocator.h"
#include "builder.h"
#include "codegen_test_utils.h"
#include "common_compiler_test.h"
#include "nodes.h"
#include "optimizing_unit_test.h"
#include "pc_relative_fixups_x86.h"
#include "register_allocator.h"
#include "scheduler.h"
#include "scheduler_arm64.h"

namespace art {

class SchedulerTest : public CommonCompilerTest {};

TEST_F(SchedulerTest, DependencyGraph) {
  ArenaPool pool;
  ArenaAllocator allocator(&pool);
  HGraph* graph = CreateGraph(&allocator);
  HBasicBlock* entry = new (&allocator) HBasicBlock(graph);
  HBasicBlock* block1 = new (&allocator) HBasicBlock(graph);
  graph->AddBlock(entry);
  graph->AddBlock(block1);
  graph->SetEntryBlock(entry);

  // entry:
  // array         ParameterValue
  // c1            IntConstant
  // c2            IntConstant
  // block1:
  // add1          Add [c1, c2]
  // add2          Add [add1, c2]
  // mul           Mul [add1, add2]
  // div_check     DivZeroCheck [add2] (env: add2, mul)
  // div           Div [add1, div_check]
  // array_get     ArrayGet [array, add1]
  // array_set     ArraySet [array, add1, add2]

  HInstruction* array = new (&allocator) HParameterValue(graph->GetDexFile(),
                                                         0,
                                                         0,
                                                         Primitive::kPrimNot);
  HInstruction* c1 = graph->GetIntConstant(1);
  HInstruction* c2 = graph->GetIntConstant(10);
  HInstruction* add1 = new (&allocator) HAdd(Primitive::kPrimInt, c1, c2);
  HInstruction* add2 = new (&allocator) HAdd(Primitive::kPrimInt, add1, c2);
  HInstruction* mul = new (&allocator) HMul(Primitive::kPrimInt, add1, add2);
  HInstruction* div_check = new (&allocator) HDivZeroCheck(add2, 0);
  HInstruction* div = new (&allocator) HDiv(Primitive::kPrimInt, add1, div_check, 0);
  HInstruction* array_get = new (&allocator) HArrayGet(array, add1, Primitive::kPrimInt, 0);
  HInstruction* array_set = new (&allocator) HArraySet(array, add1, add2, Primitive::kPrimInt, 0);

  DCHECK(div_check->CanThrow());

  entry->AddInstruction(array);

  std::vector<HInstruction*> block_instructions = {add1,
                                                   add2,
                                                   mul,
                                                   div_check,
                                                   div,
                                                   array_get,
                                                   array_set};
  for (auto instr : block_instructions) {
    block1->AddInstruction(instr);
  }

  HEnvironment* environment = new (&allocator) HEnvironment(&allocator,
                                                            2,
                                                            graph->GetDexFile(),
                                                            graph->GetMethodIdx(),
                                                            0,
                                                            kStatic,
                                                            div_check);
  div_check->SetRawEnvironment(environment);
  environment->SetRawEnvAt(0, add2);
  add2->AddEnvUseAt(div_check->GetEnvironment(), 0);
  environment->SetRawEnvAt(1, mul);
  mul->AddEnvUseAt(div_check->GetEnvironment(), 1);

  ArenaAllocator* arena = graph->GetArena();
  CriticalPathSchedulingNodeSelector critical_path_selector;
  arm64::HArm64Scheduler scheduler(arena, &critical_path_selector);
  SchedulingGraph scheduling_graph(&scheduler, arena);
  // Instructions must be inserted in reverse order into the graph.
  for (std::vector<HInstruction*>::reverse_iterator it(block_instructions.rbegin());
       it != block_instructions.rend();
       it++) {
    scheduling_graph.AddNode(*it);
  }

  ASSERT_FALSE(scheduling_graph.HasImmediateDataDependency(add1, c1));
  ASSERT_FALSE(scheduling_graph.HasImmediateDataDependency(add2, c2));

  // Define-use dependency.
  ASSERT_TRUE(scheduling_graph.HasImmediateDataDependency(add2, add1));
  ASSERT_FALSE(scheduling_graph.HasImmediateDataDependency(add1, add2));
  ASSERT_TRUE(scheduling_graph.HasImmediateDataDependency(div_check, add2));
  ASSERT_FALSE(scheduling_graph.HasImmediateDataDependency(div_check, add1));
  ASSERT_TRUE(scheduling_graph.HasImmediateDataDependency(div, div_check));
  ASSERT_TRUE(scheduling_graph.HasImmediateDataDependency(array_set, add1));
  ASSERT_TRUE(scheduling_graph.HasImmediateDataDependency(array_set, add2));

  // Read-write dependency.
  ASSERT_TRUE(scheduling_graph.HasImmediateOtherDependency(array_set, array_get));

  // Env dependency.
  ASSERT_TRUE(scheduling_graph.HasImmediateOtherDependency(div_check, mul));
  ASSERT_FALSE(scheduling_graph.HasImmediateOtherDependency(mul, div_check));

  // CanThrow.
  ASSERT_TRUE(scheduling_graph.HasImmediateOtherDependency(array_set, div_check));
}

static void CompileWithRandomSchedulerAndRun(const uint16_t* data,
                                             bool has_result,
                                             int expected) {
  for (CodegenTargetConfig target_config : GetTargetConfigs()) {
    ArenaPool pool;
    ArenaAllocator arena(&pool);
    HGraph* graph = CreateCFG(&arena, data);

    // Schedule the graph randomly.
    HInstructionScheduling scheduling(graph, target_config.GetInstructionSet());
    scheduling.Run(/*only_optimize_loop_blocks*/ false, /*schedule_randomly*/ true);

    RunCode(target_config,
            graph,
            [](HGraph* graph_arg) { RemoveSuspendChecks(graph_arg); },
            has_result, expected);
  }
}

TEST_F(SchedulerTest, RandomScheduling) {
  //
  // Java source: crafted code to make sure (random) scheduling should get correct result.
  //
  //  int result = 0;
  //  float fr = 10.0f;
  //  for (int i = 1; i < 10; i++) {
  //    fr ++;
  //    int t1 = result >> i;
  //    int t2 = result * i;
  //    result = result + t1 - t2;
  //    fr = fr / i;
  //    result += (int)fr;
  //  }
  //  return result;
  //
  const uint16_t data[] = SIX_REGISTERS_CODE_ITEM(
    Instruction::CONST_4 | 0 << 12 | 2 << 8,          // const/4 v2, #int 0
    Instruction::CONST_HIGH16 | 0 << 8, 0x4120,       // const/high16 v0, #float 10.0 // #41200000
    Instruction::CONST_4 | 1 << 12 | 1 << 8,          // const/4 v1, #int 1
    Instruction::CONST_16 | 5 << 8, 0x000a,           // const/16 v5, #int 10
    Instruction::IF_GE | 5 << 12 | 1 << 8, 0x0014,    // if-ge v1, v5, 001a // +0014
    Instruction::CONST_HIGH16 | 5 << 8, 0x3f80,       // const/high16 v5, #float 1.0 // #3f800000
    Instruction::ADD_FLOAT_2ADDR | 5 << 12 | 0 << 8,  // add-float/2addr v0, v5
    Instruction::SHR_INT | 3 << 8, 1 << 8 | 2 ,       // shr-int v3, v2, v1
    Instruction::MUL_INT | 4 << 8, 1 << 8 | 2,        // mul-int v4, v2, v1
    Instruction::ADD_INT | 5 << 8, 3 << 8 | 2,        // add-int v5, v2, v3
    Instruction::SUB_INT | 2 << 8, 4 << 8 | 5,        // sub-int v2, v5, v4
    Instruction::INT_TO_FLOAT | 1 << 12 | 5 << 8,     // int-to-float v5, v1
    Instruction::DIV_FLOAT_2ADDR | 5 << 12 | 0 << 8,  // div-float/2addr v0, v5
    Instruction::FLOAT_TO_INT | 0 << 12 | 5 << 8,     // float-to-int v5, v0
    Instruction::ADD_INT_2ADDR | 5 << 12 | 2 << 8,    // add-int/2addr v2, v5
    Instruction::ADD_INT_LIT8 | 1 << 8, 1 << 8 | 1,   // add-int/lit8 v1, v1, #int 1 // #01
    Instruction::GOTO | 0xeb << 8,                    // goto 0004 // -0015
    Instruction::RETURN | 2 << 8);                    // return v2

  constexpr int kNumberOfRuns = 10;
  for (int i = 0; i < kNumberOfRuns; ++i) {
    CompileWithRandomSchedulerAndRun(data, true, 138774);
  }
}

}  // namespace art
