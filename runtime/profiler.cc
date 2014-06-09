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

#include "profiler.h"

#include <fstream>
#include <sys/uio.h>
#include <sys/file.h>

#include "base/stl_util.h"
#include "base/unix_file/fd_file.h"
#include "class_linker.h"
#include "common_throws.h"
#include "debugger.h"
#include "dex_file-inl.h"
#include "instrumentation.h"
#include "mirror/art_method-inl.h"
#include "mirror/class-inl.h"
#include "mirror/dex_cache.h"
#include "mirror/object_array-inl.h"
#include "mirror/object-inl.h"
#include "object_utils.h"
#include "os.h"
#include "scoped_thread_state_change.h"
#include "ScopedLocalRef.h"
#include "thread.h"
#include "thread_list.h"

#ifdef HAVE_ANDROID_OS
#include "cutils/properties.h"
#endif

#if !defined(ART_USE_PORTABLE_COMPILER)
#include "entrypoints/quick/quick_entrypoints.h"
#endif

namespace art {

BackgroundMethodSamplingProfiler* BackgroundMethodSamplingProfiler::profiler_ = nullptr;
pthread_t BackgroundMethodSamplingProfiler::profiler_pthread_ = 0U;
volatile bool BackgroundMethodSamplingProfiler::shutting_down_ = false;

// TODO: this profiler runs regardless of the state of the machine.  Maybe we should use the
// wakelock or something to modify the run characteristics.  This can be done when we
// have some performance data after it's been used for a while.


// This is called from either a thread list traversal or from a checkpoint.  Regardless
// of which caller, the mutator lock must be held.
static void GetSample(Thread* thread, void* arg) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  BackgroundMethodSamplingProfiler* profiler =
      reinterpret_cast<BackgroundMethodSamplingProfiler*>(arg);
  uint32_t dex_pc;
  mirror::ArtMethod* method = thread->GetCurrentMethod(&dex_pc);
  if (false && method == nullptr) {
    LOG(INFO) << "No current method available";
    std::ostringstream os;
    thread->Dump(os);
    std::string data(os.str());
    LOG(INFO) << data;
  }
  profiler->RecordMethod(method, dex_pc);
}

// A closure that is called by the thread checkpoint code.
class SampleCheckpoint : public Closure {
 public:
  explicit SampleCheckpoint(BackgroundMethodSamplingProfiler* const profiler) :
    profiler_(profiler) {}

  virtual void Run(Thread* thread) NO_THREAD_SAFETY_ANALYSIS {
    Thread* self = Thread::Current();
    if (thread == nullptr) {
      LOG(ERROR) << "Checkpoint with nullptr thread";
      return;
    }

    // Grab the mutator lock (shared access).
    ScopedObjectAccess soa(self);

    // Grab a sample.
    GetSample(thread, this->profiler_);

    // And finally tell the barrier that we're done.
    this->profiler_->GetBarrier().Pass(self);
  }

 private:
  BackgroundMethodSamplingProfiler* const profiler_;
};

bool BackgroundMethodSamplingProfiler::ShuttingDown(Thread* self) {
  MutexLock mu(self, *Locks::profiler_lock_);
  return shutting_down_;
}

