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

#include "jit_logger.h"

#include "arch/instruction_set.h"
#include "art_method-inl.h"
#include "base/time_utils.h"
#include "base/unix_file/fd_file.h"
#include "driver/compiler_driver.h"
#include "jit/jit.h"
#include "jit/jit_code_cache.h"

namespace art {
namespace jit {

#ifdef ART_TARGET_ANDROID
static const char* kLogPrefix = "/data/misc/trace";
#else
static const char* kLogPrefix = "/tmp";
#endif

// File format of perf-PID.map:
// +---------------------+
// |ADDR SIZE symbolname1|
// |ADDR SIZE symbolname2|
// |...                  |
// +---------------------+
void JitLogger::OpenPerfMapLog() {
  std::string pid_str = std::to_string(getpid());
  std::string perf_filename = std::string(kLogPrefix) + "/perf-" + pid_str + ".map";
  perf_file_.reset(OS::CreateEmptyFileWriteOnly(perf_filename.c_str()));
  if (perf_file_ == nullptr) {
    LOG(ERROR) << "Could not create perf file at " << perf_filename <<
      " Are you on a user build? Perf only works on userdebug/eng builds";
  }
}

void JitLogger::WritePerfMapLog(JitCodeCache* code_cache, ArtMethod* method) {
  if (perf_file_ != nullptr) {
    const void* ptr = method->GetEntryPointFromQuickCompiledCode();
    size_t code_size = code_cache->GetMemorySizeOfCodePointer(ptr);
    std::string method_name = PrettyMethod(method);

    std::ostringstream stream;
    stream << std::hex
           << reinterpret_cast<uintptr_t>(ptr)
           << " "
           << code_size
           << " "
           << PrettyMethod(method)
           << std::endl;
    std::string str = stream.str();
    bool res = perf_file_->WriteFully(str.c_str(), str.size());
    if (!res) {
      LOG(WARNING) << "Failed to write jitted method info in log: write failure.";
    }
  } else {
    LOG(WARNING) << "Failed to write jitted method info in log: log file doesn't exist.";
  }
}

void JitLogger::ClosePerfMapLog() {
  if (perf_file_ != nullptr) {
    UNUSED(perf_file_->Flush());
    UNUSED(perf_file_->Close());
  }
}

//  File format of jit-PID.jump:
//
//  +--------------------------------+
//  |  PerfJitHeader                 |
//  +--------------------------------+
//  |  PerfJitCodeLoad {             | .
//  |    struct PerfJitBase;         |  .
//  |    uint32_t process_id_;       |   .
//  |    uint32_t thread_id_;        |   .
//  |    uint64_t vma_;              |   .
//  |    uint64_t code_address_;     |   .
//  |    uint64_t code_size_;        |   .
//  |    uint64_t code_id_;          |   .
//  |  }                             |   .
//  +-                              -+   .
//  |  method_name'\0'               |   +--> one jitted method
//  +-                              -+   .
//  |  jitted code binary            |   .
//  |  ...                           |   .
//  +--------------------------------+   .
//  |  PerfJitCodeDebugInfo     {    |   .
//  |    struct PerfJitBase;         |   .
//  |    uint64_t address_;          |   .
//  |    uint64_t entry_count_;      |   .
//  |    struct PerfJitDebugEntry;   |  .
//  |  }                             | .
//  +--------------------------------+
//  |  PerfJitCodeLoad               |
//     ...
//
struct PerfJitHeader {
  uint32_t magic_;            // Characters "JiTD"
  uint32_t version_;          // Header version
  uint32_t size_;             // Total size of header
  uint32_t elf_mach_target_;  // Elf mach target
  uint32_t reserved_;         // Reserved, currently not used
  uint32_t process_id_;       // JIT process id
  uint64_t time_stamp_;       // Timestamp of the header
  uint64_t flags_;            // Jit dump file information flags

