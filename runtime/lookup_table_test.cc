/*
 * Copyright (C) 2011 The Android Open Source Project
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


#include <memory>

#include "common_runtime_test.h"
#include "dex_file.h"
#include "dex_file-inl.h"
#include "lookup_table.h"
#include "scoped_thread_state_change.h"

namespace art {

class LookupTableTest: public CommonRuntimeTest {
};

TEST_F(LookupTableTest, CreateLookupTable) {
  ScopedObjectAccess soa(Thread::Current());
  std::unique_ptr<const DexFile> dex_file(OpenTestDexFile("Lookup"));
  std::unique_ptr<TypeLookupTable> table(TypeLookupTable::Create(*dex_file));
  ASSERT_NE(nullptr, table.get());
  ASSERT_NE(nullptr, table->RawData());
  ASSERT_EQ(32U, table->RawDataLength());
}

TEST_F(LookupTableTest, FindNonExistingClassWithoutCollisions) {
  ScopedObjectAccess soa(Thread::Current());
  std::unique_ptr<const DexFile> dex_file(OpenTestDexFile("Lookup"));
  std::unique_ptr<TypeLookupTable> table(TypeLookupTable::Create(*dex_file));
  ASSERT_NE(nullptr, table.get());
  const char* descriptor = "LBA;";
  size_t hash = ComputeModifiedUtf8Hash(descriptor);
  uint32_t class_def_idx = table->Lookup(descriptor, hash);
  ASSERT_EQ(0xFFFFFFFFU, class_def_idx);
}

TEST_F(LookupTableTest, FindNonExistingClassWithCollisions) {
  ScopedObjectAccess soa(Thread::Current());
  std::unique_ptr<const DexFile> dex_file(OpenTestDexFile("Lookup"));
  std::unique_ptr<TypeLookupTable> table(TypeLookupTable::Create(*dex_file));
  ASSERT_NE(nullptr, table.get());
  const char* descriptor = "LDA;";
  size_t hash = ComputeModifiedUtf8Hash(descriptor);
  uint32_t class_def_idx = table->Lookup(descriptor, hash);
  ASSERT_EQ(0xFFFFFFFFU, class_def_idx);
}

TEST_F(LookupTableTest, FindClassNoCollisions) {
  ScopedObjectAccess soa(Thread::Current());
  std::unique_ptr<const DexFile> dex_file(OpenTestDexFile("Lookup"));
  std::unique_ptr<TypeLookupTable> table(TypeLookupTable::Create(*dex_file));
  ASSERT_NE(nullptr, table.get());
  const char* descriptor = "LC;";
  size_t hash = ComputeModifiedUtf8Hash(descriptor);
  uint32_t class_def_idx = table->Lookup(descriptor, hash);
  ASSERT_EQ(2U, class_def_idx);
}

TEST_F(LookupTableTest, FindClassWithCollisions) {
  ScopedObjectAccess soa(Thread::Current());
  std::unique_ptr<const DexFile> dex_file(OpenTestDexFile("Lookup"));
  std::unique_ptr<TypeLookupTable> table(TypeLookupTable::Create(*dex_file));
  ASSERT_NE(nullptr, table.get());
  const char* descriptor = "LAB;";
  size_t hash = ComputeModifiedUtf8Hash(descriptor);
  uint32_t class_def_idx = table->Lookup(descriptor, hash);
  ASSERT_EQ(1U, class_def_idx);
}

}  // namespace art
