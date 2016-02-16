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

#include "base/arena_allocator.h"
#include "base/stringprintf.h"
#include "builder.h"
#include "dex_file.h"
#include "dex_instruction.h"
#include "nodes.h"
#include "optimizing_unit_test.h"
#include "pretty_printer.h"

#include "gtest/gtest.h"

namespace art {

static void TestCode(const uint16_t* data, const char* expected) {
  ArenaPool pool;
  ArenaAllocator allocator(&pool);
  HGraph* graph = CreateCFG(&allocator, data);
  StringPrettyPrinter printer(graph);
  printer.VisitInsertionOrder();
  ASSERT_STREQ(expected, printer.str().c_str());
}

class PrettyPrinterTest : public CommonCompilerTest {};

TEST_F(PrettyPrinterTest, ReturnVoid) {
  const uint16_t data[] = ZERO_REGISTER_CODE_ITEM(
      Instruction::RETURN_VOID);

  const char* expected =
      "BasicBlock 0, succ: 1\n"
      "  0: SuspendCheck\n"
      "  1: Goto 1\n"
      "BasicBlock 1, pred: 0, succ: 2\n"
      "  2: ReturnVoid\n"
      "BasicBlock 2, pred: 1\n"
      "  3: Exit\n"
      "";

  TestCode(data, expected);
}

TEST_F(PrettyPrinterTest, CFG1) {
  const char* expected =
      "BasicBlock 0, succ: 1\n"
      "  0: SuspendCheck\n"
      "  1: Goto 1\n"
      "BasicBlock 1, pred: 0, succ: 2\n"
      "  2: Goto 2\n"
      "BasicBlock 2, pred: 1, succ: 3\n"
      "  3: ReturnVoid\n"
      "BasicBlock 3, pred: 2\n"
      "  4: Exit\n"
      "";

  const uint16_t data[] =
    ZERO_REGISTER_CODE_ITEM(
      Instruction::GOTO | 0x100,
      Instruction::RETURN_VOID);

  TestCode(data, expected);
}

TEST_F(PrettyPrinterTest, CFG2) {
  const char* expected =
      "BasicBlock 0, succ: 1\n"
      "  0: SuspendCheck\n"
      "  1: Goto 1\n"
      "BasicBlock 1, pred: 0, succ: 2\n"
      "  2: Goto 2\n"
      "BasicBlock 2, pred: 1, succ: 3\n"
      "  3: Goto 3\n"
      "BasicBlock 3, pred: 2, succ: 4\n"
      "  4: ReturnVoid\n"
      "BasicBlock 4, pred: 3\n"
      "  5: Exit\n"
      "";

  const uint16_t data[] = ZERO_REGISTER_CODE_ITEM(
    Instruction::GOTO | 0x100,
    Instruction::GOTO | 0x100,
    Instruction::RETURN_VOID);

  TestCode(data, expected);
}

TEST_F(PrettyPrinterTest, CFG3) {
  const char* expected =
      "BasicBlock 0, succ: 1\n"
      "  0: SuspendCheck\n"
      "  1: Goto 1\n"
      "BasicBlock 1, pred: 0, succ: 3\n"
      "  2: Goto 3\n"
      "BasicBlock 2, pred: 3, succ: 4\n"
      "  3: ReturnVoid\n"
      "BasicBlock 3, pred: 1, succ: 2\n"
      "  4: Goto 2\n"
      "BasicBlock 4, pred: 2\n"
      "  5: Exit\n";

  const uint16_t data1[] = ZERO_REGISTER_CODE_ITEM(
    Instruction::GOTO | 0x200,
    Instruction::RETURN_VOID,
    Instruction::GOTO | 0xFF00);

  TestCode(data1, expected);

  const uint16_t data2[] = ZERO_REGISTER_CODE_ITEM(
    Instruction::GOTO_16, 3,
    Instruction::RETURN_VOID,
    Instruction::GOTO_16, 0xFFFF);

  TestCode(data2, expected);

  const uint16_t data3[] = ZERO_REGISTER_CODE_ITEM(
    Instruction::GOTO_32, 4, 0,
    Instruction::RETURN_VOID,
    Instruction::GOTO_32, 0xFFFF, 0xFFFF);

  TestCode(data3, expected);
}

TEST_F(PrettyPrinterTest, CFG4) {
  const char* expected =
      "BasicBlock 0, succ: 3\n"
      "  2: SuspendCheck\n"
      "  3: Goto 3\n"
      "BasicBlock 1, pred: 3, 1, succ: 1\n"
      "  1: SuspendCheck\n"
      "  4: Goto 1\n"
      "BasicBlock 3, pred: 0, succ: 1\n"
      "  0: Goto 1\n"
      "";

  const uint16_t data1[] = ZERO_REGISTER_CODE_ITEM(
    Instruction::NOP,
    Instruction::GOTO | 0xFF00);

  TestCode(data1, expected);

  const uint16_t data2[] = ZERO_REGISTER_CODE_ITEM(
    Instruction::GOTO_32, 0, 0);

  TestCode(data2, expected);
}

TEST_F(PrettyPrinterTest, CFG5) {
  const char* expected =
      "BasicBlock 0, succ: 1\n"
      "  0: SuspendCheck\n"
      "  1: Goto 1\n"
      "BasicBlock 1, pred: 0, succ: 3\n"
      "  2: ReturnVoid\n"
      "BasicBlock 3, pred: 1\n"
      "  3: Exit\n"
      "";

  const uint16_t data[] = ZERO_REGISTER_CODE_ITEM(
    Instruction::RETURN_VOID,
    Instruction::GOTO | 0x100,
    Instruction::GOTO | 0xFE00);

  TestCode(data, expected);
}

TEST_F(PrettyPrinterTest, CFG6) {
  const char* expected =
      "BasicBlock 0, succ: 1\n"
      "  4: IntConstant [8, 8]\n"
      "  2: SuspendCheck\n"
      "  3: Goto 1\n"
      "BasicBlock 1, pred: 0, succ: 5, 2\n"
      "  8: Equal(4, 4) [9]\n"
      "  9: If(8)\n"
      "BasicBlock 2, pred: 1, succ: 3\n"
      "  10: Goto 3\n"
      "BasicBlock 3, pred: 5, 2, succ: 4\n"
      "  11: ReturnVoid\n"
      "BasicBlock 4, pred: 3\n"
      "  12: Exit\n"
      "BasicBlock 5, pred: 1, succ: 3\n"
      "  0: Goto 3\n"
      "";

  const uint16_t data[] = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::IF_EQ, 3,
    Instruction::GOTO | 0x100,
    Instruction::RETURN_VOID);

