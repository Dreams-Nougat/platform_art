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

#include "profile_assistant.h"

namespace art {

static constexpr const uint32_t kMethodsThreshold = 10;

ProfileCompilationInfo* ProfileAssistant::ProcessProfiles(
      const std::vector<std::string>& profile_files,
      const std::vector<std::string>& reference_profile_files) {
  DCHECK(!profile_files.empty());
  DCHECK(!reference_profile_files.empty() ||
      (profile_files.size() == reference_profile_files.size()));

  std::vector<ProfileCompilationInfo> new_info;

  bool should_compile = false;
  for (size_t i = 0; i < profile_files.size(); i++) {
    if (!new_info[i].Load(profile_files[i])) {
      return nullptr;
    }
    should_compile |= (new_info[i].GetNumberOfMethods() > kMethodsThreshold);
  }
  if (!should_compile) {
    return nullptr;
  }
  std::unique_ptr<ProfileCompilationInfo> result(new ProfileCompilationInfo());

  for (size_t i = 0; i < new_info.size(); i++) {
    if (!new_info[i].Load(reference_profile_files[i])) {
      return nullptr;
    }
    if (!new_info[i].Save(reference_profile_files[i])) {
      return nullptr;
    }
    result->Load(new_info[i]);
  }
  return result.release();
}

}  // namespace art