void* BackgroundMethodSamplingProfiler::RunProfilerThread(void* arg) {
  Runtime* runtime = Runtime::Current();
  BackgroundMethodSamplingProfiler* profiler =
      reinterpret_cast<BackgroundMethodSamplingProfiler*>(arg);

  // Add a random delay for the first time run so that we don't hammer the CPU
  // with all profiles running at the same time.
  const int kRandomDelayMaxSecs = 30;
  const double kMaxBackoffSecs = 24*60*60;   // Max backoff time.

  srand(MicroTime() * getpid());
  int startup_delay = rand() % kRandomDelayMaxSecs;   // random delay for startup.


  CHECK(runtime->AttachCurrentThread("Profiler", true, runtime->GetSystemThreadGroup(),
                                      !runtime->IsCompiler()));

  Thread* self = Thread::Current();

  double backoff = 1.0;
  while (true) {
    if (ShuttingDown(self)) {
      break;
    }

    {
      // wait until we need to run another profile
      uint64_t delay_secs = profiler->options_.GetPeriodS() * backoff;

      // Add a startup delay to prevent all the profiles running at once.
      delay_secs += startup_delay;

      // Immediate startup for benchmarking?
      if (profiler->options_.GetStartImmediately() && startup_delay > 0) {
        delay_secs = 0;
      }

      startup_delay = 0;

      VLOG(profiler) << "Delaying profile start for " << delay_secs << " secs";
      MutexLock mu(self, profiler->wait_lock_);
      profiler->period_condition_.TimedWait(self, delay_secs * 1000, 0);

      // Expand the backoff by its coefficient, but don't go beyond the max.
      backoff = std::min(backoff * profiler->options_.GetBackoffCoefficient(), kMaxBackoffSecs);
    }

    if (ShuttingDown(self)) {
      break;
    }


    uint64_t start_us = MicroTime();
    uint64_t end_us = start_us + profiler->options_.GetDurationS() * UINT64_C(1000000);
    uint64_t now_us = start_us;

    VLOG(profiler) << "Starting profiling run now for "
                   << PrettyDuration((end_us - start_us) * 1000);

    SampleCheckpoint check_point(profiler);

    size_t valid_samples = 0;
    while (now_us < end_us) {
      if (ShuttingDown(self)) {
        break;
      }

      usleep(profiler->options_.GetIntervalUs());    // Non-interruptible sleep.

      ThreadList* thread_list = runtime->GetThreadList();

      profiler->profiler_barrier_->Init(self, 0);
      size_t barrier_count = thread_list->RunCheckpointOnRunnableThreads(&check_point);

      // All threads are suspended, nothing to do.
      if (barrier_count == 0) {
        now_us = MicroTime();
        continue;
      }

      valid_samples += barrier_count;

      ScopedThreadStateChange tsc(self, kWaitingForCheckPointsToRun);

      // Wait for the barrier to be crossed by all runnable threads.  This wait
      // is done with a timeout so that we can detect problems with the checkpoint
      // running code.  We should never see this.
      const uint32_t kWaitTimeoutMs = 10000;
      const uint32_t kWaitTimeoutUs = kWaitTimeoutMs * 1000;

      uint64_t waitstart_us = MicroTime();
      // Wait for all threads to pass the barrier.
      profiler->profiler_barrier_->Increment(self, barrier_count, kWaitTimeoutMs);
      uint64_t waitend_us = MicroTime();
      uint64_t waitdiff_us = waitend_us - waitstart_us;

      // We should never get a timeout.  If we do, it suggests a problem with the checkpoint
      // code.  Crash the process in this case.
      CHECK_LT(waitdiff_us, kWaitTimeoutUs);

      // Update the current time.
      now_us = MicroTime();
    }

    if (valid_samples > 0) {
      // After the profile has been taken, write it out.
      ScopedObjectAccess soa(self);   // Acquire the mutator lock.
      uint32_t size = profiler->WriteProfile();
      VLOG(profiler) << "Profile size: " << size;
    }
  }

  LOG(INFO) << "Profiler shutdown";
  runtime->DetachCurrentThread();
  return nullptr;
}

// Write out the profile file if we are generating a profile.
uint32_t BackgroundMethodSamplingProfiler::WriteProfile() {
  std::string full_name = output_filename_;
  VLOG(profiler) << "Saving profile to " << full_name;

  int fd = open(full_name.c_str(), O_RDWR);
  if (fd < 0) {
    // Open failed.
    LOG(ERROR) << "Failed to open profile file " << full_name;
    return 0;
  }

  // Lock the file for exclusive access.  This will block if another process is using
  // the file.
  int err = flock(fd, LOCK_EX);
  if (err < 0) {
    LOG(ERROR) << "Failed to lock profile file " << full_name;
    return 0;
  }

  // Read the previous profile.
  profile_table_.ReadPrevious(fd, options_.GetProfileType());

  // Move back to the start of the file.
  lseek(fd, 0, SEEK_SET);

  // Format the profile output and write to the file.
  std::ostringstream os;
  uint32_t num_methods = DumpProfile(os);
  std::string data(os.str());
  const char *p = data.c_str();
  size_t length = data.length();
  size_t full_length = length;
  do {
    int n = ::write(fd, p, length);
    p += n;
    length -= n;
  } while (length > 0);

  // Truncate the file to the new length.
  ftruncate(fd, full_length);

  // Now unlock the file, allowing another process in.
  err = flock(fd, LOCK_UN);
  if (err < 0) {
    LOG(ERROR) << "Failed to unlock profile file " << full_name;
  }

  // Done, close the file.
  ::close(fd);

  // Clean the profile for the next time.
  CleanProfile();

  return num_methods;
}

