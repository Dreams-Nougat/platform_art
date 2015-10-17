/*
 * Copyright (C) 2008 The Android Open Source Project
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

#include "dalvik_system_DexFile.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "class_linker.h"
#include "common_throws.h"
#include "dex_file-inl.h"
#include "jni_internal.h"
#include "mirror/class_loader.h"
#include "mirror/object-inl.h"
#include "mirror/string.h"
#include "oat_file_assistant.h"
#include "oat_file_manager.h"
#include "os.h"
#include "profiler.h"
#include "runtime.h"
#include "scoped_thread_state_change.h"
#include "ScopedLocalRef.h"
#include "ScopedUtfChars.h"
#include "utils.h"
#include "well_known_classes.h"
#include "zip_archive.h"

namespace art {

static bool ConvertJavaArrayToDexFiles(
    JNIEnv* env,
    jobject arrayObject,
    /*out*/ std::vector<const DexFile*>& dex_files,
    /*out*/ const OatFile*& oat_file) {
  jarray array = reinterpret_cast<jarray>(arrayObject);

  jsize array_size = env->GetArrayLength(array);
  if (env->ExceptionCheck() == JNI_TRUE) {
    return false;
  }

  // TODO: Optimize. On 32bit we can use an int array.
  jboolean is_long_data_copied;
  jlong* long_data = env->GetLongArrayElements(reinterpret_cast<jlongArray>(array),
                                               &is_long_data_copied);
  if (env->ExceptionCheck() == JNI_TRUE) {
    return false;
  }

  oat_file = reinterpret_cast<const OatFile*>(static_cast<uintptr_t>(long_data[kOatFileIndex]));
  dex_files.reserve(array_size - 1);
  for (jsize i = kDexFileIndexStart; i < array_size; ++i) {
    dex_files.push_back(reinterpret_cast<const DexFile*>(static_cast<uintptr_t>(long_data[i])));
  }

  env->ReleaseLongArrayElements(reinterpret_cast<jlongArray>(array), long_data, JNI_ABORT);
  return env->ExceptionCheck() != JNI_TRUE;
}

static jlongArray ConvertDexFilesToJavaArray(JNIEnv* env,
                                             const OatFile* oat_file,
                                             std::vector<std::unique_ptr<const DexFile>>& vec) {
  // Add one for the oat file.
  jlongArray long_array = env->NewLongArray(static_cast<jsize>(kDexFileIndexStart + vec.size()));
  if (env->ExceptionCheck() == JNI_TRUE) {
    return nullptr;
  }

  jboolean is_long_data_copied;
  jlong* long_data = env->GetLongArrayElements(long_array, &is_long_data_copied);
  if (env->ExceptionCheck() == JNI_TRUE) {
    return nullptr;
  }

  long_data[kOatFileIndex] = reinterpret_cast<uintptr_t>(oat_file);
  for (size_t i = 0; i < vec.size(); ++i) {
    long_data[kDexFileIndexStart + i] = reinterpret_cast<uintptr_t>(vec[i].get());
  }

  env->ReleaseLongArrayElements(long_array, long_data, 0);
  if (env->ExceptionCheck() == JNI_TRUE) {
    return nullptr;
  }

  // Now release all the unique_ptrs.
  for (auto& dex_file : vec) {
    dex_file.release();
  }

  return long_array;
}

// A smart pointer that provides read-only access to a Java string's UTF chars.
// Unlike libcore's NullableScopedUtfChars, this will *not* throw NullPointerException if
// passed a null jstring. The correct idiom is:
//
//   NullableScopedUtfChars name(env, javaName);
//   if (env->ExceptionCheck()) {
//       return null;
//   }
//   // ... use name.c_str()
//
// TODO: rewrite to get rid of this, or change ScopedUtfChars to offer this option.
class NullableScopedUtfChars {
 public:
  NullableScopedUtfChars(JNIEnv* env, jstring s) : mEnv(env), mString(s) {
    mUtfChars = (s != nullptr) ? env->GetStringUTFChars(s, nullptr) : nullptr;
  }

  ~NullableScopedUtfChars() {
    if (mUtfChars) {
      mEnv->ReleaseStringUTFChars(mString, mUtfChars);
    }
  }

  const char* c_str() const {
    return mUtfChars;
  }