  TestCode(data, expected);
}

TEST_F(PrettyPrinterTest, CFG7) {
  const char* expected =
      "BasicBlock 0, succ: 1\n"
      "  6: IntConstant [10, 10]\n"
      "  4: SuspendCheck\n"
      "  5: Goto 1\n"
      "BasicBlock 1, pred: 0, succ: 5, 6\n"
      "  10: Equal(6, 6) [11]\n"
      "  11: If(10)\n"
      "BasicBlock 2, pred: 6, 3, succ: 3\n"
      "  12: Goto 3\n"
      "BasicBlock 3, pred: 5, 2, succ: 2\n"
      "  2: SuspendCheck\n"
      "  13: Goto 2\n"
      "BasicBlock 5, pred: 1, succ: 3\n"
      "  0: Goto 3\n"
      "BasicBlock 6, pred: 1, succ: 2\n"
      "  1: Goto 2\n";

  const uint16_t data[] = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::IF_EQ, 3,
    Instruction::GOTO | 0x100,
    Instruction::GOTO | 0xFF00);

  TestCode(data, expected);
}

TEST_F(PrettyPrinterTest, IntConstant) {
  const char* expected =
      "BasicBlock 0, succ: 1\n"
      "  3: IntConstant\n"
      "  1: SuspendCheck\n"
      "  2: Goto 1\n"
      "BasicBlock 1, pred: 0, succ: 2\n"
      "  5: ReturnVoid\n"
      "BasicBlock 2, pred: 1\n"
      "  6: Exit\n";

  const uint16_t data[] = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::RETURN_VOID);

  TestCode(data, expected);
}
}  // namespace art
