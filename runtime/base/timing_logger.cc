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


#define ATRACE_TAG ATRACE_TAG_DALVIK
#include <stdio.h>
#include <cutils/trace.h>

#include "timing_logger.h"

#include "base/logging.h"
#include "thread-inl.h"
#include "base/stl_util.h"
#include "base/histogram-inl.h"

#include <cmath>
#include <iomanip>

namespace art {

constexpr size_t CumulativeLogger::kLowMemoryBucketCount;
constexpr size_t CumulativeLogger::kDefaultBucketCount;
CumulativeLogger::CumulativeLogger(const std::string& name)
    : name_(name),
      lock_name_("CumulativeLoggerLock" + name),
      lock_(lock_name_.c_str(), kDefaultMutexLevel, true) {
  Reset();
}

CumulativeLogger::~CumulativeLogger() {
  STLDeleteElements(&histograms_);
}

void CumulativeLogger::SetName(const std::string& name) {
  MutexLock mu(Thread::Current(), lock_);
  name_.assign(name);
}

void CumulativeLogger::Start() {
}

void CumulativeLogger::End() {
  MutexLock mu(Thread::Current(), lock_);
  iterations_++;
}

void CumulativeLogger::Reset() {
  MutexLock mu(Thread::Current(), lock_);
  iterations_ = 0;
  total_time_ = 0;
  STLDeleteElements(&histograms_);
}

void CumulativeLogger::AddLogger(const TimingLogger &logger) {
  MutexLock mu(Thread::Current(), lock_);
  const TimingLogger::SplitTimings& splits = logger.GetSplits();
  for (auto it = splits.begin(), end = splits.end(); it != end; ++it) {
    TimingLogger::SplitTiming split = *it;
    uint64_t split_time = split.first;
    const char* split_name = split.second;
    AddPair(split_name, split_time);
  }
}

size_t CumulativeLogger::GetIterations() const {
  MutexLock mu(Thread::Current(), lock_);
  return iterations_;
}

void CumulativeLogger::Dump(std::ostream &os) {
  MutexLock mu(Thread::Current(), lock_);
  DumpHistogram(os);
}

void CumulativeLogger::AddPair(const std::string& label, uint64_t delta_time) {
  // Convert delta time to microseconds so that we don't overflow our counters.
  delta_time /= kAdjust;
  total_time_ += delta_time;
  Histogram<uint64_t>* histogram;
  Histogram<uint64_t> dummy(label.c_str());
  auto it = histograms_.find(&dummy);
  if (it == histograms_.end()) {
    const size_t max_buckets = Runtime::Current()->GetHeap()->IsLowMemoryMode() ?
        kLowMemoryBucketCount : kDefaultBucketCount;
    histogram = new Histogram<uint64_t>(label.c_str(), kInitialBucketSize, max_buckets);
    histograms_.insert(histogram);
  } else {
    histogram = *it;
  }
  histogram->AddValue(delta_time);
}

class CompareHistorgramByTimeSpentDeclining {
 public:
  bool operator()(const Histogram<uint64_t>* a, const Histogram<uint64_t>* b) const {
    return a->Sum() > b->Sum();
  }
};

void CumulativeLogger::DumpHistogram(std::ostream &os) {
  os << "Start Dumping histograms for " << iterations_ << " iterations"
     << " for " << name_ << "\n";
  std::set<Histogram<uint64_t>*, CompareHistorgramByTimeSpentDeclining>
      sorted_histograms(histograms_.begin(), histograms_.end());
  for (Histogram<uint64_t>* histogram : sorted_histograms) {
    Histogram<uint64_t>::CumulativeData cumulative_data;
    // We don't expect DumpHistogram to be called often, so it is not performance critical.
    histogram->CreateHistogram(&cumulative_data);
    histogram->PrintConfidenceIntervals(os, 0.99, cumulative_data);
  }
  os << "Done Dumping histograms \n";
}

TimingLogger::TimingLogger(const char* name, bool precise, bool verbose)
    : name_(name), precise_(precise), verbose_(verbose), current_split_(NULL) {
}

void TimingLogger::Reset() {
  current_split_ = NULL;
  splits_.clear();
}

void TimingLogger::StartSplit(const char* new_split_label) {
  DCHECK(new_split_label != nullptr) << "Starting split with null label.";
  TimingLogger::ScopedSplit* explicit_scoped_split =
      new TimingLogger::ScopedSplit(new_split_label, this);
  explicit_scoped_split->explicit_ = true;
}

void TimingLogger::EndSplit() {
  CHECK(current_split_ != nullptr) << "Ending a non-existent split.";
  DCHECK(current_split_->label_ != nullptr);
  DCHECK(current_split_->explicit_ == true)
      << "Explicitly ending scoped split: " << current_split_->label_;
  delete current_split_;
  // TODO: current_split_ = nullptr;
}

// Ends the current split and starts the one given by the label.
void TimingLogger::NewSplit(const char* new_split_label) {
  if (current_split_ == nullptr) {
    StartSplit(new_split_label);
  } else {
    DCHECK(new_split_label != nullptr) << "New split (" << new_split_label << ") with null label.";
    current_split_->TailInsertSplit(new_split_label);
  }
}

uint64_t TimingLogger::GetTotalNs() const {
  uint64_t total_ns = 0;
  for (auto it = splits_.begin(), end = splits_.end(); it != end; ++it) {
    TimingLogger::SplitTiming split = *it;
    total_ns += split.first;
  }
  return total_ns;
}

void TimingLogger::Dump(std::ostream &os) const {
  uint64_t longest_split = 0;
  uint64_t total_ns = 0;
  for (auto it = splits_.begin(), end = splits_.end(); it != end; ++it) {
    TimingLogger::SplitTiming split = *it;
    uint64_t split_time = split.first;
    longest_split = std::max(longest_split, split_time);
    total_ns += split_time;
  }
  // Compute which type of unit we will use for printing the timings.
  TimeUnit tu = GetAppropriateTimeUnit(longest_split);
  uint64_t divisor = GetNsToTimeUnitDivisor(tu);
  // Print formatted splits.
  for (auto it = splits_.begin(), end = splits_.end(); it != end; ++it) {
    const TimingLogger::SplitTiming& split = *it;
    uint64_t split_time = split.first;
    if (!precise_ && divisor >= 1000) {
      // Make the fractional part 0.
      split_time -= split_time % (divisor / 1000);
    }
    os << name_ << ": " << std::setw(8) << FormatDuration(split_time, tu) << " "
       << split.second << "\n";
  }
  os << name_ << ": end, " << NsToMs(total_ns) << " ms\n";
}


TimingLogger::ScopedSplit::ScopedSplit(const char* label, TimingLogger* timing_logger) {
  DCHECK(label != NULL) << "New scoped split (" << label << ") with null label.";
  CHECK(timing_logger != NULL) << "New scoped split (" << label << ") without TimingLogger.";
  timing_logger_ = timing_logger;
  label_ = label;
  running_ns_ = 0;
  explicit_ = false;

  // Stash away the current split and pause it.
  enclosing_split_ = timing_logger->current_split_;
  if (enclosing_split_ != NULL) {
    enclosing_split_->Pause();
  }

  timing_logger_->current_split_ = this;

  ATRACE_BEGIN(label_);

  start_ns_ = NanoTime();
  if (timing_logger_->verbose_) {
    LOG(INFO) << "Begin: " << label_;
  }
}

TimingLogger::ScopedSplit::~ScopedSplit() {
  uint64_t current_time = NanoTime();
  uint64_t split_time = current_time - start_ns_;
  running_ns_ += split_time;
  ATRACE_END();

  if (timing_logger_->verbose_) {
    LOG(INFO) << "End: " << label_ << " " << PrettyDuration(split_time);
  }

  // If one or more enclosed explicitly started splits are not terminated we can
  // either fail or "unwind" the stack of splits in the timing logger to 'this'
  // (by deleting the intervening scoped splits). This implements the latter.
  TimingLogger::ScopedSplit* current = timing_logger_->current_split_;
  while ((current != NULL) && (current != this)) {
    delete current;
    current = timing_logger_->current_split_;
  }

  CHECK(current != NULL) << "Missing scoped split (" << this->label_
                           << ") in timing logger (" << timing_logger_->name_ << ").";
  CHECK(timing_logger_->current_split_ == this);

  timing_logger_->splits_.push_back(SplitTiming(running_ns_, label_));

  timing_logger_->current_split_ = enclosing_split_;
  if (enclosing_split_ != NULL) {
    enclosing_split_->Resume();
  }
}


void TimingLogger::ScopedSplit::TailInsertSplit(const char* label) {
  // Sleight of hand here: Rather than embedding a new scoped split, we're updating the current
  // scoped split in place. Basically, it's one way to make explicit and scoped splits compose
  // well while maintaining the current semantics of NewSplit. An alternative is to push a new split
  // since we unwind the stack of scoped splits in the scoped split destructor. However, this implies
  // that the current split is not ended by NewSplit (which calls TailInsertSplit), which would
  // be different from what we had before.

  uint64_t current_time = NanoTime();
  uint64_t split_time = current_time - start_ns_;
  ATRACE_END();
  timing_logger_->splits_.push_back(std::pair<uint64_t, const char*>(split_time, label_));

  if (timing_logger_->verbose_) {
    LOG(INFO) << "End: " << label_ << " " << PrettyDuration(split_time) << "\n"
              << "Begin: " << label;
  }

  label_ = label;
  start_ns_ = current_time;
  running_ns_ = 0;

  ATRACE_BEGIN(label);
}

void TimingLogger::ScopedSplit::Pause() {
  uint64_t current_time = NanoTime();
  uint64_t split_time = current_time - start_ns_;
  running_ns_ += split_time;
  ATRACE_END();
}


void TimingLogger::ScopedSplit::Resume() {
  uint64_t current_time = NanoTime();

  start_ns_ = current_time;
  ATRACE_BEGIN(label_);
}

}  // namespace art
