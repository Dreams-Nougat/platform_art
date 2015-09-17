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

#ifndef ART_COMPILER_OPTIMIZING_OPTIMIZING_COMPILER_STATS_H_
#define ART_COMPILER_OPTIMIZING_OPTIMIZING_COMPILER_STATS_H_

#include <sstream>
#include <string>
#include <type_traits>

#include "atomic.h"

namespace art {

enum MethodCompilationStat {
  kAttemptCompilation = 0,
  kCompiledBaseline,
  kCompiledOptimized,
  kCompiledQuick,
  kInlinedInvoke,
  kInstructionSimplifications,
  kInstructionSimplificationsArch,
  kUnresolvedMethod,
  kUnresolvedField,
  kNotCompiledBranchOutsideMethodCode,
  kNotCompiledCannotBuildSSA,
  kNotCompiledCantAccesType,
  kNotCompiledHugeMethod,
  kNotCompiledLargeMethodNoBranches,
  kNotCompiledMalformedOpcode,
  kNotCompiledNoCodegen,
  kNotCompiledPathological,
  kNotCompiledSpaceFilter,
  kNotCompiledUnhandledInstruction,
  kNotCompiledUnsupportedIsa,
  kNotCompiledVerifyAtRuntime,
  kNotCompiledClassVerifyErrorUnknown,
  kNotCompiledVerifyErrorBadClassHard,
  kNotCompiledVerifyErrorBadClassSoft,
  kNotCompiledVerifyErrorNoClass,
  kNotCompiledVerifyErrorNoField,
  kNotCompiledVerifyErrorNoMethod,
  kNotCompiledVerifyErrorAccessClass,
  kNotCompiledVerifyErrorAccessField,
  kNotCompiledVerifyErrorAccessMethod,
  kNotCompiledVerifyErrorClassChange,
  kNotCompiledVerifyErrorInstantiation,
  kNotCompiledVerifyErrorForceInterpreter,
  kNotCompiledVerifyErrorLocking,
  kNotOptimizedDisabled,
  kNotOptimizedRegisterAllocator,
  kNotOptimizedTryCatch,
  kRemovedCheckedCast,
  kRemovedDeadInstruction,
  kRemovedNullCheck,
  kLastStat
};

class OptimizingCompilerStats {
 public:
  OptimizingCompilerStats() {}

  void RecordStat(MethodCompilationStat stat, size_t count = 1) {
    compile_stats_[stat] += count;
  }

  void Log() const {
    if (compile_stats_[kAttemptCompilation] == 0) {
      LOG(INFO) << "Did not compile any method.";
    } else {
      size_t unoptimized_percent =
          compile_stats_[kCompiledBaseline] * 100 / compile_stats_[kAttemptCompilation];
      size_t optimized_percent =
          compile_stats_[kCompiledOptimized] * 100 / compile_stats_[kAttemptCompilation];
      size_t quick_percent =
          compile_stats_[kCompiledQuick] * 100 / compile_stats_[kAttemptCompilation];
      std::ostringstream oss;
      oss << "Attempted compilation of " << compile_stats_[kAttemptCompilation] << " methods: ";

      oss << unoptimized_percent << "% (" << compile_stats_[kCompiledBaseline] << ") unoptimized, ";
      oss << optimized_percent << "% (" << compile_stats_[kCompiledOptimized] << ") optimized, ";
      oss << quick_percent << "% (" << compile_stats_[kCompiledQuick] << ") quick.";

      LOG(INFO) << oss.str();

      for (int i = 0; i < kLastStat; i++) {
        if (compile_stats_[i] != 0) {
          LOG(INFO) << PrintMethodCompilationStat(static_cast<MethodCompilationStat>(i)) << ": "
              << compile_stats_[i];
        }
      }
    }
  }