  size_t size() const {
    return strlen(mUtfChars);
  }

  // Element access.
  const char& operator[](size_t n) const {
    return mUtfChars[n];
  }

 private:
  JNIEnv* mEnv;
  jstring mString;
  const char* mUtfChars;

  // Disallow copy and assignment.
  NullableScopedUtfChars(const NullableScopedUtfChars&);
  void operator=(const NullableScopedUtfChars&);
};

static jobject DexFile_openDexFileNative(
    JNIEnv* env, jclass, jstring javaSourceName, jstring javaOutputName, jint) {
  ScopedUtfChars sourceName(env, javaSourceName);
  if (sourceName.c_str() == nullptr) {
    return 0;
  }
  NullableScopedUtfChars outputName(env, javaOutputName);
  if (env->ExceptionCheck()) {
    return 0;
  }

  Runtime* const runtime = Runtime::Current();
  ClassLinker* linker = runtime->GetClassLinker();
  std::vector<std::unique_ptr<const DexFile>> dex_files;
  std::vector<std::string> error_msgs;
  const OatFile* oat_file = nullptr;

  dex_files = runtime->GetOatFileManager().OpenDexFilesFromOat(sourceName.c_str(),
                                                               outputName.c_str(),
                                                               /*out*/ &oat_file,
                                                               /*out*/ &error_msgs);

  if (!dex_files.empty()) {
    jlongArray array = ConvertDexFilesToJavaArray(env, oat_file, dex_files);
    if (array == nullptr) {
      ScopedObjectAccess soa(env);
      for (auto& dex_file : dex_files) {
        if (linker->FindDexCache(soa.Self(), *dex_file, true) != nullptr) {
          dex_file.release();
        }
      }
    }
    return array;
  } else {
    ScopedObjectAccess soa(env);
    CHECK(!error_msgs.empty());
    // The most important message is at the end. So set up nesting by going forward, which will
    // wrap the existing exception as a cause for the following one.
    auto it = error_msgs.begin();
    auto itEnd = error_msgs.end();
    for ( ; it != itEnd; ++it) {
      ThrowWrappedIOException("%s", it->c_str());
    }

    return nullptr;
  }
}

static jboolean DexFile_closeDexFile(JNIEnv* env, jclass, jobject cookie) {
  std::vector<const DexFile*> dex_files;
  const OatFile* oat_file;
  if (!ConvertJavaArrayToDexFiles(env, cookie, dex_files, oat_file)) {
    Thread::Current()->AssertPendingException();
    return JNI_FALSE;
  }
  Runtime* const runtime = Runtime::Current();
  bool all_deleted = true;
  {
    ScopedObjectAccess soa(env);
    mirror::Object* dex_files_object = soa.Decode<mirror::Object*>(cookie);
    mirror::LongArray* long_dex_files = dex_files_object->AsLongArray();
    // Delete dex files associated with this dalvik.system.DexFile since there should not be running
    // code using it. dex_files is a vector due to multidex.
    ClassLinker* const class_linker = runtime->GetClassLinker();
    int32_t i = kDexFileIndexStart;  // Oat file is at index 0.
    for (const DexFile* dex_file : dex_files) {
      if (dex_file != nullptr) {
        // Only delete the dex file if the dex cache is not found to prevent runtime crashes if there
        // are calls to DexFile.close while the ART DexFile is still in use.
        if (class_linker->FindDexCache(soa.Self(), *dex_file, true) == nullptr) {
          // Clear the element in the array so that we can call close again.
          long_dex_files->Set(i, 0);
          delete dex_file;
        } else {
          all_deleted = false;
        }
      }
      ++i;
    }
  }

  // oat_file can be null if we are running without dex2oat.
  if (all_deleted && oat_file != nullptr) {
    // If all of the dex files are no longer in use we can unmap the corresponding oat file.
    VLOG(class_linker) << "Unregistering " << oat_file;
    runtime->GetOatFileManager().UnRegisterAndDeleteOatFile(oat_file);
  }
  return all_deleted ? JNI_TRUE : JNI_FALSE;
}

