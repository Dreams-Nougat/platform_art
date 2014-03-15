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

#ifndef ART_RUNTIME_ENTRYPOINTS_PORTABLE_PORTABLE_ENTRYPOINTS_H_
#define ART_RUNTIME_ENTRYPOINTS_PORTABLE_PORTABLE_ENTRYPOINTS_H_

#include "dex_file-inl.h"
#include "runtime.h"

namespace art {
namespace mirror {
  class ArtMethod;
  class Object;
}  // namespace mirror
class Thread;

#define PORTABLE_ENTRYPOINT_OFFSET(ptr_size, x) \
    Thread::PortableEntryPointOffset<ptr_size>(OFFSETOF_MEMBER(PortableEntryPoints, x))

// Pointers to functions that are called by code generated by compiler's adhering to the portable
// compiler ABI.
struct PACKED(4) PortableEntryPoints {
  // Invocation
  void (*pPortableImtConflictTrampoline)(mirror::ArtMethod*);
  void (*pPortableResolutionTrampoline)(mirror::ArtMethod*);
  void (*pPortableToInterpreterBridge)(mirror::ArtMethod*);
};

}  // namespace art

#endif  // ART_RUNTIME_ENTRYPOINTS_PORTABLE_PORTABLE_ENTRYPOINTS_H_
