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

#ifndef ART_RUNTIME_METHOD_HELPER_INL_H_
#define ART_RUNTIME_METHOD_HELPER_INL_H_

#include "method_helper.h"

#include "class_linker.h"
#include "dex_file-inl.h"

namespace art {

template <template <class T> class HandleKind>
template <template <class T2> class HandleKind2>
inline bool MethodHelperT<HandleKind>::HasSameNameAndSignature(MethodHelperT<HandleKind2>* other) {
  const DexFile* dex_file = method_->GetDexFile();
  const DexFile::MethodId& mid = dex_file->GetMethodId(GetMethod()->GetDexMethodIndex());
  if (method_->GetDexCache() == other->method_->GetDexCache()) {
    const DexFile::MethodId& other_mid =
        dex_file->GetMethodId(other->GetMethod()->GetDexMethodIndex());
    return mid.name_idx_ == other_mid.name_idx_ && mid.proto_idx_ == other_mid.proto_idx_;
  }
  const DexFile* other_dex_file = other->method_->GetDexFile();
  const DexFile::MethodId& other_mid =
      other_dex_file->GetMethodId(other->GetMethod()->GetDexMethodIndex());
  if (!DexFileStringEquals(dex_file, mid.name_idx_, other_dex_file, other_mid.name_idx_)) {
    return false;  // Name mismatch.
  }
  return dex_file->GetMethodSignature(mid) == other_dex_file->GetMethodSignature(other_mid);
}

}  // namespace art

#endif  // ART_RUNTIME_METHOD_HELPER_INL_H_