bool BackgroundMethodSamplingProfiler::Start(
    const std::string& output_filename, const ProfilerOptions& options) {
  if (!options.IsEnabled()) {
    LOG(INFO) << "Profiler disabled. To enable setprop dalvik.vm.profiler 1.";
    return false;
  }

  CHECK(!output_filename.empty());

  Thread* self = Thread::Current();
  {
    MutexLock mu(self, *Locks::profiler_lock_);
    // Don't start two profiler threads.
    if (profiler_ != nullptr) {
      return true;
    }
  }

  LOG(INFO) << "Starting profiler using output file: " << output_filename
            << " and options: " << options;
  {
    MutexLock mu(self, *Locks::profiler_lock_);
    profiler_ = new BackgroundMethodSamplingProfiler(output_filename, options);

    CHECK_PTHREAD_CALL(pthread_create, (&profiler_pthread_, nullptr, &RunProfilerThread,
        reinterpret_cast<void*>(profiler_)),
                       "Profiler thread");
  }
  return true;
}



void BackgroundMethodSamplingProfiler::Stop() {
  BackgroundMethodSamplingProfiler* profiler = nullptr;
  pthread_t profiler_pthread = 0U;
  {
    MutexLock trace_mu(Thread::Current(), *Locks::profiler_lock_);
    CHECK(!shutting_down_);
    profiler = profiler_;
    shutting_down_ = true;
    profiler_pthread = profiler_pthread_;
  }

  // Now wake up the sampler thread if it sleeping.
  {
    MutexLock profile_mu(Thread::Current(), profiler->wait_lock_);
    profiler->period_condition_.Signal(Thread::Current());
  }
  // Wait for the sample thread to stop.
  CHECK_PTHREAD_CALL(pthread_join, (profiler_pthread, nullptr), "profiler thread shutdown");

  {
    MutexLock mu(Thread::Current(), *Locks::profiler_lock_);
    profiler_ = nullptr;
  }
  delete profiler;
}


void BackgroundMethodSamplingProfiler::Shutdown() {
  Stop();
}

BackgroundMethodSamplingProfiler::BackgroundMethodSamplingProfiler(
  const std::string& output_filename, const ProfilerOptions& options)
    : output_filename_(output_filename),
      options_(options),
      wait_lock_("Profile wait lock"),
      period_condition_("Profile condition", wait_lock_),
      profile_table_(wait_lock_),
      profiler_barrier_(new Barrier(0)) {
  // Populate the filtered_methods set.
  // This is empty right now, but to add a method, do this:
  //
  // filtered_methods_.insert("void java.lang.Object.wait(long, int)");
}

// A method has been hit, record its invocation in the method map.
// The mutator_lock must be held (shared) when this is called.
void BackgroundMethodSamplingProfiler::RecordMethod(mirror::ArtMethod* method, uint32_t dex_pc) {
  if (method == nullptr) {
    profile_table_.NullMethod();
    // Don't record a nullptr method.
    return;
  }

  mirror::Class* cls = method->GetDeclaringClass();
  if (cls != nullptr) {
    if (cls->GetClassLoader() == nullptr) {
      // Don't include things in the boot
      profile_table_.BootMethod();
      return;
    }
  }

  bool is_filtered = false;

  if (strcmp(method->GetName(), "<clinit>") == 0) {
    // always filter out class init
    is_filtered = true;
  }

  // Filter out methods by name if there are any.
  if (!is_filtered && filtered_methods_.size() > 0) {
    std::string method_full_name = PrettyMethod(method);

    // Don't include specific filtered methods.
    is_filtered = filtered_methods_.count(method_full_name) != 0;
  }

  // Add to the profile table unless it is filtered out.
  if (!is_filtered) {
    if (options_.GetProfileType() == kProfilerMethod) {
      profile_table_.Put(method);
    } else if (options_.GetProfileType() == kProfilerMethodAndDexPC) {
      profile_table_.PutDexPC(method, dex_pc);
    }
  }
}