static jclass DexFile_defineClassNative(JNIEnv* env,
                                        jclass,
                                        jstring javaName,
                                        jobject javaLoader,
                                        jobject cookie,
                                        jobject dexFile) {
  std::vector<const DexFile*> dex_files;
  const OatFile* oat_file;
  if (!ConvertJavaArrayToDexFiles(env, cookie, /*out*/ dex_files, /*out*/ oat_file)) {
    VLOG(class_linker) << "Failed to find dex_file";
    DCHECK(env->ExceptionCheck());
    return nullptr;
  }

  ScopedUtfChars class_name(env, javaName);
  if (class_name.c_str() == nullptr) {
    VLOG(class_linker) << "Failed to find class_name";
    return nullptr;
  }
  const std::string descriptor(DotToDescriptor(class_name.c_str()));
  const size_t hash(ComputeModifiedUtf8Hash(descriptor.c_str()));
  for (auto& dex_file : dex_files) {
    const DexFile::ClassDef* dex_class_def = dex_file->FindClassDef(descriptor.c_str(), hash);
    if (dex_class_def != nullptr) {
      ScopedObjectAccess soa(env);
      ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
      StackHandleScope<1> hs(soa.Self());
      Handle<mirror::ClassLoader> class_loader(
          hs.NewHandle(soa.Decode<mirror::ClassLoader*>(javaLoader)));
      class_linker->RegisterDexFile(
          *dex_file,
          class_linker->GetOrCreateAllocatorForClassLoader(class_loader.Get()));
      mirror::Class* result = class_linker->DefineClass(soa.Self(),
                                                        descriptor.c_str(),
                                                        hash,
                                                        class_loader,
                                                        *dex_file,
                                                        *dex_class_def);
      // Add the used dex file. This only required for the DexFile.loadClass API since normal
      // class loaders already keep their dex files live.
      class_linker->InsertDexFileInToClassLoader(soa.Decode<mirror::Object*>(dexFile),
                                                 class_loader.Get());
      if (result != nullptr) {
        VLOG(class_linker) << "DexFile_defineClassNative returning " << result
                           << " for " << class_name.c_str();
        return soa.AddLocalReference<jclass>(result);
      }
    }
  }
  VLOG(class_linker) << "Failed to find dex_class_def " << class_name.c_str();
  return nullptr;
}

// Needed as a compare functor for sets of const char
struct CharPointerComparator {
  bool operator()(const char *str1, const char *str2) const {
    return strcmp(str1, str2) < 0;
  }
};

// Note: this can be an expensive call, as we sort out duplicates in MultiDex files.
static jobjectArray DexFile_getClassNameList(JNIEnv* env, jclass, jobject cookie) {
  const OatFile* oat_file = nullptr;
  std::vector<const DexFile*> dex_files;
  if (!ConvertJavaArrayToDexFiles(env, cookie, /*out */ dex_files, /* out */ oat_file)) {
    DCHECK(env->ExceptionCheck());
    return nullptr;
  }

  // Push all class descriptors into a set. Use set instead of unordered_set as we want to
  // retrieve all in the end.
  std::set<const char*, CharPointerComparator> descriptors;
  for (auto& dex_file : dex_files) {
    for (size_t i = 0; i < dex_file->NumClassDefs(); ++i) {
      const DexFile::ClassDef& class_def = dex_file->GetClassDef(i);
      const char* descriptor = dex_file->GetClassDescriptor(class_def);
      descriptors.insert(descriptor);
    }
  }

  // Now create output array and copy the set into it.
  jobjectArray result = env->NewObjectArray(descriptors.size(),
                                            WellKnownClasses::java_lang_String,
                                            nullptr);
  if (result != nullptr) {
    auto it = descriptors.begin();
    auto it_end = descriptors.end();
    jsize i = 0;
    for (; it != it_end; it++, ++i) {
      std::string descriptor(DescriptorToDot(*it));
      ScopedLocalRef<jstring> jdescriptor(env, env->NewStringUTF(descriptor.c_str()));
      if (jdescriptor.get() == nullptr) {
        return nullptr;
      }
      env->SetObjectArrayElement(result, i, jdescriptor.get());
    }
  }
  return result;
}

