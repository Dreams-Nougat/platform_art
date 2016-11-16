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

#ifndef ART_COMPILER_OPTIMIZING_ESCAPE_H_
#define ART_COMPILER_OPTIMIZING_ESCAPE_H_

#include <functional>

namespace art {

class HInstruction;

/*
 * Methods related to escape analysis, i.e. determining whether an object
 * allocation is visible outside ('escapes') its immediate method context.
 */

/*
 * Prototype of a function that takes a reference and use instruction and
 * returns whether the reference can escape.
 */
typedef std::function<bool (HInstruction*, HInstruction*)> NoEscapeFunction;

/*
 * Performs escape analysis on the given instruction, typically a reference to an
 * allocation. The method assigns true to parameter 'is_singleton' if the reference
 * is the only name that can refer to its value during the lifetime of the method,
 * meaning that the reference is not aliased with something else, is not stored to
 * heap memory, and not passed to another method. The method assigns true to parameter
 * 'is_singleton_and_non_escaping' if the reference is a singleton and is not returned
 * to the caller or used as an environment local of an HDeoptimize instruction.
 *
 * When set, the no_escape function is applied to any use of the allocation instruction
 * prior to any built-in escape analysis. This allows clients to define better escape
 * analysis in particular circumstances.
 */
void CalculateEscape(HInstruction* reference,
                     NoEscapeFunction* no_escape,
                     /*out*/ bool* is_singleton,
                     /*out*/ bool* is_singleton_and_non_escaping);

/*
 * Convenience method for testing singleton and non-escaping property at once.
 * Callers should be aware * that this method invokes the full calculation at each call.
 */
bool IsNonEscapingSingleton(HInstruction* reference, NoEscapeFunction* no_escape);

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_ESCAPE_H_