// Clean out any recordings for the method traces.
void BackgroundMethodSamplingProfiler::CleanProfile() {
  profile_table_.Clear();
}

uint32_t BackgroundMethodSamplingProfiler::DumpProfile(std::ostream& os) {
  return profile_table_.Write(os, options_.GetProfileType());
}

// Profile Table.
// This holds a mapping of mirror::ArtMethod* to a count of how many times a sample
// hit it at the top of the stack.
ProfileSampleResults::ProfileSampleResults(Mutex& lock) : lock_(lock), num_samples_(0),
    num_null_methods_(0),
    num_boot_methods_(0) {
  for (int i = 0; i < kHashSize; i++) {
    table[i] = nullptr;
    dex_table[i] = nullptr;
  }
}

ProfileSampleResults::~ProfileSampleResults() {
  for (int i = 0; i < kHashSize; i++) {
     delete table[i];
     if (dex_table[i] != nullptr) {
       for (MethodDexPCMap::iterator di = dex_table[i]->begin(); di != dex_table[i]->end(); ++di) {
         MethodDexPCCount* dex_pc_counts = di->second;
         if (dex_pc_counts != nullptr) {
           delete dex_pc_counts->dex_pc_map_;
           delete dex_pc_counts;
         }
       }
     }
     delete dex_table[i];
  }
  for (PreviousProfile::iterator pi = previous_.begin(); pi != previous_.end(); ++pi) {
    delete pi->second.dex_pc_map_;
  }
}

// Add a method to the profile table.  If it's the first time the method
// has been seen, add it with count=1, otherwise increment the count.
void ProfileSampleResults::Put(mirror::ArtMethod* method) {
  MutexLock mu(Thread::Current(), lock_);
  uint32_t index = Hash(method);
  if (table[index] == nullptr) {
    table[index] = new Map();
  }
  Map::iterator i = table[index]->find(method);
  if (i == table[index]->end()) {
    (*table[index])[method] = 1;
  } else {
    i->second++;
  }
  num_samples_++;
}

// Add a method with dex pc to the profile table
void ProfileSampleResults::PutDexPC(mirror::ArtMethod* method, uint32_t dex_pc) {
  MutexLock mu(Thread::Current(), lock_);
  uint32_t index = Hash(method);
  if (dex_table[index] == nullptr) {
    dex_table[index] = new MethodDexPCMap();
  }
  MethodDexPCMap::iterator i = dex_table[index]->find(method);
  if (i == dex_table[index]->end()) {
    MethodDexPCCount* new_dex_pc_count = new MethodDexPCCount();
    new_dex_pc_count->total_count_ = 1;
    new_dex_pc_count->dex_pc_map_ = new DexPCCountMap();
    (*new_dex_pc_count->dex_pc_map_)[dex_pc] = 1;
    (*dex_table[index])[method] = new_dex_pc_count;
  } else {
    MethodDexPCCount* dex_pc_count = i->second;
    dex_pc_count->total_count_++;
    DexPCCountMap::iterator dex_pc_i = dex_pc_count->dex_pc_map_->find(dex_pc);
    if (dex_pc_i == dex_pc_count->dex_pc_map_->end()) {
      (*dex_pc_count->dex_pc_map_)[dex_pc] = 1;
    } else {
      dex_pc_i->second++;
    }
  }
  num_samples_++;
}