  static const uint32_t kMagic = 0x4A695444;  // "JiTD"
  static const uint32_t kVersion = 1;
};

// Each record starts with such basic information: event type, total size, and timestamp.
struct PerfJitBase {
  enum PerfJitEvent {
    kLoad = 0,           // Logs a jitted code load event.
    kMove = 1,           // Logs a jitted code move event.
    kDebugInfo = 2,      // Logs debug line/column information.
    kClose = 3           // Logs JIT VM end of life event.
  };
  uint32_t event_;       // Must be one of the events defined in PerfJitEvent.
  uint32_t size_;        // Total size of this event record.
  uint64_t time_stamp_;  // Timestamp for the event.
};

// Logs a jitted code load event, i.e. a recently generated jitted method by ART JIT.
struct PerfJitCodeLoad : PerfJitBase {
  uint32_t process_id_;    // Process id of the JIT.
  uint32_t thread_id_;     // Thread id of the jitted code.
  uint64_t vma_;
  uint64_t code_address_;  // Address where is jitted code is loaded.
  uint64_t code_size_;     // Size of the jited code.
  uint64_t code_id_;       // ID for each jitted code.
};

// Logs debug line information, for source line/column mapping.
struct PerfJitCodeDebugInfo : PerfJitBase {
  uint64_t address_;       // Starting code address which the debug info describes.
  uint64_t entry_count_;   // How many instances of PerfJitDebugEntry.
  // Followed by entry_count_ instances of PerfJitDebugEntry.
};

struct PerfJitDebugEntry {
  uint64_t address_;      // Code address which maps to the line/column in source.
  uint32_t line_number_;  // Source line number starting at 1.
  uint32_t column_;       // Column discriminator, default 0.
  // Followed by null-terminated name or \0xff\0 if same as previous.
};

static uint32_t GetElfMach() {
#if defined(__arm__)
  static const uint32_t kElfMachARM = 0x28;
  return kElfMachARM;
#elif defined(__aarch64__)
  static const uint32_t kElfMachARM64 = 0xB7;
  return kElfMachARM64;
#elif defined(__i386__)
  static const uint32_t kElfMachIA32 = 0x3;
  return kElfMachIA32;
#elif defined(__x86_64__)
  static const uint32_t kElfMachX64 = 0x3E;
  return kElfMachX64;
#else
  UNIMPLEMENTED(WARNING) << "Unsupported architecture in JitLogger";
  return 0;
#endif
}

void JitLogger::OpenMarkerFile() {
  int fd = jit_dump_file_->Fd();
  // The 'perf inject' tool requires that the jit-PID.dump file
  // must have a mmap(PROT_READ|PROT_EXEC) record in perf.data.
  marker_address_ = mmap(nullptr, kPageSize, PROT_READ | PROT_EXEC, MAP_PRIVATE, fd, 0);
  if (marker_address_ == MAP_FAILED) {
    LOG(WARNING) << "Failed to create record in perf.data. JITed code profiling will not work.";
    return;
  }
}

void JitLogger::CloseMarkerFile() {
  if (marker_address_ != nullptr) {
    munmap(marker_address_, kPageSize);
  }
}

void JitLogger::WriteJitDumpDebugInfo() {
  // In the future, we can add java source file line/column mapping here.
}

void JitLogger::WriteJitDumpHeader() {
  PerfJitHeader header;

  std::memset(&header, 0, sizeof(header));
  header.magic_ = PerfJitHeader::kMagic;
  header.version_ = PerfJitHeader::kVersion;
  header.size_ = sizeof(header);
  header.elf_mach_target_ = GetElfMach();
  header.process_id_ = getpid();
  header.time_stamp_ = art::NanoTime();  // CLOCK_MONOTONIC clock is required.
  header.flags_ = 0;

  bool res = jit_dump_file_->WriteFully(reinterpret_cast<const char*>(&header), sizeof(header));
  if (!res) {
    LOG(WARNING) << "Failed to write profiling log. The 'perf inject' tool will not work.";
  }
}

void JitLogger::OpenJitDumpLog() {
  std::string pid_str = std::to_string(getpid());
  std::string jitdump_filename = std::string(kLogPrefix) + "/jit-" + pid_str + ".dump";

  jit_dump_file_.reset(OS::CreateEmptyFile(jitdump_filename.c_str()));
  if (jit_dump_file_ == nullptr) {
    LOG(ERROR) << "Could not create jit dump file at " << jitdump_filename <<
      " Are you on a user build? Perf only works on userdebug/eng builds";
    return;
  }

  OpenMarkerFile();
  WriteJitDumpHeader();
}

void JitLogger::WriteJitDumpLog(JitCodeCache* code_cache, ArtMethod* method) {
  if (jit_dump_file_ != nullptr) {
    const void* code = method->GetEntryPointFromQuickCompiledCode();
    size_t code_size = code_cache->GetMemorySizeOfCodePointer(code);
    std::string method_name = PrettyMethod(method);

    PerfJitCodeLoad jit_code;
    std::memset(&jit_code, 0, sizeof(jit_code));
    jit_code.event_ = PerfJitCodeLoad::kLoad;
    jit_code.size_ = sizeof(jit_code) + method_name.size() + 1 + code_size;
    jit_code.time_stamp_ = art::NanoTime();    // CLOCK_MONOTONIC clock is required.
    jit_code.process_id_ = static_cast<uint32_t>(getpid());
    jit_code.thread_id_ = jit_code.process_id_;  // Use PID here to avoid confusing profiler.
    jit_code.vma_ = 0x0;
    jit_code.code_address_ = reinterpret_cast<uint64_t>(code);
    jit_code.code_size_ = code_size;
    jit_code.code_id_ = code_index_++;

    // Write one complete jitted method info, including:
    // - PerfJitCodeLoad structure
    // - Method name
    // - Complete generated code of this method
    //
    // Use UNUSED() here to avoid compiler warnings.
    UNUSED(jit_dump_file_->WriteFully(reinterpret_cast<const char*>(&jit_code), sizeof(jit_code)));
    UNUSED(jit_dump_file_->WriteFully(method_name.c_str(), method_name.size() + 1));
    UNUSED(jit_dump_file_->WriteFully(code, code_size));
    WriteJitDumpDebugInfo();
  } else {
    LOG(WARNING) << "Failed to write jitted method info in log: 'perf inject' tool will not work.";
  }
}

void JitLogger::CloseJitDumpLog() {
  if (jit_dump_file_ != nullptr) {
    CloseMarkerFile();
    UNUSED(jit_dump_file_->Flush());
    UNUSED(jit_dump_file_->Close());
  }
}

}  // namespace jit
}  // namespace art
