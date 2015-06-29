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

#include "dex_instruction-inl.h"
#include "gtest/gtest.h"

namespace art {

TEST(StaticGetters, PropertiesOfNopTest) {
  Instruction::Code nop = Instruction::NOP;
  EXPECT_STREQ("nop", Instruction::Name(nop));
  EXPECT_EQ(Instruction::k10x, Instruction::FormatOf(nop));
  EXPECT_EQ(Instruction::kNone, Instruction::IndexTypeOf(nop));
  EXPECT_EQ(Instruction::kContinue, Instruction::FlagsOf(nop));
  EXPECT_EQ(Instruction::kVerifyNone, Instruction::VerifyFlagsOf(nop));
}

}  // namespace art