// Write the profile table to the output stream.  Also merge with the previous profile.
uint32_t ProfileSampleResults::Write(std::ostream& os, ProfileDataType type) {
  ScopedObjectAccess soa(Thread::Current());
  num_samples_ += previous_num_samples_;
  num_null_methods_ += previous_num_null_methods_;
  num_boot_methods_ += previous_num_boot_methods_;

  VLOG(profiler) << "Profile: "
                 << num_samples_ << "/" << num_null_methods_ << "/" << num_boot_methods_;
  os << num_samples_ << "/" << num_null_methods_ << "/" << num_boot_methods_ << "\n";
  uint32_t num_methods = 0;
  if (type == kProfilerMethod) {
    for (int i = 0 ; i < kHashSize; i++) {
      Map *map = table[i];
      if (map != nullptr) {
        for (const auto &meth_iter : *map) {
          mirror::ArtMethod *method = meth_iter.first;
          std::string method_name = PrettyMethod(method);

          const DexFile::CodeItem* codeitem = method->GetCodeItem();
          uint32_t method_size = 0;
          if (codeitem != nullptr) {
            method_size = codeitem->insns_size_in_code_units_;
          }
          uint32_t count = meth_iter.second;

          // Merge this profile entry with one from a previous run (if present).  Also
          // remove the previous entry.
          PreviousProfile::iterator pi = previous_.find(method_name);
          if (pi != previous_.end()) {
            count += pi->second.count_;
            previous_.erase(pi);
          }
          os << StringPrintf("%s/%u/%u\n",  method_name.c_str(), count, method_size);
          ++num_methods;
        }
      }
    }
  } else if (type == kProfilerMethodAndDexPC) {
    for (int i = 0 ; i < kHashSize; i++) {
      MethodDexPCMap *dex_map = dex_table[i];
      if (dex_map != nullptr) {
        for (const auto &dex_pc_iter : *dex_map) {
          mirror::ArtMethod *method = dex_pc_iter.first;
          std::string method_name = PrettyMethod(method);

          const DexFile::CodeItem* codeitem = method->GetCodeItem();
          uint32_t method_size = 0;
          if (codeitem != nullptr) {
            method_size = codeitem->insns_size_in_code_units_;
          }
          MethodDexPCCount* dex_pc_counts = dex_pc_iter.second;
          uint32_t total_count = dex_pc_counts->total_count_;
          DexPCCountMap* dex_pc_map = dex_pc_counts->dex_pc_map_;

          PreviousProfile::iterator pi = previous_.find(method_name);
          if (pi != previous_.end()) {
            total_count += pi->second.count_;
            DexPCCountMap* previous_dex_pc_map = pi->second.dex_pc_map_;
            if (previous_dex_pc_map != nullptr) {
              for (DexPCCountMap::iterator dex_pc_i = previous_dex_pc_map->begin();
                  dex_pc_i != previous_dex_pc_map->end(); ++previous_dex_pc_map) {
                uint32_t dex_pc = dex_pc_i->first;
                uint32_t count = dex_pc_i->second;
                DexPCCountMap::iterator di = dex_pc_map->find(dex_pc);
                if (di != dex_pc_map->end()) {
                  (*dex_pc_map)[dex_pc] += count;
                } else {
                  (*dex_pc_map)[dex_pc] = count;
                }
              }
            }
            delete previous_dex_pc_map;
            previous_.erase(pi);
          }
          // We write out profile data with dex pc information in the following format:
          // "method/total_count/size/[pc_1:count_1,pc_2:count_2,...]".
          os << StringPrintf("%s/%u/%u/[", method_name.c_str(), total_count , method_size);
          std::vector<std::string> dex_pc_count_vector;
          for (DexPCCountMap::iterator dex_pc_i = dex_pc_map->begin(); dex_pc_i != dex_pc_map->end(); ++dex_pc_i) {
            dex_pc_count_vector.push_back(StringPrintf("%u:%u", dex_pc_i->first, dex_pc_i->second));
          }
          os << Join(dex_pc_count_vector, ',');
          os << "]\n";
          ++num_methods;
        }
      }
    }
  }

  // Now we write out the remaining previous methods.
  for (PreviousProfile::iterator pi = previous_.begin(); pi != previous_.end(); ++pi) {
    if (type == kProfilerMethod) {
      os << StringPrintf("%s/%u/%u\n",  pi->first.c_str(), pi->second.count_, pi->second.method_size_);
    } else if (type == kProfilerMethodAndDexPC) {
      os << StringPrintf("%s/%u/%u/[",  pi->first.c_str(), pi->second.count_, pi->second.method_size_);
      DexPCCountMap* previous_dex_pc_map = pi->second.dex_pc_map_;
      if (previous_dex_pc_map != nullptr) {
        std::vector<std::string> dex_pc_count_vector;
        for (DexPCCountMap::iterator dex_pc_i = previous_dex_pc_map->begin();
            dex_pc_i != previous_dex_pc_map->end(); ++dex_pc_i) {
          dex_pc_count_vector.push_back(StringPrintf("%u:%u", dex_pc_i->first, dex_pc_i->second));
        }
        os << Join(dex_pc_count_vector, ',');
      }
      os << "]\n";
    }
    ++num_methods;
  }
  return num_methods;
}

