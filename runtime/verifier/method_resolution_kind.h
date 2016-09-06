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

#ifndef ART_RUNTIME_VERIFIER_METHOD_RESOLUTION_KIND_H_
#define ART_RUNTIME_VERIFIER_METHOD_RESOLUTION_KIND_H_

namespace art {
namespace verifier {

// Values corresponding to the method resolution algorithms defined in mirror::Class.
enum MethodResolutionKind {
  kDirectMethodResolution,
  kVirtualMethodResolution,
  kInterfaceMethodResolution,
};

}  // namespace verifier
}  // namespace art

#endif  // ART_RUNTIME_VERIFIER_METHOD_RESOLUTION_KIND_H_
