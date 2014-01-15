/*
 * Copyright (C) 2012 The Android Open Source Project
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

#include "well_known_classes.h"

#include <stdlib.h>

#include "base/logging.h"
#include "mirror/class.h"
#include "ScopedLocalRef.h"
#include "thread-inl.h"

namespace art {

jclass WellKnownClasses::com_android_dex_Dex;
jclass WellKnownClasses::dalvik_system_PathClassLoader;
jclass WellKnownClasses::java_lang_ClassLoader;
jclass WellKnownClasses::java_lang_ClassNotFoundException;
jclass WellKnownClasses::java_lang_Daemons;
jclass WellKnownClasses::java_lang_Error;
jclass WellKnownClasses::java_lang_Object;
jclass WellKnownClasses::java_lang_reflect_AbstractMethod;
jclass WellKnownClasses::java_lang_reflect_ArtMethod;
jclass WellKnownClasses::java_lang_reflect_Constructor;
jclass WellKnownClasses::java_lang_reflect_Field;
jclass WellKnownClasses::java_lang_reflect_Method;
jclass WellKnownClasses::java_lang_reflect_Proxy;
jclass WellKnownClasses::java_lang_RuntimeException;
jclass WellKnownClasses::java_lang_StackOverflowError;
jclass WellKnownClasses::java_lang_StringFactory;
jclass WellKnownClasses::java_lang_System;
jclass WellKnownClasses::java_lang_Thread;
jclass WellKnownClasses::java_lang_Thread$UncaughtExceptionHandler;
jclass WellKnownClasses::java_lang_ThreadGroup;
jclass WellKnownClasses::java_lang_Throwable;
jclass WellKnownClasses::java_nio_DirectByteBuffer;
jclass WellKnownClasses::org_apache_harmony_dalvik_ddmc_Chunk;
jclass WellKnownClasses::org_apache_harmony_dalvik_ddmc_DdmServer;

jmethodID WellKnownClasses::com_android_dex_Dex_create;
jmethodID WellKnownClasses::java_lang_Boolean_valueOf;
jmethodID WellKnownClasses::java_lang_Byte_valueOf;
jmethodID WellKnownClasses::java_lang_Character_valueOf;
jmethodID WellKnownClasses::java_lang_ClassLoader_loadClass;
jmethodID WellKnownClasses::java_lang_ClassNotFoundException_init;
jmethodID WellKnownClasses::java_lang_Daemons_requestGC;
jmethodID WellKnownClasses::java_lang_Daemons_requestHeapTrim;
jmethodID WellKnownClasses::java_lang_Daemons_start;
jmethodID WellKnownClasses::java_lang_Double_valueOf;
jmethodID WellKnownClasses::java_lang_Float_valueOf;
jmethodID WellKnownClasses::java_lang_Integer_valueOf;
jmethodID WellKnownClasses::java_lang_Long_valueOf;
jmethodID WellKnownClasses::java_lang_ref_FinalizerReference_add;
jmethodID WellKnownClasses::java_lang_ref_ReferenceQueue_add;
jmethodID WellKnownClasses::java_lang_reflect_Proxy_invoke;
jmethodID WellKnownClasses::java_lang_Runtime_nativeLoad;
jmethodID WellKnownClasses::java_lang_Short_valueOf;
jmethodID WellKnownClasses::java_lang_System_runFinalization = NULL;
jmethodID WellKnownClasses::java_lang_Thread_init;
jmethodID WellKnownClasses::java_lang_Thread_run;
jmethodID WellKnownClasses::java_lang_Thread$UncaughtExceptionHandler_uncaughtException;
jmethodID WellKnownClasses::java_lang_ThreadGroup_removeThread;
jmethodID WellKnownClasses::java_nio_DirectByteBuffer_init;
jmethodID WellKnownClasses::org_apache_harmony_dalvik_ddmc_DdmServer_broadcast;
jmethodID WellKnownClasses::org_apache_harmony_dalvik_ddmc_DdmServer_dispatch;

jmethodID WellKnownClasses::java_lang_String_init;
jmethodID WellKnownClasses::java_lang_String_init_B;
jmethodID WellKnownClasses::java_lang_String_init_BI;
jmethodID WellKnownClasses::java_lang_String_init_BII;
jmethodID WellKnownClasses::java_lang_String_init_BIII;
jmethodID WellKnownClasses::java_lang_String_init_BIIString;
jmethodID WellKnownClasses::java_lang_String_init_BString;
jmethodID WellKnownClasses::java_lang_String_init_BIICharset;
jmethodID WellKnownClasses::java_lang_String_init_BCharset;
jmethodID WellKnownClasses::java_lang_String_init_C;
jmethodID WellKnownClasses::java_lang_String_init_CII;
jmethodID WellKnownClasses::java_lang_String_init_IIC;
jmethodID WellKnownClasses::java_lang_String_init_String;
jmethodID WellKnownClasses::java_lang_String_init_StringBuffer;
jmethodID WellKnownClasses::java_lang_String_init_III;
jmethodID WellKnownClasses::java_lang_String_init_StringBuilder;
jmethodID WellKnownClasses::java_lang_StringFactory_newEmptyString;
jmethodID WellKnownClasses::java_lang_StringFactory_newStringFromBytes_B;
jmethodID WellKnownClasses::java_lang_StringFactory_newStringFromBytes_BI;
jmethodID WellKnownClasses::java_lang_StringFactory_newStringFromBytes_BII;
jmethodID WellKnownClasses::java_lang_StringFactory_newStringFromBytes_BIII;
jmethodID WellKnownClasses::java_lang_StringFactory_newStringFromBytes_BIIString;
jmethodID WellKnownClasses::java_lang_StringFactory_newStringFromBytes_BString;
jmethodID WellKnownClasses::java_lang_StringFactory_newStringFromBytes_BIICharset;
jmethodID WellKnownClasses::java_lang_StringFactory_newStringFromBytes_BCharset;
jmethodID WellKnownClasses::java_lang_StringFactory_newStringFromChars_C;
jmethodID WellKnownClasses::java_lang_StringFactory_newStringFromChars_CII;
jmethodID WellKnownClasses::java_lang_StringFactory_newStringFromCharsNoCheck;
jmethodID WellKnownClasses::java_lang_StringFactory_newStringFromString;
jmethodID WellKnownClasses::java_lang_StringFactory_newStringFromStringBuffer;
jmethodID WellKnownClasses::java_lang_StringFactory_newStringFromCodePoints;
jmethodID WellKnownClasses::java_lang_StringFactory_newStringFromStringBuilder;

jfieldID WellKnownClasses::java_lang_Thread_daemon;
jfieldID WellKnownClasses::java_lang_Thread_group;
jfieldID WellKnownClasses::java_lang_Thread_lock;
jfieldID WellKnownClasses::java_lang_Thread_name;
jfieldID WellKnownClasses::java_lang_Thread_priority;
jfieldID WellKnownClasses::java_lang_Thread_uncaughtHandler;
jfieldID WellKnownClasses::java_lang_Thread_nativePeer;
jfieldID WellKnownClasses::java_lang_ThreadGroup_mainThreadGroup;
jfieldID WellKnownClasses::java_lang_ThreadGroup_name;
jfieldID WellKnownClasses::java_lang_ThreadGroup_systemThreadGroup;
jfieldID WellKnownClasses::java_lang_reflect_AbstractMethod_artMethod;
jfieldID WellKnownClasses::java_lang_reflect_Field_artField;
jfieldID WellKnownClasses::java_lang_reflect_Proxy_h;
jfieldID WellKnownClasses::java_nio_DirectByteBuffer_capacity;
jfieldID WellKnownClasses::java_nio_DirectByteBuffer_effectiveDirectAddress;
jfieldID WellKnownClasses::org_apache_harmony_dalvik_ddmc_Chunk_data;
jfieldID WellKnownClasses::org_apache_harmony_dalvik_ddmc_Chunk_length;
jfieldID WellKnownClasses::org_apache_harmony_dalvik_ddmc_Chunk_offset;
jfieldID WellKnownClasses::org_apache_harmony_dalvik_ddmc_Chunk_type;

static jclass CacheClass(JNIEnv* env, const char* jni_class_name) {
  ScopedLocalRef<jclass> c(env, env->FindClass(jni_class_name));
  if (c.get() == NULL) {
    LOG(FATAL) << "Couldn't find class: " << jni_class_name;
  }
  return reinterpret_cast<jclass>(env->NewGlobalRef(c.get()));
}

static jfieldID CacheField(JNIEnv* env, jclass c, bool is_static, const char* name, const char* signature) {
  jfieldID fid = is_static ? env->GetStaticFieldID(c, name, signature) : env->GetFieldID(c, name, signature);
  if (fid == NULL) {
    LOG(FATAL) << "Couldn't find field \"" << name << "\" with signature \"" << signature << "\"";
  }
  return fid;
}

jmethodID CacheMethod(JNIEnv* env, jclass c, bool is_static, const char* name, const char* signature) {
  jmethodID mid = is_static ? env->GetStaticMethodID(c, name, signature) : env->GetMethodID(c, name, signature);
  if (mid == NULL) {
    LOG(FATAL) << "Couldn't find method \"" << name << "\" with signature \"" << signature << "\"";
  }
  return mid;
}

static jmethodID CachePrimitiveBoxingMethod(JNIEnv* env, char prim_name, const char* boxed_name) {
  ScopedLocalRef<jclass> boxed_class(env, env->FindClass(boxed_name));
  return CacheMethod(env, boxed_class.get(), true, "valueOf",
                     StringPrintf("(%c)L%s;", prim_name, boxed_name).c_str());
}

void WellKnownClasses::Init(JNIEnv* env) {
  com_android_dex_Dex = CacheClass(env, "com/android/dex/Dex");
  dalvik_system_PathClassLoader = CacheClass(env, "dalvik/system/PathClassLoader");
  java_lang_ClassLoader = CacheClass(env, "java/lang/ClassLoader");
  java_lang_ClassNotFoundException = CacheClass(env, "java/lang/ClassNotFoundException");
  java_lang_Daemons = CacheClass(env, "java/lang/Daemons");
  java_lang_Object = CacheClass(env, "java/lang/Object");
  java_lang_Error = CacheClass(env, "java/lang/Error");
  java_lang_reflect_AbstractMethod = CacheClass(env, "java/lang/reflect/AbstractMethod");
  java_lang_reflect_ArtMethod = CacheClass(env, "java/lang/reflect/ArtMethod");
  java_lang_reflect_Constructor = CacheClass(env, "java/lang/reflect/Constructor");
  java_lang_reflect_Field = CacheClass(env, "java/lang/reflect/Field");
  java_lang_reflect_Method = CacheClass(env, "java/lang/reflect/Method");
  java_lang_reflect_Proxy = CacheClass(env, "java/lang/reflect/Proxy");
  java_lang_RuntimeException = CacheClass(env, "java/lang/RuntimeException");
  java_lang_StackOverflowError = CacheClass(env, "java/lang/StackOverflowError");
  java_lang_StringFactory = CacheClass(env, "java/lang/StringFactory");
  java_lang_System = CacheClass(env, "java/lang/System");
  java_lang_Thread = CacheClass(env, "java/lang/Thread");
  java_lang_Thread$UncaughtExceptionHandler = CacheClass(env, "java/lang/Thread$UncaughtExceptionHandler");
  java_lang_ThreadGroup = CacheClass(env, "java/lang/ThreadGroup");
  java_lang_Throwable = CacheClass(env, "java/lang/Throwable");
  java_nio_DirectByteBuffer = CacheClass(env, "java/nio/DirectByteBuffer");
  org_apache_harmony_dalvik_ddmc_Chunk = CacheClass(env, "org/apache/harmony/dalvik/ddmc/Chunk");
  org_apache_harmony_dalvik_ddmc_DdmServer = CacheClass(env, "org/apache/harmony/dalvik/ddmc/DdmServer");

  com_android_dex_Dex_create = CacheMethod(env, com_android_dex_Dex, true, "create", "(Ljava/nio/ByteBuffer;)Lcom/android/dex/Dex;");
  java_lang_ClassNotFoundException_init = CacheMethod(env, java_lang_ClassNotFoundException, false, "<init>", "(Ljava/lang/String;Ljava/lang/Throwable;)V");
  java_lang_ClassLoader_loadClass = CacheMethod(env, java_lang_ClassLoader, false, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");

  java_lang_Daemons_requestGC = CacheMethod(env, java_lang_Daemons, true, "requestGC", "()V");
  java_lang_Daemons_requestHeapTrim = CacheMethod(env, java_lang_Daemons, true, "requestHeapTrim", "()V");
  java_lang_Daemons_start = CacheMethod(env, java_lang_Daemons, true, "start", "()V");

  ScopedLocalRef<jclass> java_lang_ref_FinalizerReference(env, env->FindClass("java/lang/ref/FinalizerReference"));
  java_lang_ref_FinalizerReference_add = CacheMethod(env, java_lang_ref_FinalizerReference.get(), true, "add", "(Ljava/lang/Object;)V");
  ScopedLocalRef<jclass> java_lang_ref_ReferenceQueue(env, env->FindClass("java/lang/ref/ReferenceQueue"));
  java_lang_ref_ReferenceQueue_add = CacheMethod(env, java_lang_ref_ReferenceQueue.get(), true, "add", "(Ljava/lang/ref/Reference;)V");

  java_lang_reflect_Proxy_invoke = CacheMethod(env, java_lang_reflect_Proxy, true, "invoke", "(Ljava/lang/reflect/Proxy;Ljava/lang/reflect/ArtMethod;[Ljava/lang/Object;)Ljava/lang/Object;");
  java_lang_Thread_init = CacheMethod(env, java_lang_Thread, false, "<init>", "(Ljava/lang/ThreadGroup;Ljava/lang/String;IZ)V");
  java_lang_Thread_run = CacheMethod(env, java_lang_Thread, false, "run", "()V");
  java_lang_Thread$UncaughtExceptionHandler_uncaughtException = CacheMethod(env, java_lang_Thread$UncaughtExceptionHandler, false, "uncaughtException", "(Ljava/lang/Thread;Ljava/lang/Throwable;)V");
  java_lang_ThreadGroup_removeThread = CacheMethod(env, java_lang_ThreadGroup, false, "removeThread", "(Ljava/lang/Thread;)V");
  java_nio_DirectByteBuffer_init = CacheMethod(env, java_nio_DirectByteBuffer, false, "<init>", "(JI)V");
  org_apache_harmony_dalvik_ddmc_DdmServer_broadcast = CacheMethod(env, org_apache_harmony_dalvik_ddmc_DdmServer, true, "broadcast", "(I)V");
  org_apache_harmony_dalvik_ddmc_DdmServer_dispatch = CacheMethod(env, org_apache_harmony_dalvik_ddmc_DdmServer, true, "dispatch", "(I[BII)Lorg/apache/harmony/dalvik/ddmc/Chunk;");

  ScopedLocalRef<jclass> java_lang_String(env, env->FindClass("java/lang/String"));
  java_lang_String_init = CacheMethod(env, java_lang_String.get(), false, "<init>", "()V");
  java_lang_String_init_B = CacheMethod(env, java_lang_String.get(), false, "<init>", "([B)V");
  java_lang_String_init_BI = CacheMethod(env, java_lang_String.get(), false, "<init>", "([BI)V");
  java_lang_String_init_BII = CacheMethod(env, java_lang_String.get(), false, "<init>", "([BII)V");
  java_lang_String_init_BIII = CacheMethod(env, java_lang_String.get(), false, "<init>", "([BIII)V");
  java_lang_String_init_BIIString = CacheMethod(env, java_lang_String.get(), false, "<init>", "([BIILjava/lang/String;)V");
  java_lang_String_init_BString = CacheMethod(env, java_lang_String.get(), false, "<init>", "([BLjava/lang/String;)V");
  java_lang_String_init_BIICharset = CacheMethod(env, java_lang_String.get(), false, "<init>", "([BIILjava/nio/charset/Charset;)V");
  java_lang_String_init_BCharset = CacheMethod(env, java_lang_String.get(), false, "<init>", "([BLjava/nio/charset/Charset;)V");
  java_lang_String_init_C = CacheMethod(env, java_lang_String.get(), false, "<init>", "([C)V");
  java_lang_String_init_CII = CacheMethod(env, java_lang_String.get(), false, "<init>", "([CII)V");
  java_lang_String_init_IIC = CacheMethod(env, java_lang_String.get(), false, "<init>", "(II[C)V");
  java_lang_String_init_String = CacheMethod(env, java_lang_String.get(), false, "<init>", "(Ljava/lang/String;)V");
  java_lang_String_init_StringBuffer = CacheMethod(env, java_lang_String.get(), false, "<init>", "(Ljava/lang/StringBuffer;)V");
  java_lang_String_init_III = CacheMethod(env, java_lang_String.get(), false, "<init>", "([III)V");
  java_lang_String_init_StringBuilder = CacheMethod(env, java_lang_String.get(), false, "<init>", "(Ljava/lang/StringBuilder;)V");
  java_lang_StringFactory_newEmptyString = CacheMethod(env, java_lang_StringFactory, true, "newEmptyString", "()Ljava/lang/String;");
  java_lang_StringFactory_newStringFromBytes_B = CacheMethod(env, java_lang_StringFactory, true, "newStringFromBytes", "([B)Ljava/lang/String;");
  java_lang_StringFactory_newStringFromBytes_BI = CacheMethod(env, java_lang_StringFactory, true, "newStringFromBytes", "([BI)Ljava/lang/String;");
  java_lang_StringFactory_newStringFromBytes_BII = CacheMethod(env, java_lang_StringFactory, true, "newStringFromBytes", "([BII)Ljava/lang/String;");
  java_lang_StringFactory_newStringFromBytes_BIII = CacheMethod(env, java_lang_StringFactory, true, "newStringFromBytes", "([BIII)Ljava/lang/String;");
  java_lang_StringFactory_newStringFromBytes_BIIString = CacheMethod(env, java_lang_StringFactory, true, "newStringFromBytes", "([BIILjava/lang/String;)Ljava/lang/String;");
  java_lang_StringFactory_newStringFromBytes_BString = CacheMethod(env, java_lang_StringFactory, true, "newStringFromBytes", "([BLjava/lang/String;)Ljava/lang/String;");
  java_lang_StringFactory_newStringFromBytes_BIICharset = CacheMethod(env, java_lang_StringFactory, true, "newStringFromBytes", "([BIILjava/nio/charset/Charset;)Ljava/lang/String;");
  java_lang_StringFactory_newStringFromBytes_BCharset = CacheMethod(env, java_lang_StringFactory, true, "newStringFromBytes", "([BLjava/nio/charset/Charset;)Ljava/lang/String;");
  java_lang_StringFactory_newStringFromChars_C = CacheMethod(env, java_lang_StringFactory, true, "newStringFromChars", "([C)Ljava/lang/String;");
  java_lang_StringFactory_newStringFromChars_CII = CacheMethod(env, java_lang_StringFactory, true, "newStringFromChars", "([CII)Ljava/lang/String;");
  java_lang_StringFactory_newStringFromCharsNoCheck = CacheMethod(env, java_lang_StringFactory, true, "newStringFromCharsNoCheck", "(II[C)Ljava/lang/String;");
  java_lang_StringFactory_newStringFromString = CacheMethod(env, java_lang_StringFactory, true, "newStringFromString", "(Ljava/lang/String;)Ljava/lang/String;");
  java_lang_StringFactory_newStringFromStringBuffer = CacheMethod(env, java_lang_StringFactory, true, "newStringFromStringBuffer", "(Ljava/lang/StringBuffer;)Ljava/lang/String;");
  java_lang_StringFactory_newStringFromCodePoints = CacheMethod(env, java_lang_StringFactory, true, "newStringFromCodePoints", "([III)Ljava/lang/String;");
  java_lang_StringFactory_newStringFromStringBuilder = CacheMethod(env, java_lang_StringFactory, true, "newStringFromStringBuilder", "(Ljava/lang/StringBuilder;)Ljava/lang/String;");

  java_lang_Thread_daemon = CacheField(env, java_lang_Thread, false, "daemon", "Z");
  java_lang_Thread_group = CacheField(env, java_lang_Thread, false, "group", "Ljava/lang/ThreadGroup;");
  java_lang_Thread_lock = CacheField(env, java_lang_Thread, false, "lock", "Ljava/lang/Object;");
  java_lang_Thread_name = CacheField(env, java_lang_Thread, false, "name", "Ljava/lang/String;");
  java_lang_Thread_priority = CacheField(env, java_lang_Thread, false, "priority", "I");
  java_lang_Thread_uncaughtHandler = CacheField(env, java_lang_Thread, false, "uncaughtHandler", "Ljava/lang/Thread$UncaughtExceptionHandler;");
  java_lang_Thread_nativePeer = CacheField(env, java_lang_Thread, false, "nativePeer", "J");
  java_lang_ThreadGroup_mainThreadGroup = CacheField(env, java_lang_ThreadGroup, true, "mainThreadGroup", "Ljava/lang/ThreadGroup;");
  java_lang_ThreadGroup_name = CacheField(env, java_lang_ThreadGroup, false, "name", "Ljava/lang/String;");
  java_lang_ThreadGroup_systemThreadGroup = CacheField(env, java_lang_ThreadGroup, true, "systemThreadGroup", "Ljava/lang/ThreadGroup;");
  java_lang_reflect_AbstractMethod_artMethod = CacheField(env, java_lang_reflect_AbstractMethod, false, "artMethod", "Ljava/lang/reflect/ArtMethod;");
  java_lang_reflect_Field_artField = CacheField(env, java_lang_reflect_Field, false, "artField", "Ljava/lang/reflect/ArtField;");
  java_lang_reflect_Proxy_h = CacheField(env, java_lang_reflect_Proxy, false, "h", "Ljava/lang/reflect/InvocationHandler;");
  java_nio_DirectByteBuffer_capacity = CacheField(env, java_nio_DirectByteBuffer, false, "capacity", "I");
  java_nio_DirectByteBuffer_effectiveDirectAddress = CacheField(env, java_nio_DirectByteBuffer, false, "effectiveDirectAddress", "J");
  org_apache_harmony_dalvik_ddmc_Chunk_data = CacheField(env, org_apache_harmony_dalvik_ddmc_Chunk, false, "data", "[B");
  org_apache_harmony_dalvik_ddmc_Chunk_length = CacheField(env, org_apache_harmony_dalvik_ddmc_Chunk, false, "length", "I");
  org_apache_harmony_dalvik_ddmc_Chunk_offset = CacheField(env, org_apache_harmony_dalvik_ddmc_Chunk, false, "offset", "I");
  org_apache_harmony_dalvik_ddmc_Chunk_type = CacheField(env, org_apache_harmony_dalvik_ddmc_Chunk, false, "type", "I");

  java_lang_Boolean_valueOf = CachePrimitiveBoxingMethod(env, 'Z', "java/lang/Boolean");
  java_lang_Byte_valueOf = CachePrimitiveBoxingMethod(env, 'B', "java/lang/Byte");
  java_lang_Character_valueOf = CachePrimitiveBoxingMethod(env, 'C', "java/lang/Character");
  java_lang_Double_valueOf = CachePrimitiveBoxingMethod(env, 'D', "java/lang/Double");
  java_lang_Float_valueOf = CachePrimitiveBoxingMethod(env, 'F', "java/lang/Float");
  java_lang_Integer_valueOf = CachePrimitiveBoxingMethod(env, 'I', "java/lang/Integer");
  java_lang_Long_valueOf = CachePrimitiveBoxingMethod(env, 'J', "java/lang/Long");
  java_lang_Short_valueOf = CachePrimitiveBoxingMethod(env, 'S', "java/lang/Short");
}

void WellKnownClasses::LateInit(JNIEnv* env) {
  ScopedLocalRef<jclass> java_lang_Runtime(env, env->FindClass("java/lang/Runtime"));
  java_lang_Runtime_nativeLoad = CacheMethod(env, java_lang_Runtime.get(), true, "nativeLoad", "(Ljava/lang/String;Ljava/lang/ClassLoader;Ljava/lang/String;)Ljava/lang/String;");
}

mirror::Class* WellKnownClasses::ToClass(jclass global_jclass) {
  return reinterpret_cast<mirror::Class*>(Thread::Current()->DecodeJObject(global_jclass));
}

jmethodID WellKnownClasses::StringInitToStringFactoryMethodID(jmethodID string_init) {
  // TODO: Prioritize ordering.
  if (string_init == java_lang_String_init) {
    return java_lang_StringFactory_newEmptyString;
  } else if (string_init == java_lang_String_init_B) {
    return java_lang_StringFactory_newStringFromBytes_B;
  } else if (string_init == java_lang_String_init_BI) {
    return java_lang_StringFactory_newStringFromBytes_BI;
  } else if (string_init == java_lang_String_init_BII) {
    return java_lang_StringFactory_newStringFromBytes_BII;
  } else if (string_init == java_lang_String_init_BIII) {
    return java_lang_StringFactory_newStringFromBytes_BIII;
  } else if (string_init == java_lang_String_init_BIIString) {
    return java_lang_StringFactory_newStringFromBytes_BIIString;
  } else if (string_init == java_lang_String_init_BString) {
    return java_lang_StringFactory_newStringFromBytes_BString;
  } else if (string_init == java_lang_String_init_BIICharset) {
    return java_lang_StringFactory_newStringFromBytes_BIICharset;
  } else if (string_init == java_lang_String_init_BCharset) {
    return java_lang_StringFactory_newStringFromBytes_BCharset;
  } else if (string_init == java_lang_String_init_C) {
    return java_lang_StringFactory_newStringFromChars_C;
  } else if (string_init == java_lang_String_init_CII) {
    return java_lang_StringFactory_newStringFromChars_CII;
  } else if (string_init == java_lang_String_init_IIC) {
    return java_lang_StringFactory_newStringFromCharsNoCheck;
  } else if (string_init == java_lang_String_init_String) {
    return java_lang_StringFactory_newStringFromString;
  } else if (string_init == java_lang_String_init_StringBuffer) {
    return java_lang_StringFactory_newStringFromStringBuffer;
  } else if (string_init == java_lang_String_init_III) {
    return java_lang_StringFactory_newStringFromCodePoints;
  } else if (string_init == java_lang_String_init_StringBuilder) {
    return java_lang_StringFactory_newStringFromStringBuilder;
  }
  LOG(FATAL) << "Could not find StringFactory method for String.<init>";
  return nullptr;
}

}  // namespace art
