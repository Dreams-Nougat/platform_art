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

#ifndef ART_COMPILER_OPTIMIZING_INDUCTION_VAR_RANGE_H_
#define ART_COMPILER_OPTIMIZING_INDUCTION_VAR_RANGE_H_

#include "induction_var_analysis.h"

namespace art {

/**
 * This class implements range analysis on expressions within loops. It takes the results
 * of induction variable analysis in the constructor and provides a public API to obtain
 * a conservative lower and upper bound value on each instruction in the HIR.
 *
 * The range analysis is done with a combination of symbolic and partial integral evaluation
 * of expressions. The analysis avoids complications with wrap-around arithmetic on the integral
 * parts but all clients should be aware that wrap-around may occur on any of the symbolic parts.
 * For example, given a known range for [0,100] for i, the evaluation yields range [-100,100]
 * for expression -2*i+100, which is exact, and range [x,x+100] for expression i+x, which may
 * wrap-around anywhere in the range depending on the actual value of x.
 */
class InductionVarRange {
 public:
  /*
   * A value that can be represented as "a * instruction + b" for 32-bit constants, where
   * Value() denotes an unknown lower and upper bound. Although range analysis could yield
   * more complex values, the format is sufficiently powerful to represent useful cases
   * and feeds directly into optimizations like bounds check elimination.
   */
  struct Value {
    Value() : instruction(nullptr), a_constant(0), b_constant(0), is_known(false) {}
    Value(HInstruction* i, int32_t a, int32_t b)
        : instruction(a != 0 ? i : nullptr), a_constant(a), b_constant(b), is_known(true) {}
    explicit Value(int32_t b) : Value(nullptr, 0, b) {}
    // Representation as: a_constant x instruction + b_constant.
    HInstruction* instruction;
    int32_t a_constant;
    int32_t b_constant;
    // If true, represented by prior fields. Otherwise unknown value.
    bool is_known;
  };

  explicit InductionVarRange(HInductionVarAnalysis* induction);

  /**
   * Given a context denoted by the first instruction, returns a possibly conservative lower
   * and upper bound on the instruction's value in the output parameters min_val and max_val,
   * respectively. The need_finite_test flag denotes if an additional finite-test is needed
   * to protect the range evaluation inside its loop. The parameter chase_hint defines an
   * instruction at which chasing may stop. Returns false on failure.
   */
  bool GetInductionRange(HInstruction* context,
                         HInstruction* instruction,
                         HInstruction* chase_hint,
                         /*out*/ Value* min_val,
                         /*out*/ Value* max_val,
                         /*out*/ bool* needs_finite_test);

  /**
   * Returns true if range analysis is able to generate code for the lower and upper
   * bound expressions on the instruction in the given context. The need_finite_test
   * and need_taken test flags denote if an additional finite-test and/or taken-test
   * are needed to protect the range evaluation inside its loop.
   */
  bool CanGenerateRange(HInstruction* context,
                        HInstruction* instruction,
                        /*out*/ bool* needs_finite_test,
                        /*out*/ bool* needs_taken_test);

  /**
   * Generates the actual code in the HIR for the lower and upper bound expressions on the
   * instruction in the given context. Code for the lower and upper bound expression are
   * generated in given block and graph and are returned in the output parameters lower and
   * upper, respectively. For a loop invariant, lower is not set.
   *
   * For example, given expression x+i with range [0, 5] for i, calling this method
   * will generate the following sequence:
   *
   * block:
   *   lower: add x, 0
   *   upper: add x, 5
   *
   * Precondition: CanGenerateRange() returns true.
   */
  void GenerateRange(HInstruction* context,
                     HInstruction* instruction,
                     HGraph* graph,
                     HBasicBlock* block,
                     /*out*/ HInstruction** lower,
                     /*out*/ HInstruction** upper);

  /**
   * Generates explicit taken-test for the loop in the given context. Code is generated in
   * given block and graph. Returns generated taken-test.
   *
   * Precondition: CanGenerateRange() returns true and needs_taken_test is set.
   */
  HInstruction* GenerateTakenTest(HInstruction* context, HGraph* graph, HBasicBlock* block);

  /**
   * Returns true if induction analysis is able to generate code for last value of
   * the given instruction inside the closest enveloping loop.
   */
  bool CanGenerateLastValue(HInstruction* instruction);

  /**
   * Generates last value of the given instruction in the closest enveloping loop.
   * Code is generated in given block and graph. Returns generated last value.
   *
   * Precondition: CanGenerateLastValue() returns true.
   */
  HInstruction* GenerateLastValue(HInstruction* instruction, HGraph* graph, HBasicBlock* block);

  /**
   * Updates all matching fetches with the given replacement in all induction information
   * that is associated with the given instruction.
   */
  void Replace(HInstruction* instruction, HInstruction* fetch, HInstruction* replacement);

