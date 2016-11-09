/*
 * Copyright (C) 2013 The Android Open Source Project
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

#include "get_loaded_classes.h"

#include <iostream>
#include <pthread.h>
#include <stdio.h>
#include <vector>

#include "base/macros.h"
#include "jni.h"
#include "openjdkjvmti/jvmti.h"
#include "ScopedLocalRef.h"
#include "ScopedUtfChars.h"

#include "ti-agent/common_helper.h"
#include "ti-agent/common_load.h"

namespace art {
namespace Test907GetLoadedClasses {

static jstring GetClassName(JNIEnv* jni_env, jclass cls) {
  ScopedLocalRef<jclass> class_class(jni_env, jni_env->GetObjectClass(cls));
  jmethodID mid = jni_env->GetMethodID(class_class.get(), "getName", "()Ljava/lang/String;");
  return reinterpret_cast<jstring>(jni_env->CallObjectMethod(cls, mid));
}

extern "C" JNIEXPORT jobjectArray JNICALL Java_Main_getLoadedClasses(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED) {
  jint count = -1;
  jclass* classes = nullptr;
  jvmtiError result = jvmti_env->GetLoadedClasses(&count, &classes);
  if (result != JVMTI_ERROR_NONE) {
    char* err;
    jvmti_env->GetErrorName(result, &err);
    printf("Failure running GetLoadedClasses: %s\n", err);
    return nullptr;
  }

  auto callback = [&](jint i) {
    jstring class_name = GetClassName(env, classes[i]);
    env->DeleteLocalRef(classes[i]);
    return class_name;
  };
  jobjectArray ret = CreateObjectArray(env, count, "java/lang/String", callback);

  // Need to Deallocate.
  jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(classes));

  return ret;
}

// Don't do anything
jint OnLoad(JavaVM* vm,
            char* options ATTRIBUTE_UNUSED,
            void* reserved ATTRIBUTE_UNUSED) {
  if (vm->GetEnv(reinterpret_cast<void**>(&jvmti_env), JVMTI_VERSION_1_0)) {
    printf("Unable to get jvmti env!\n");
    return 1;
  }
  return 0;
}

}  // namespace Test907GetLoadedClasses
}  // namespace art