void ProfileSampleResults::Clear() {
  num_samples_ = 0;
  num_null_methods_ = 0;
  num_boot_methods_ = 0;
  for (int i = 0; i < kHashSize; i++) {
    delete table[i];
    table[i] = nullptr;
    if (dex_table[i] != nullptr) {
      for (MethodDexPCMap::iterator di = dex_table[i]->begin(); di != dex_table[i]->end(); ++di) {
        MethodDexPCCount* dex_pc_counts = di->second;
        if (dex_pc_counts != nullptr) {
          delete dex_pc_counts->dex_pc_map_;
          dex_pc_counts->dex_pc_map_ = nullptr;
          delete dex_pc_counts;
          dex_pc_counts = nullptr;
        }
      }
    }
    delete dex_table[i];
    dex_table[i] = nullptr;
  }
  for (PreviousProfile::iterator pi = previous_.begin(); pi != previous_.end(); ++pi) {
    delete pi->second.dex_pc_map_;
    pi->second.dex_pc_map_ = nullptr;
  }
  previous_.clear();
}

uint32_t ProfileSampleResults::Hash(mirror::ArtMethod* method) {
  return (PointerToLowMemUInt32(method) >> 3) % kHashSize;
}

// Read a single line into the given string.  Returns true if everything OK, false
// on EOF or error.
static bool ReadProfileLine(int fd, std::string& line) {
  char buf[4];
  line.clear();
  while (true) {
    int n = read(fd, buf, 1);     // TODO: could speed this up but is it worth it?
    if (n != 1) {
      return false;
    }
    if (buf[0] == '\n') {
      break;
    }
    line += buf[0];
  }
  return true;
}

void ProfileSampleResults::ReadPrevious(int fd, ProfileDataType type) {
  // Reset counters.
  previous_num_samples_ = previous_num_null_methods_ = previous_num_boot_methods_ = 0;

  std::string line;

  // The first line contains summary information.
  if (!ReadProfileLine(fd, line)) {
    return;
  }
  std::vector<std::string> summary_info;
  Split(line, '/', summary_info);
  if (summary_info.size() != 3) {
    // Bad summary info.  It should be count/nullcount/bootcount
    return;
  }
  previous_num_samples_ = atoi(summary_info[0].c_str());
  previous_num_null_methods_ = atoi(summary_info[1].c_str());
  previous_num_boot_methods_ = atoi(summary_info[2].c_str());

  // Now read each line until the end of file.  Each line consists of 3 or 4 fields separated by /
  while (true) {
    if (!ReadProfileLine(fd, line)) {
      break;
    }
    std::vector<std::string> info;
    Split(line, '/', info);
    if (info.size() != 3 && info.size() != 4) {
      // Malformed.
      break;
    }
    std::string methodname = info[0];
    uint32_t total_count = atoi(info[1].c_str());
    uint32_t size = atoi(info[2].c_str());
    DexPCCountMap* dex_pc_map = nullptr;
    if (type == kProfilerMethodAndDexPC && info.size() == 4) {
      dex_pc_map = new DexPCCountMap();
      std::string dex_pc_counts_str = info[3].substr(1, info[3].size() - 2);
      std::vector<std::string> dex_pc_count_pairs;
      Split(dex_pc_counts_str, ',', dex_pc_count_pairs);
      for (uint32_t i = 0; i < dex_pc_count_pairs.size(); ++i) {
        std::vector<std::string> dex_pc_count;
        Split(dex_pc_count_pairs[i], ':', dex_pc_count);
        uint32_t dex_pc = atoi(dex_pc_count[0].c_str());
        uint32_t count = atoi(dex_pc_count[1].c_str());
        (*dex_pc_map)[dex_pc] = count;
      }
    }
    previous_[methodname] = PreviousValue(total_count, size, dex_pc_map);
  }
}

