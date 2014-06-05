/*
 * Copyright (C) 2014 The Android Open Source Project
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

#include "selectivity.h"
namespace art {
// Initialize the function pointers.
bool (*Selectivity::precompile_summary_logic)(CompilerDriver* driver,
                                              VerificationResults* verification_results) = nullptr;

bool (*Selectivity::skip_class_compile)(const DexFile& dex_file,
                                        const DexFile::ClassDef&) = nullptr;

bool (*Selectivity::skip_method_compile)(const DexFile::CodeItem* code_item,
                                         uint32_t method_idx, uint32_t* access_flags,
                                         uint16_t* class_def_idx, const DexFile& dex_file,
                                         DexToDexCompilationLevel* dex_to_dex_compilation_level) = nullptr;

void (*Selectivity::analyze_resolved_method)(mirror::ArtMethod* method, const DexFile& dex_file) = nullptr;

void (*Selectivity::analyze_verified_method)(verifier::MethodVerifier* verifier) = nullptr;

void (*Selectivity::dump_selectivity_stats)() = nullptr;

void (*Selectivity::toggle_analysis)(bool setting, std::string disable_passes) = nullptr;

CompilerOptions::CompilerFilter Selectivity::original_compiler_filter = CompilerOptions::kO1;

CompilerOptions::CompilerFilter Selectivity::used_compiler_filter = CompilerOptions::kO1;

void Selectivity::SetPreCompileSummaryLogic(bool (*function)(CompilerDriver* driver,
                                                             VerificationResults* verification_results)) {
  if (function != nullptr) {
    precompile_summary_logic = function;
  }
}

bool Selectivity::PreCompileSummaryLogic(CompilerDriver* driver,
                                         VerificationResults* verification_results) {
  if (precompile_summary_logic == nullptr) {
    return false;
  } else {
    return (*precompile_summary_logic)(driver, verification_results);
  }
}

void Selectivity::SetSkipClassCompile(bool (*function)(const DexFile& dex_file,
                                                       const DexFile::ClassDef& class_def)) {
  if (function != nullptr) {
    skip_class_compile = function;
  }
}

bool Selectivity::SkipClassCompile(const DexFile& dex_file,
                                   const DexFile::ClassDef& class_def) {
  if (skip_class_compile == nullptr) {
    return false;
  } else {
    return (*skip_class_compile)(dex_file, class_def);
  }
}

void Selectivity::SetSkipMethodCompile(bool (*function)(const DexFile::CodeItem* code_item,
                                                        uint32_t method_idx, uint32_t* access_flags,
                                                        uint16_t* class_def_idx, const DexFile& dex_file,
                                                        DexToDexCompilationLevel* dex_to_dex_compilation_level)) {
  if (function != nullptr) {
    skip_method_compile = function;
  }
}

bool Selectivity::SkipMethodCompile(const DexFile::CodeItem* code_item,
                                    uint32_t method_idx, uint32_t* access_flags,
                                    uint16_t* class_def_idx, const DexFile& dex_file,
                                    DexToDexCompilationLevel* dex_to_dex_compilation_level) {
  if (skip_method_compile == nullptr) {
    return false;
  } else {
    return (*skip_method_compile)(code_item, method_idx, access_flags, class_def_idx,
                                  dex_file, dex_to_dex_compilation_level);
  }
}

void Selectivity::SetAnalyzeResolvedMethod(void (*function)(mirror::ArtMethod* method,
                                               const DexFile& dex_file)) {
  if (function != nullptr) {
    analyze_resolved_method = function;
  }
}

void Selectivity::AnalyzeResolvedMethod(mirror::ArtMethod* method, const DexFile& dex_file) {
  if (analyze_resolved_method != nullptr) {
    (*analyze_resolved_method)(method, dex_file);
  }
}

void Selectivity::SetAnalyzeVerifiedMethod(void (*function)(verifier::MethodVerifier* verifier)) {
  if (function != nullptr) {
    analyze_verified_method = function;
  }
}

void Selectivity::AnalyzeVerifiedMethod(verifier::MethodVerifier* verifier) {
  if (analyze_verified_method != nullptr) {
    (*analyze_verified_method)(verifier);
  }
}

void Selectivity::SetDumpSelectivityStats(void (*function)()) {
  if (function != nullptr) {
    dump_selectivity_stats = function;
  }
}

void Selectivity::DumpSelectivityStats() {
  if (dump_selectivity_stats != nullptr) {
    (*dump_selectivity_stats)();
  }
}

void Selectivity::SetToggleAnalysis(void (*function)(bool setting, std::string disable_passes)) {
  if (function != nullptr) {
    toggle_analysis = function;
  }
}

void Selectivity::ToggleAnalysis(bool setting, std::string disable_passes) {
  if (toggle_analysis != nullptr) {
    (*toggle_analysis)(setting, disable_passes);
  }
}
}  // namespace art
