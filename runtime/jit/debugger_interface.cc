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

#include "debugger_interface.h"

#include "base/logging.h"
#include "base/mutex.h"
#include "thread-inl.h"
#include "thread.h"

#include <unordered_map>

namespace art {

// -------------------------------------------------------------------
// Binary GDB JIT Interface as described in
//   http://sourceware.org/gdb/onlinedocs/gdb/Declarations.html
// -------------------------------------------------------------------
extern "C" {
  typedef enum {
    JIT_NOACTION = 0,
    JIT_REGISTER_FN,
    JIT_UNREGISTER_FN
  } JITAction;

  struct JITCodeEntry {
    JITCodeEntry* next_;
    JITCodeEntry* prev_;
    const uint8_t *symfile_addr_;
    uint64_t symfile_size_;
  };

  struct JITDescriptor {
    uint32_t version_;
    uint32_t action_flag_;
    JITCodeEntry* relevant_entry_;
    JITCodeEntry* first_entry_;
  };

  // GDB will place breakpoint into this function.
  // To prevent GCC from inlining or removing it we place noinline attribute
  // and inline assembler statement inside.
  void __attribute__((noinline)) __jit_debug_register_code();
  void __attribute__((noinline)) __jit_debug_register_code() {
    __asm__("");
  }

  // GDB will inspect contents of this descriptor.
  // Static initialization is necessary to prevent GDB from seeing
  // uninitialized descriptor.
  JITDescriptor __jit_debug_descriptor = { 1, JIT_NOACTION, nullptr, nullptr };
}

static Mutex g_jit_debug_mutex("JIT debug interface lock");

JITCodeEntry* CreateJITCodeEntry(const uint8_t *symfile_addr, uintptr_t symfile_size) {
  Thread* self = Thread::Current();
  MutexLock mu(self, g_jit_debug_mutex);

  JITCodeEntry* entry = new JITCodeEntry;
  entry->symfile_addr_ = symfile_addr;
  entry->symfile_size_ = symfile_size;
  entry->prev_ = nullptr;

  entry->next_ = __jit_debug_descriptor.first_entry_;
  if (entry->next_ != nullptr) {
    entry->next_->prev_ = entry;
  }
  __jit_debug_descriptor.first_entry_ = entry;
  __jit_debug_descriptor.relevant_entry_ = entry;

  __jit_debug_descriptor.action_flag_ = JIT_REGISTER_FN;
  __jit_debug_register_code();
  return entry;
}

void DeleteJITCodeEntry(JITCodeEntry* entry) {
  Thread* self = Thread::Current();
  MutexLock mu(self, g_jit_debug_mutex);

  if (entry->prev_ != nullptr) {
    entry->prev_->next_ = entry->next_;
  } else {
    __jit_debug_descriptor.first_entry_ = entry->next_;
  }

  if (entry->next_ != nullptr) {
    entry->next_->prev_ = entry->prev_;
  }

  __jit_debug_descriptor.relevant_entry_ = entry;
  __jit_debug_descriptor.action_flag_ = JIT_UNREGISTER_FN;
  __jit_debug_register_code();
  delete entry;
}

// Mapping from address to entry.  It takes ownership of the entries
// so that the user of the JIT interface does not have to store them.
static std::unordered_map<uintptr_t, JITCodeEntry*> g_jit_code_entries;

void CreateJITCodeEntryForAddress(uintptr_t address,
                                  const uint8_t *symfile_addr,
                                  uintptr_t symfile_size) {
  DCHECK_NE(address, 0u);
  DCHECK(g_jit_code_entries.find(address) == g_jit_code_entries.end());
  g_jit_code_entries.emplace(address, CreateJITCodeEntry(symfile_addr, symfile_size));
}

bool DeleteJITCodeEntryForAddress(uintptr_t address) {
  const auto& it = g_jit_code_entries.find(address);
  if (it == g_jit_code_entries.end()) {
    return false;
  }
  DeleteJITCodeEntry(it->second);
  g_jit_code_entries.erase(it);
  return true;
}

}  // namespace art
