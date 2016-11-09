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

#ifndef ART_TEST_TI_AGENT_COMMON_HELPER_H_
#define ART_TEST_TI_AGENT_COMMON_HELPER_H_

#include "jni.h"
#include "ScopedLocalRef.h"

namespace art {

template <typename T>
static jobjectArray CreateObjectArray(JNIEnv* env,
                                      jint length,
                                      const char* component_type_descriptor,
                                      T src) {
  if (length < 0) {
    return nullptr;
  }

  ScopedLocalRef<jclass> obj_class(env, env->FindClass(component_type_descriptor));
  if (obj_class.get() == nullptr) {
    return nullptr;
  }

  jobjectArray ret = env->NewObjectArray(length, obj_class.get(), nullptr);
  if (ret == nullptr) {
    return ret;
  }

  for (jint i = 0; i < length; ++i) {
    jobject element = src(i);
    env->SetObjectArrayElement(ret, static_cast<jint>(i), element);
    env->DeleteLocalRef(element);
    if (env->ExceptionCheck()) {
      return nullptr;
    }
  }

  return ret;
}

}  // namespace art

#endif  // ART_TEST_TI_AGENT_COMMON_HELPER_H_
