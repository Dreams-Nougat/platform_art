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
 *
 * Header file of an in-memory representation of DEX files.
 */

#ifndef ART_DEXLAYOUT_DEX_IR_BUILDER_H_
#define ART_DEXLAYOUT_DEX_IR_BUILDER_H_

#include "dex_ir.h"

namespace art {
namespace dex_ir {

dex_ir::Header* DexIrBuilder(const DexFile& dex_file);
ClassDef* ReadClassDef(const DexFile& dex_file,
                       const DexFile::ClassDef& disk_class_def,
                       Header* header);

}  // namespace dex_ir
}  // namespace art

#endif  // ART_DEXLAYOUT_DEX_IR_BUILDER_H_
