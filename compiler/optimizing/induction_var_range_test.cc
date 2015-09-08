/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include <limits.h>

#include "base/arena_allocator.h"
#include "builder.h"
#include "gtest/gtest.h"
#include "induction_var_analysis.h"
#include "induction_var_range.h"
#include "nodes.h"
#include "optimizing_unit_test.h"

namespace art {

using Value = InductionVarRange::Value;

/**
 * Fixture class for the InductionVarRange tests.
 */
class InductionVarRangeTest : public testing::Test {
 public:
  InductionVarRangeTest()  : pool_(), allocator_(&pool_) {
    graph_ = CreateGraph(&allocator_);
    iva_ = new (&allocator_) HInductionVarAnalysis(graph_);
    BuildGraph();
  }

  ~InductionVarRangeTest() { }

  void ExpectEqual(Value v1, Value v2) {
    EXPECT_EQ(v1.instruction, v2.instruction);
    EXPECT_EQ(v1.constant, v2.constant);
  }

  /** Constructs bare minimum graph. */
  void BuildGraph() {
    graph_->SetNumberOfVRegs(1);
    HBasicBlock* entry_block = new (&allocator_) HBasicBlock(graph_);
    HBasicBlock* exit_block = new (&allocator_) HBasicBlock(graph_);
    graph_->AddBlock(entry_block);
    graph_->AddBlock(exit_block);
    graph_->SetEntryBlock(entry_block);
    graph_->SetExitBlock(exit_block);
  }

  /** Constructs an invariant. */
  HInductionVarAnalysis::InductionInfo* CreateInvariant(char opc,
                                                      HInductionVarAnalysis::InductionInfo* a,
                                                      HInductionVarAnalysis::InductionInfo* b) {
    HInductionVarAnalysis::InductionOp op;
    switch (opc) {
      case '+': op = HInductionVarAnalysis::kAdd; break;
      case '-': op = HInductionVarAnalysis::kSub; break;
      case 'n': op = HInductionVarAnalysis::kNeg; break;
      case '*': op = HInductionVarAnalysis::kMul; break;
      case '/': op = HInductionVarAnalysis::kDiv; break;
      default:  op = HInductionVarAnalysis::kNop; break;
    }
    return iva_->CreateInvariantOp(op, a, b);
  }

  /** Constructs a fetch. */
  HInductionVarAnalysis::InductionInfo* CreateFetch(HInstruction* fetch) {
    return iva_->CreateInvariantFetch(fetch);
  }

  /** Constructs a constant. */
  HInductionVarAnalysis::InductionInfo* CreateConst(int32_t c) {
    return CreateFetch(graph_->GetIntConstant(c));
  }

  /** Constructs a range [lo, hi] using a periodic induction. */
  HInductionVarAnalysis::InductionInfo* CreateRange(int32_t lo, int32_t hi) {
    return iva_->CreateInduction(HInductionVarAnalysis::kPeriodic, CreateConst(lo), CreateConst(hi));
  }

  /** Constructs a wrap-around induction consisting of a constant, followed by a range. */
  HInductionVarAnalysis::InductionInfo* CreateWrapAround(int32_t initial, int32_t lo, int32_t hi) {
    return iva_->CreateInduction(HInductionVarAnalysis::kWrapAround, CreateConst(initial), CreateRange(lo, hi));
  }

  //
  // Relay methods.
  //

  Value GetMin(HInductionVarAnalysis::InductionInfo* info,
               HInductionVarAnalysis::InductionInfo* induc) {
    return InductionVarRange::GetMin(info, induc);
  }

  Value GetMax(HInductionVarAnalysis::InductionInfo* info,
               HInductionVarAnalysis::InductionInfo* induc) {
    return InductionVarRange::GetMax(info, induc);
  }

  Value GetMul(HInductionVarAnalysis::InductionInfo* info1,
               HInductionVarAnalysis::InductionInfo* info2, int32_t fail_value) {
    return InductionVarRange::GetMul(info1, info2, nullptr, fail_value);
  }

  Value GetDiv(HInductionVarAnalysis::InductionInfo* info1,
               HInductionVarAnalysis::InductionInfo* info2, int32_t fail_value) {
    return InductionVarRange::GetDiv(info1, info2, nullptr, fail_value);
  }

