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

#ifndef ART_COMPILER_ELF_WRITER_DEBUG_H_
#define ART_COMPILER_ELF_WRITER_DEBUG_H_

#include "elf_builder.h"
#include "dwarf/dwarf_constants.h"
#include "oat_writer.h"
#include "utils/array_ref.h"

namespace art {
namespace dwarf {

template<typename ElfTypes>
void WriteCFISection(ElfBuilder<ElfTypes>* builder,
                     const ArrayRef<const MethodDebugInfo>& method_infos,
                     CFIFormat format);

template<typename ElfTypes>
void WriteDebugSections(ElfBuilder<ElfTypes>* builder,
                        const ArrayRef<const MethodDebugInfo>& method_infos);

template <typename ElfTypes>
void WriteDebugSymbols(ElfBuilder<ElfTypes>* builder,
                       const ArrayRef<const dwarf::MethodDebugInfo>& method_infos);

}  // namespace dwarf
}  // namespace art

#endif  // ART_COMPILER_ELF_WRITER_DEBUG_H_
