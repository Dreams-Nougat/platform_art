/*
 * Copyright (C) 2013 The Android Open Source Project
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

#include "verification_results.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/mutex-inl.h"
#include "driver/compiler_driver.h"
#include "driver/compiler_options.h"
#include "thread.h"
#include "thread-inl.h"
#include "verified_method.h"
#include "verifier/method_verifier-inl.h"

namespace art {

VerificationResults::VerificationResults(const CompilerOptions* compiler_options)
    : compiler_options_(compiler_options),
      verified_methods_lock_("compiler verified methods lock"),
      rejected_classes_lock_("compiler rejected classes lock") {}

VerificationResults::~VerificationResults() {
  WriterMutexLock mu(Thread::Current(), verified_methods_lock_);
  DeleteResults(preregistered_dex_files_);
  STLDeleteValues(&verified_methods_);
}

void VerificationResults::ProcessVerifiedMethod(verifier::MethodVerifier* method_verifier) {
  DCHECK(method_verifier != nullptr);
  MethodReference ref = method_verifier->GetMethodReference();
  bool compile = IsCandidateForCompilation(ref, method_verifier->GetAccessFlags());
  std::unique_ptr<const VerifiedMethod> verified_method(
      VerifiedMethod::Create(method_verifier, compile));
  if (verified_method == nullptr) {
    // We'll punt this later.
    return;
  }
  bool inserted;
  DexFileMethodArray* const array = GetMethodArray(ref.dex_file);
  const VerifiedMethod* existing = nullptr;
  if (array != nullptr) {
    DCHECK(array != nullptr);
    Atomic<const VerifiedMethod*>* slot = &(*array)[ref.dex_method_index];
    inserted = slot->CompareExchangeStrongSequentiallyConsistent(nullptr, verified_method.get());
    if (!inserted) {
      existing = slot->LoadSequentiallyConsistent();
      DCHECK_NE(verified_method.get(), existing);
    }
  } else {
    WriterMutexLock mu(Thread::Current(), verified_methods_lock_);
    auto it = verified_methods_.find(ref);
    inserted = it == verified_methods_.end();
    if (inserted) {
      verified_methods_.Put(ref, verified_method.get());
      DCHECK(verified_methods_.find(ref) != verified_methods_.end());
    } else {
      existing = it->second;
    }
  }
  if (inserted) {
    // Successfully added, release the unique_ptr since we no longer have ownership.
    DCHECK_EQ(GetVerifiedMethod(ref), verified_method.get());
    verified_method.release();
  } else {
    // TODO: Investigate why are we doing the work again for this method and try to avoid it.
    LOG(WARNING) << "Method processed more than once: " << ref.PrettyMethod();
    if (!Runtime::Current()->UseJitCompilation()) {
      DCHECK_EQ(existing->GetDevirtMap().size(), verified_method->GetDevirtMap().size());
      DCHECK_EQ(existing->GetSafeCastSet().size(), verified_method->GetSafeCastSet().size());
    }
    // Let the unique_ptr delete the new verified method since there was already an existing one
    // registered. It is unsafe to replace the existing one since the JIT may be using it to
    // generate a native GC map.
  }
}

const VerifiedMethod* VerificationResults::GetVerifiedMethod(MethodReference ref) {
  DexFileMethodArray* array = GetMethodArray(ref.dex_file);
  if (array != nullptr) {
    return (*array)[ref.dex_method_index].LoadRelaxed();
  }
  ReaderMutexLock mu(Thread::Current(), verified_methods_lock_);
  auto it = verified_methods_.find(ref);
  return (it != verified_methods_.end()) ? it->second : nullptr;
}

void VerificationResults::CreateVerifiedMethodFor(MethodReference ref) {
  DexFileMethodArray* array = GetMethodArray(ref.dex_file);
  DCHECK(array != nullptr);
  // This method should only be called for classes verified at compile time,
  // which have no verifier error, nor has methods that we know will throw
  // at runtime.
  (*array)[ref.dex_method_index].StoreSequentiallyConsistent(
      new VerifiedMethod(/* encountered_error_types */ 0, /* has_runtime_throw */ false));
}

void VerificationResults::AddRejectedClass(ClassReference ref) {
  {
    WriterMutexLock mu(Thread::Current(), rejected_classes_lock_);
    rejected_classes_.insert(ref);
  }
  DCHECK(IsClassRejected(ref));
}

bool VerificationResults::IsClassRejected(ClassReference ref) {
  ReaderMutexLock mu(Thread::Current(), rejected_classes_lock_);
  return (rejected_classes_.find(ref) != rejected_classes_.end());
}

bool VerificationResults::IsCandidateForCompilation(MethodReference&,
                                                    const uint32_t access_flags) {
  if (!compiler_options_->IsBytecodeCompilationEnabled()) {
    return false;
  }
  // Don't compile class initializers unless kEverything.
  if ((compiler_options_->GetCompilerFilter() != CompilerFilter::kEverything) &&
     ((access_flags & kAccConstructor) != 0) && ((access_flags & kAccStatic) != 0)) {
    return false;
  }
  return true;
}

void VerificationResults::PreRegisterDexFile(const DexFile* dex_file) {
  CHECK(preregistered_dex_files_.find(dex_file) == preregistered_dex_files_.end())
      << dex_file->GetLocation();
  DexFileMethodArray array(dex_file->NumMethodIds());
  WriterMutexLock mu(Thread::Current(), verified_methods_lock_);
  // There can be some verified methods that are already registered for the dex_file since we set
  // up well known classes earlier. Remove these and put them in the array so that we don't
  // accidentally miss seeing them.
  for (auto it = verified_methods_.begin(); it != verified_methods_.end(); ) {
    MethodReference ref = it->first;
    if (ref.dex_file == dex_file) {
      array[ref.dex_method_index].StoreSequentiallyConsistent(it->second);
      it = verified_methods_.erase(it);
    } else {
      ++it;
    }
  }
  preregistered_dex_files_.emplace(dex_file, std::move(array));
}

void VerificationResults::DeleteResults(DexFileResults& array) {
  for (auto& pair : array) {
    for (Atomic<const VerifiedMethod*>& method : pair.second) {
      delete method.LoadSequentiallyConsistent();
    }
  }
  array.clear();
}

VerificationResults::DexFileMethodArray* VerificationResults::GetMethodArray(
    const DexFile* dex_file) {
  auto it = preregistered_dex_files_.find(dex_file);
  if (it != preregistered_dex_files_.end()) {
    return &it->second;
  }
  return nullptr;
}

}  // namespace art