static jint GetDexOptNeeded(JNIEnv* env,
                            const char* filename,
                            const char* pkgname,
                            const char* instruction_set,
                            const jboolean defer) {
  if ((filename == nullptr) || !OS::FileExists(filename)) {
    LOG(ERROR) << "DexFile_getDexOptNeeded file '" << filename << "' does not exist";
    ScopedLocalRef<jclass> fnfe(env, env->FindClass("java/io/FileNotFoundException"));
    const char* message = (filename == nullptr) ? "<empty file name>" : filename;
    env->ThrowNew(fnfe.get(), message);
    return OatFileAssistant::kNoDexOptNeeded;
  }

  const InstructionSet target_instruction_set = GetInstructionSetFromString(instruction_set);
  if (target_instruction_set == kNone) {
    ScopedLocalRef<jclass> iae(env, env->FindClass("java/lang/IllegalArgumentException"));
    std::string message(StringPrintf("Instruction set %s is invalid.", instruction_set));
    env->ThrowNew(iae.get(), message.c_str());
    return 0;
  }

  // TODO: Verify the dex location is well formed, and throw an IOException if
  // not?

  OatFileAssistant oat_file_assistant(filename, target_instruction_set, false, pkgname);

  // Always treat elements of the bootclasspath as up-to-date.
  if (oat_file_assistant.IsInBootClassPath()) {
    return OatFileAssistant::kNoDexOptNeeded;
  }

  // TODO: Checking the profile should probably be done in the GetStatus()
  // function. We have it here because GetStatus() should not be copying
  // profile files. But who should be copying profile files?
  if (oat_file_assistant.OdexFileIsOutOfDate()) {
    // Needs recompile if profile has changed significantly.
    if (Runtime::Current()->GetProfilerOptions().IsEnabled()) {
      if (oat_file_assistant.IsProfileChangeSignificant()) {
        if (!defer) {
          oat_file_assistant.CopyProfileFile();
        }
        return OatFileAssistant::kDex2OatNeeded;
      } else if (oat_file_assistant.ProfileExists()
          && !oat_file_assistant.OldProfileExists()) {
        if (!defer) {
          oat_file_assistant.CopyProfileFile();
        }
      }
    }
  }

  return oat_file_assistant.GetDexOptNeeded();
}

static jint DexFile_getDexOptNeeded(JNIEnv* env,
                                    jclass,
                                    jstring javaFilename,
                                    jstring javaPkgname,
                                    jstring javaInstructionSet,
                                    jboolean defer) {
  ScopedUtfChars filename(env, javaFilename);
  if (env->ExceptionCheck()) {
    return 0;
  }

  NullableScopedUtfChars pkgname(env, javaPkgname);

  ScopedUtfChars instruction_set(env, javaInstructionSet);
  if (env->ExceptionCheck()) {
    return 0;
  }

  return GetDexOptNeeded(env,
                         filename.c_str(),
                         pkgname.c_str(),
                         instruction_set.c_str(),
                         defer);
}

// public API, null pkgname
static jboolean DexFile_isDexOptNeeded(JNIEnv* env, jclass, jstring javaFilename) {
  const char* instruction_set = GetInstructionSetString(kRuntimeISA);
  ScopedUtfChars filename(env, javaFilename);
  jint status = GetDexOptNeeded(env, filename.c_str(), nullptr /* pkgname */,
                                instruction_set, false /* defer */);
  return (status != OatFileAssistant::kNoDexOptNeeded) ? JNI_TRUE : JNI_FALSE;
}

static JNINativeMethod gMethods[] = {
  NATIVE_METHOD(DexFile, closeDexFile, "(Ljava/lang/Object;)Z"),
  NATIVE_METHOD(DexFile, defineClassNative,
                "(Ljava/lang/String;Ljava/lang/ClassLoader;Ljava/lang/Object;Ldalvik/system/DexFile;)Ljava/lang/Class;"),
  NATIVE_METHOD(DexFile, getClassNameList, "(Ljava/lang/Object;)[Ljava/lang/String;"),
  NATIVE_METHOD(DexFile, isDexOptNeeded, "(Ljava/lang/String;)Z"),
  NATIVE_METHOD(DexFile, getDexOptNeeded,
                "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Z)I"),
  NATIVE_METHOD(DexFile, openDexFileNative,
                "(Ljava/lang/String;Ljava/lang/String;I)Ljava/lang/Object;"),
};

void register_dalvik_system_DexFile(JNIEnv* env) {
  REGISTER_NATIVE_METHODS("dalvik/system/DexFile");
}

}  // namespace art