  Value AddValue(Value v1, Value v2) { return InductionVarRange::AddValue(v1, v2, INT_MIN); }
  Value SubValue(Value v1, Value v2) { return InductionVarRange::SubValue(v1, v2, INT_MIN); }
  Value MulValue(Value v1, Value v2) { return InductionVarRange::MulValue(v1, v2, INT_MIN); }
  Value DivValue(Value v1, Value v2) { return InductionVarRange::DivValue(v1, v2, INT_MIN); }
  Value MinValue(Value v1, Value v2) { return InductionVarRange::MinValue(v1, v2); }
  Value MaxValue(Value v1, Value v2) { return InductionVarRange::MaxValue(v1, v2); }

  // General building fields.
  ArenaPool pool_;
  ArenaAllocator allocator_;
  HGraph* graph_;
  HInductionVarAnalysis* iva_;

  // Two dummy instructions.
  HReturnVoid x_;
  HReturnVoid y_;
};

//
// The actual InductionVarRange tests.
//

TEST_F(InductionVarRangeTest, GetMinMaxNull) {
  ExpectEqual(Value(INT_MIN), GetMin(nullptr, nullptr));
  ExpectEqual(Value(INT_MAX), GetMax(nullptr, nullptr));
}

TEST_F(InductionVarRangeTest, GetMinMaxAdd) {
  ExpectEqual(Value(12), GetMin(CreateInvariant('+', CreateConst(2), CreateRange(10, 20)), nullptr));
  ExpectEqual(Value(22), GetMax(CreateInvariant('+', CreateConst(2), CreateRange(10, 20)), nullptr));
  ExpectEqual(Value(&x_, -20), GetMin(CreateInvariant('+', CreateFetch(&x_), CreateRange(-20, -10)), nullptr));
  ExpectEqual(Value(&x_, -10), GetMax(CreateInvariant('+', CreateFetch(&x_), CreateRange(-20, -10)), nullptr));
  ExpectEqual(Value(&x_, 10), GetMin(CreateInvariant('+', CreateRange(10, 20), CreateFetch(&x_)), nullptr));
  ExpectEqual(Value(&x_, 20), GetMax(CreateInvariant('+', CreateRange(10, 20), CreateFetch(&x_)), nullptr));
  ExpectEqual(Value(5), GetMin(CreateInvariant('+', CreateRange(-5, -1), CreateRange(10, 20)), nullptr));
  ExpectEqual(Value(19), GetMax(CreateInvariant('+', CreateRange(-5, -1), CreateRange(10, 20)), nullptr));
}

TEST_F(InductionVarRangeTest, GetMinMaxSub) {
  ExpectEqual(Value(-18), GetMin(CreateInvariant('-', CreateConst(2), CreateRange(10, 20)), nullptr));
  ExpectEqual(Value(-8), GetMax(CreateInvariant('-', CreateConst(2), CreateRange(10, 20)), nullptr));
  ExpectEqual(Value(&x_, 10), GetMin(CreateInvariant('-', CreateFetch(&x_), CreateRange(-20, -10)), nullptr));
  ExpectEqual(Value(&x_, 20), GetMax(CreateInvariant('-', CreateFetch(&x_), CreateRange(-20, -10)), nullptr));
  ExpectEqual(Value(INT_MIN), GetMin(CreateInvariant('-', CreateRange(10, 20), CreateFetch(&x_)), nullptr));
  ExpectEqual(Value(INT_MAX), GetMax(CreateInvariant('-', CreateRange(10, 20), CreateFetch(&x_)), nullptr));
  ExpectEqual(Value(-25), GetMin(CreateInvariant('-', CreateRange(-5, -1), CreateRange(10, 20)), nullptr));
  ExpectEqual(Value(-11), GetMax(CreateInvariant('-', CreateRange(-5, -1), CreateRange(10, 20)), nullptr));
}

TEST_F(InductionVarRangeTest, GetMinMaxNeg) {
  ExpectEqual(Value(-20), GetMin(CreateInvariant('n', nullptr, CreateRange(10, 20)), nullptr));
  ExpectEqual(Value(-10), GetMax(CreateInvariant('n', nullptr, CreateRange(10, 20)), nullptr));
  ExpectEqual(Value(10), GetMin(CreateInvariant('n', nullptr, CreateRange(-20, -10)), nullptr));
  ExpectEqual(Value(20), GetMax(CreateInvariant('n', nullptr, CreateRange(-20, -10)), nullptr));
  ExpectEqual(Value(INT_MIN), GetMin(CreateInvariant('n', nullptr, CreateFetch(&x_)), nullptr));
  ExpectEqual(Value(INT_MAX), GetMax(CreateInvariant('n', nullptr, CreateFetch(&x_)), nullptr));
}

TEST_F(InductionVarRangeTest, GetMinMaxMul) {
  ExpectEqual(Value(20), GetMin(CreateInvariant('*', CreateConst(2), CreateRange(10, 20)), nullptr));
  ExpectEqual(Value(40), GetMax(CreateInvariant('*', CreateConst(2), CreateRange(10, 20)), nullptr));
}

TEST_F(InductionVarRangeTest, GetMinMaxDiv) {
  ExpectEqual(Value(3), GetMin(CreateInvariant('/', CreateRange(12, 20), CreateConst(4)), nullptr));
  ExpectEqual(Value(5), GetMax(CreateInvariant('/', CreateRange(12, 20), CreateConst(4)), nullptr));
}

TEST_F(InductionVarRangeTest, GetMinMaxConstant) {
  ExpectEqual(Value(12345), GetMin(CreateConst(12345), nullptr));
  ExpectEqual(Value(12345), GetMax(CreateConst(12345), nullptr));
}

TEST_F(InductionVarRangeTest, GetMinMaxFetch) {
  ExpectEqual(Value(&x_, 0), GetMin(CreateFetch(&x_), nullptr));
  ExpectEqual(Value(&x_, 0), GetMax(CreateFetch(&x_), nullptr));
}

TEST_F(InductionVarRangeTest, GetMinMaxLinear) {
}

TEST_F(InductionVarRangeTest, GetMinMaxWrapAround) {
  ExpectEqual(Value(-5), GetMin(CreateWrapAround(-5, -1, 10), nullptr));
  ExpectEqual(Value(10), GetMax(CreateWrapAround(-5, -1, 10), nullptr));
  ExpectEqual(Value(-1), GetMin(CreateWrapAround(2, -1, 10), nullptr));
  ExpectEqual(Value(10), GetMax(CreateWrapAround(2, -1, 10), nullptr));
  ExpectEqual(Value(-1), GetMin(CreateWrapAround(20, -1, 10), nullptr));
  ExpectEqual(Value(20), GetMax(CreateWrapAround(20, -1, 10), nullptr));
}

TEST_F(InductionVarRangeTest, GetMinMaxPeriodic) {
  ExpectEqual(Value(-2), GetMin(CreateRange(-2, 99), nullptr));
  ExpectEqual(Value(99), GetMax(CreateRange(-2, 99), nullptr));
}

TEST_F(InductionVarRangeTest, GetMulMin) {
  ExpectEqual(Value(6), GetMul(CreateRange(2, 10), CreateRange(3, 5), INT_MIN));
  ExpectEqual(Value(-50), GetMul(CreateRange(2, 10), CreateRange(-5, -3), INT_MIN));
  ExpectEqual(Value(-50), GetMul(CreateRange(-10, -2), CreateRange(3, 5), INT_MIN));
  ExpectEqual(Value(6), GetMul(CreateRange(-10, -2), CreateRange(-5, -3), INT_MIN));
}

TEST_F(InductionVarRangeTest, GetMulMax) {
  ExpectEqual(Value(50), GetMul(CreateRange(2, 10), CreateRange(3, 5), INT_MAX));
  ExpectEqual(Value(-6), GetMul(CreateRange(2, 10), CreateRange(-5, -3), INT_MAX));
  ExpectEqual(Value(-6), GetMul(CreateRange(-10, -2), CreateRange(3, 5), INT_MAX));
  ExpectEqual(Value(50), GetMul(CreateRange(-10, -2), CreateRange(-5, -3), INT_MAX));
}

TEST_F(InductionVarRangeTest, GetDivMin) {
  ExpectEqual(Value(10), GetDiv(CreateRange(40, 1000), CreateRange(2, 4), INT_MIN));
  ExpectEqual(Value(-500), GetDiv(CreateRange(40, 1000), CreateRange(-4, -2), INT_MIN));
  ExpectEqual(Value(-500), GetDiv(CreateRange(-1000, -40), CreateRange(2, 4), INT_MIN));
  ExpectEqual(Value(10), GetDiv(CreateRange(-1000, -40), CreateRange(-4, -2), INT_MIN));
}

TEST_F(InductionVarRangeTest, GetDivMax) {
  ExpectEqual(Value(500), GetDiv(CreateRange(40, 1000), CreateRange(2, 4), INT_MAX));
  ExpectEqual(Value(-10), GetDiv(CreateRange(40, 1000), CreateRange(-4, -2), INT_MAX));
  ExpectEqual(Value(-10), GetDiv(CreateRange(-1000, -40), CreateRange(2, 4), INT_MAX));
  ExpectEqual(Value(500), GetDiv(CreateRange(-1000, -40), CreateRange(-4, -2), INT_MAX));
}

TEST_F(InductionVarRangeTest, AddValue) {
  ExpectEqual(Value(110), AddValue(Value(10), Value(100)));
  ExpectEqual(Value(INT_MIN), AddValue(Value(&x_, -4), Value(&x_, -1)));
  ExpectEqual(Value(INT_MIN), AddValue(Value(&x_, 5), Value(&y_, -7)));
  ExpectEqual(Value(&x_, 23), AddValue(Value(&x_, 20), Value(3)));
  ExpectEqual(Value(&y_, 5), AddValue(Value(55), Value(&y_, -50)));
  // Unsafe.
  ExpectEqual(Value(INT_MIN), AddValue(Value(INT_MAX - 5), Value(6)));
}

TEST_F(InductionVarRangeTest, SubValue) {
  ExpectEqual(Value(-90), SubValue(Value(10), Value(100)));
  ExpectEqual(Value(-3), SubValue(Value(&x_, -4), Value(&x_, -1)));
  ExpectEqual(Value(INT_MIN), SubValue(Value(&x_, 5), Value(&y_, -7)));
  ExpectEqual(Value(&x_, 17), SubValue(Value(&x_, 20), Value(3)));
  ExpectEqual(Value(INT_MIN), SubValue(Value(55), Value(&y_, -50)));
  // Unsafe.
  ExpectEqual(Value(INT_MIN), SubValue(Value(INT_MIN + 5), Value(6)));
}

TEST_F(InductionVarRangeTest, MulValue) {
  ExpectEqual(Value(1000), MulValue(Value(10), Value(100)));
  ExpectEqual(Value(INT_MIN), MulValue(Value(&x_, -4), Value(&x_, -1)));
  ExpectEqual(Value(INT_MIN), MulValue(Value(&x_, 5), Value(&y_, -7)));
  ExpectEqual(Value(nullptr, INT_MIN), MulValue(Value(&x_, 20), Value(nullptr, 3)));
  ExpectEqual(Value(nullptr, INT_MIN), MulValue(Value(nullptr, 55), Value(&y_, -50)));
  // Unsafe.
  ExpectEqual(Value(nullptr, INT_MIN), MulValue(Value(nullptr, 90000), Value(nullptr, -90000)));
}

TEST_F(InductionVarRangeTest, DivValue) {
  ExpectEqual(Value(nullptr, 25), DivValue(Value(nullptr, 100), Value(nullptr, 4)));
  ExpectEqual(Value(nullptr, INT_MIN), DivValue(Value(&x_, -4), Value(&x_, -1)));
  ExpectEqual(Value(nullptr, INT_MIN), DivValue(Value(&x_, 5), Value(&y_, -7)));
  ExpectEqual(Value(nullptr, INT_MIN), DivValue(Value(&x_, 20), Value(nullptr, 3)));
  ExpectEqual(Value(nullptr, INT_MIN), DivValue(Value(nullptr, 55), Value(&y_, -50)));
  // Unsafe.
  ExpectEqual(Value(nullptr, INT_MIN), DivValue(Value(nullptr, 1), Value(nullptr, 0)));
}

TEST_F(InductionVarRangeTest, MinValue) {
  ExpectEqual(Value(nullptr, 10), MinValue(Value(nullptr, 10), Value(nullptr, 100)));
  ExpectEqual(Value(&x_, -4), MinValue(Value(&x_, -4), Value(&x_, -1)));
  ExpectEqual(Value(nullptr, INT_MIN), MinValue(Value(&x_, 5), Value(&y_, -7)));
  ExpectEqual(Value(nullptr, INT_MIN), MinValue(Value(&x_, 20), Value(nullptr, 3)));
  ExpectEqual(Value(nullptr, INT_MIN), MinValue(Value(nullptr, 55), Value(&y_, -50)));
}

TEST_F(InductionVarRangeTest, MaxValue) {
  ExpectEqual(Value(nullptr, 100), MaxValue(Value(nullptr, 10), Value(nullptr, 100)));
  ExpectEqual(Value(&x_, -1), MaxValue(Value(&x_, -4), Value(&x_, -1)));
  ExpectEqual(Value(nullptr, INT_MAX), MaxValue(Value(&x_, 5), Value(&y_, -7)));
  ExpectEqual(Value(nullptr, INT_MAX), MaxValue(Value(&x_, 20), Value(nullptr, 3)));
  ExpectEqual(Value(nullptr, INT_MAX), MaxValue(Value(nullptr, 55), Value(&y_, -50)));
}

}  // namespace art