bool ProfileFile::LoadFile(const std::string& fileName) {
  LOG(VERBOSE) << "reading profile file " << fileName;
  struct stat st;
  int err = stat(fileName.c_str(), &st);
  if (err == -1) {
    LOG(VERBOSE) << "not found";
    return false;
  }
  if (st.st_size == 0) {
    return false;  // Empty profiles are invalid.
  }
  std::ifstream in(fileName.c_str());
  if (!in) {
    LOG(VERBOSE) << "profile file " << fileName << " exists but can't be opened";
    LOG(VERBOSE) << "file owner: " << st.st_uid << ":" << st.st_gid;
    LOG(VERBOSE) << "me: " << getuid() << ":" << getgid();
    LOG(VERBOSE) << "file permissions: " << std::oct << st.st_mode;
    LOG(VERBOSE) << "errno: " << errno;
    return false;
  }
  // The first line contains summary information.
  std::string line;
  std::getline(in, line);
  if (in.eof()) {
    return false;
  }
  std::vector<std::string> summary_info;
  Split(line, '/', summary_info);
  if (summary_info.size() != 3) {
    // Bad summary info.  It should be total/null/boot.
    return false;
  }
  // This is the number of hits in all profiled methods (without nullptr or boot methods)
  uint32_t total_count = atoi(summary_info[0].c_str());

  // Now read each line until the end of file.  Each line consists of 3 fields separated by '/'.
  // Store the info in descending order given by the most used methods.
  typedef std::set<std::pair<int, std::vector<std::string>>> ProfileSet;
  ProfileSet countSet;
  while (!in.eof()) {
    std::getline(in, line);
    if (in.eof()) {
      break;
    }
    std::vector<std::string> info;
    Split(line, '/', info);
    if (info.size() != 3 && info.size() != 4) {
      // Malformed.
      return false;
    }
    int count = atoi(info[1].c_str());
    countSet.insert(std::make_pair(-count, info));
  }

  uint32_t curTotalCount = 0;
  ProfileSet::iterator end = countSet.end();
  const ProfileData* prevData = nullptr;
  for (ProfileSet::iterator it = countSet.begin(); it != end ; it++) {
    const std::string& methodname = it->second[0];
    uint32_t count = -it->first;
    uint32_t size = atoi(it->second[2].c_str());
    double usedPercent = (count * 100.0) / total_count;

    curTotalCount += count;
    // Methods with the same count should be part of the same top K percentage bucket.
    double topKPercentage = (prevData != nullptr) && (prevData->GetCount() == count)
      ? prevData->GetTopKUsedPercentage()
      : 100 * static_cast<double>(curTotalCount) / static_cast<double>(total_count);

    // Add it to the profile map.
    ProfileData curData = ProfileData(methodname, count, size, usedPercent, topKPercentage);
    profile_map_[methodname] = curData;
    prevData = &curData;
  }
  return true;
}

bool ProfileFile::GetProfileData(ProfileFile::ProfileData* data, const std::string& method_name) {
  ProfileMap::iterator i = profile_map_.find(method_name);
  if (i == profile_map_.end()) {
    return false;
  }
  *data = i->second;
  return true;
}

bool ProfileFile::GetTopKSamples(std::set<std::string>& topKSamples, double topKPercentage) {
  ProfileMap::iterator end = profile_map_.end();
  for (ProfileMap::iterator it = profile_map_.begin(); it != end; it++) {
    if (it->second.GetTopKUsedPercentage() < topKPercentage) {
      topKSamples.insert(it->first);
    }
  }
  return true;
}

}  // namespace art