  /**
   * Incrementally updates induction information for just the given loop.
   */
  void ReVisit(HLoopInformation* loop) {
    induction_analysis_->induction_.erase(loop);
    induction_analysis_->VisitLoop(loop);
  }

 private:
  /*
   * Enum used in IsConstant() request.
   */
  enum ConstantRequest {
    kExact,
    kAtMost,
    kAtLeast
  };

  /**
   * Returns true if exact or upper/lower bound on the given induction
   * information is known as a 64-bit constant, which is returned in value.
   */
  bool IsConstant(HInductionVarAnalysis::InductionInfo* info,
                  ConstantRequest request,
                  /*out*/ int64_t* value) const;

  /** Returns whether induction information can be obtained. */
  bool HasInductionInfo(HInstruction* context,
                        HInstruction* instruction,
                        /*out*/ HLoopInformation** loop,
                        /*out*/ HInductionVarAnalysis::InductionInfo** info,
                        /*out*/ HInductionVarAnalysis::InductionInfo** trip) const;

  bool HasFetchInLoop(HInductionVarAnalysis::InductionInfo* info) const;
  bool NeedsTripCount(HInductionVarAnalysis::InductionInfo* info,
                      /*out*/ int64_t* stride_value) const;
  bool IsBodyTripCount(HInductionVarAnalysis::InductionInfo* trip) const;
  bool IsUnsafeTripCount(HInductionVarAnalysis::InductionInfo* trip) const;
  bool IsWellBehavedTripCount(HInductionVarAnalysis::InductionInfo* trip) const;

  Value GetLinear(HInductionVarAnalysis::InductionInfo* info,
                  HInductionVarAnalysis::InductionInfo* trip,
                  bool in_body,
                  bool is_min) const;
  Value GetFetch(HInstruction* instruction,
                 HInductionVarAnalysis::InductionInfo* trip,
                 bool in_body,
                 bool is_min) const;
  Value GetVal(HInductionVarAnalysis::InductionInfo* info,
               HInductionVarAnalysis::InductionInfo* trip,
               bool in_body,
               bool is_min) const;
  Value GetMul(HInductionVarAnalysis::InductionInfo* info1,
               HInductionVarAnalysis::InductionInfo* info2,
               HInductionVarAnalysis::InductionInfo* trip,
               bool in_body,
               bool is_min) const;
  Value GetDiv(HInductionVarAnalysis::InductionInfo* info1,
               HInductionVarAnalysis::InductionInfo* info2,
               HInductionVarAnalysis::InductionInfo* trip,
               bool in_body,
               bool is_min) const;
  Value GetXor(HInductionVarAnalysis::InductionInfo* info1,
               HInductionVarAnalysis::InductionInfo* info2) const;

  Value MulRangeAndConstant(int64_t value,
                            HInductionVarAnalysis::InductionInfo* info,
                            HInductionVarAnalysis::InductionInfo* trip,
                            bool in_body,
                            bool is_min) const;
  Value DivRangeAndConstant(int64_t value,
                            HInductionVarAnalysis::InductionInfo* info,
                            HInductionVarAnalysis::InductionInfo* trip,
                            bool in_body,
                            bool is_min) const;

  Value AddValue(Value v1, Value v2) const;
  Value SubValue(Value v1, Value v2) const;
  Value MulValue(Value v1, Value v2) const;
  Value DivValue(Value v1, Value v2) const;
  Value MergeVal(Value v1, Value v2, bool is_min) const;

  /**
   * Generates code for lower/upper/taken-test or last value in the HIR. Returns true on
   * success. With values nullptr, the method can be used to determine if code generation
   * would be successful without generating actual code yet.
   */
  bool GenerateCode(HInstruction* context,
                    HInstruction* instruction,
                    bool is_last_val,
                    HGraph* graph,
                    HBasicBlock* block,
                    /*out*/ HInstruction** lower,
                    /*out*/ HInstruction** upper,
                    /*out*/ HInstruction** taken_test,
                    /*out*/ int64_t* stride_value,
                    /*out*/ bool* needs_finite_test,
                    /*out*/ bool* needs_taken_test) const;

  bool GenerateCode(HInductionVarAnalysis::InductionInfo* info,
                    HInductionVarAnalysis::InductionInfo* trip,
                    HGraph* graph,
                    HBasicBlock* block,
                    /*out*/ HInstruction** result,
                    bool in_body,
                    bool is_min) const;

  void ReplaceInduction(HInductionVarAnalysis::InductionInfo* info,
                        HInstruction* fetch,
                        HInstruction* replacement);

  /** Results of prior induction variable analysis. */
  HInductionVarAnalysis* induction_analysis_;

  /** Instruction at which chasing may stop. */
  HInstruction* chase_hint_;

  friend class HInductionVarAnalysis;
  friend class InductionVarRangeTest;

  DISALLOW_COPY_AND_ASSIGN(InductionVarRange);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_INDUCTION_VAR_RANGE_H_
