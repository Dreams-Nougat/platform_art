/*
 * Copyright 2016 The Android Open Source Project
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

#ifndef ART_COMPILER_JIT_JIT_LOGGER_H_
#define ART_COMPILER_JIT_JIT_LOGGER_H_

#include "base/mutex.h"
#include "compiled_method.h"
#include "driver/compiler_driver.h"
#include "driver/compiler_options.h"

namespace art {

class ArtMethod;

namespace jit {

//
// JitLogger supports two approaches of perf profiling.
//
// (1) perf-map:
//     The perf-map mechanism generates perf-PID.map file,
//     which provides simple "address, size, method_name" information to perf,
//     and allows perf to map samples in jit-code-cache to jitted method symbols.
//
//     Command line Example:
//       $ perf record dalvikvm -Xcompiler-option --generate-debug-info -cp <classpath> Test
//       $ perf report
//     NOTE:
//       - Make sure that the perf-PID.map file is available for 'perf report' tool to access.
//
//
// (2) perf-inject:
//     The perf-inject mechansim generates jit-PID.dump file,
//     which provides rich informations about a jitted method.
//     It allows perf or other profiling tools to do advanced anlaysis on jitted code,
//     for example instruction level profiling.
//
//     Command line Example:
//       $ perf record -k mono dalvikvm -Xcompiler-option --generate-debug-info -cp <classpath> Test
//       $ perf inject -i perf.data -o perf.data.jitted
//       $ perf report -i perf.data.jitted
//       $ perf annotate -i perf.data.jitted
//     NOTE:
//       - The 'perf record -k mono' option requires 4.1 (or higher version) kernel.
//       - The '-k mono' option tells 'perf record' to use CLOCK_MONOTONIC clock during sampling;
//         which is required by 'perf inject', to make sure that
//         both perf.data and jit-PID.dump have unified clock source for timestamps.
//       - The 'perf inject' tool injects information from jit-PID.dump into perf.data file,
//         and generates small ELF files (jitted-TID-CODEID.so) for each jitted method.
//       - Make sure above small ELF files are available for 'perf annotate' tool to access.
//
class JitLogger {
  public:
    JitLogger() {
      code_index_ = 0;
      marker_address_ = nullptr;
    }

    void OpenLog() {
      OpenPerfMapLog();
      OpenJitDumpLog();
    }

    void WriteLog(JitCodeCache* code_cache, ArtMethod* method)
        REQUIRES_SHARED(Locks::mutator_lock_) {
      WritePerfMapLog(code_cache, method);
      WriteJitDumpLog(code_cache, method);
    }

    void CloseLog() {
      ClosePerfMapLog();
      CloseJitDumpLog();
    }

  private:
    // For perf-map profiling
    void OpenPerfMapLog();
    void WritePerfMapLog(JitCodeCache* code_cache, ArtMethod* method)
        REQUIRES_SHARED(Locks::mutator_lock_);
    void ClosePerfMapLog();

    // For perf-inject profiling
    void OpenJitDumpLog();
    void WriteJitDumpLog(JitCodeCache* code_cache, ArtMethod* method)
        REQUIRES_SHARED(Locks::mutator_lock_);
    void CloseJitDumpLog();

    void OpenMarkerFile();
    void CloseMarkerFile();
    void WriteJitDumpHeader();
    void WriteJitDumpDebugInfo();

    std::unique_ptr<File> perf_file_;
    std::unique_ptr<File> jit_dump_file_;
    uint64_t code_index_;
    void* marker_address_;

    DISALLOW_COPY_AND_ASSIGN(JitLogger);
};

}  // namespace jit
}  // namespace art

#endif  // ART_COMPILER_JIT_JIT_LOGGER_H_