 private:
  std::string PrintMethodCompilationStat(MethodCompilationStat stat) const {
    switch (stat) {
      case kAttemptCompilation : return "kAttemptCompilation";
      case kCompiledBaseline : return "kCompiledBaseline";
      case kCompiledOptimized : return "kCompiledOptimized";
      case kCompiledQuick : return "kCompiledQuick";
      case kInlinedInvoke : return "kInlinedInvoke";
      case kInstructionSimplifications: return "kInstructionSimplifications";
      case kInstructionSimplificationsArch: return "kInstructionSimplificationsArch";
      case kUnresolvedMethod : return "kUnresolvedMethod";
      case kUnresolvedField : return "kUnresolvedField";
      case kNotCompiledBranchOutsideMethodCode: return "kNotCompiledBranchOutsideMethodCode";
      case kNotCompiledCannotBuildSSA : return "kNotCompiledCannotBuildSSA";
      case kNotCompiledCantAccesType : return "kNotCompiledCantAccesType";
      case kNotCompiledHugeMethod : return "kNotCompiledHugeMethod";
      case kNotCompiledLargeMethodNoBranches : return "kNotCompiledLargeMethodNoBranches";
      case kNotCompiledMalformedOpcode : return "kNotCompiledMalformedOpcode";
      case kNotCompiledNoCodegen : return "kNotCompiledNoCodegen";
      case kNotCompiledPathological : return "kNotCompiledPathological";
      case kNotCompiledSpaceFilter : return "kNotCompiledSpaceFilter";
      case kNotCompiledUnhandledInstruction : return "kNotCompiledUnhandledInstruction";
      case kNotCompiledUnsupportedIsa : return "kNotCompiledUnsupportedIsa";
      case kNotCompiledVerifyAtRuntime : return "kNotCompiledVerifyAtRuntime";
      case kNotOptimizedDisabled : return "kNotOptimizedDisabled";
      case kNotOptimizedRegisterAllocator : return "kNotOptimizedRegisterAllocator";
      case kNotOptimizedTryCatch : return "kNotOptimizedTryCatch";
      case kNotCompiledClassVerifyErrorUnknown : return "kNotCompiledClassVerifyErrorUnknown";
      case kNotCompiledVerifyErrorBadClassHard : return "kNotCompiledVerifyErrorBadClassHard";
      case kNotCompiledVerifyErrorBadClassSoft : return "kNotCompiledVerifyErrorBadClassSoft";
      case kNotCompiledVerifyErrorNoClass : return "kNotCompiledVerifyErrorNoClass";
      case kNotCompiledVerifyErrorNoField : return "kNotCompiledVerifyErrorNoField";
      case kNotCompiledVerifyErrorNoMethod : return "kNotCompiledVerifyErrorNoMethod";
      case kNotCompiledVerifyErrorAccessClass : return "kNotCompiledVerifyErrorAccessClass";
      case kNotCompiledVerifyErrorAccessField : return "kNotCompiledVerifyErrorAccessField";
      case kNotCompiledVerifyErrorAccessMethod : return "kNotCompiledVerifyErrorAccessMethod";
      case kNotCompiledVerifyErrorClassChange : return "kNotCompiledVerifyErrorClassChange";
      case kNotCompiledVerifyErrorInstantiation : return "kNotCompiledVerifyErrorInstantiation";
      case kNotCompiledVerifyErrorForceInterpreter :
         return "kNotCompiledVerifyErrorForceInterpreter";
      case kNotCompiledVerifyErrorLocking : return "kNotCompiledVerifyErrorLocking";
      case kRemovedCheckedCast: return "kRemovedCheckedCast";
      case kRemovedDeadInstruction: return "kRemovedDeadInstruction";
      case kRemovedNullCheck: return "kRemovedNullCheck";

      case kLastStat: break;  // Invalid to print out.
    }
    LOG(FATAL) << "invalid stat "
        << static_cast<std::underlying_type<MethodCompilationStat>::type>(stat);
    UNREACHABLE();
  }

  AtomicInteger compile_stats_[kLastStat];

  DISALLOW_COPY_AND_ASSIGN(OptimizingCompilerStats);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_OPTIMIZING_COMPILER_STATS_H_
