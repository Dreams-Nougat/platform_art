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

#include "verifier_metadata.h"

#include "bytecode_utils.h"
#include "class_linker.h"
#include "common_runtime_test.h"
#include "dex_file.h"
#include "handle_scope-inl.h"
#include "method_verifier-inl.h"
#include "mirror/class_loader.h"
#include "runtime.h"
#include "thread.h"
#include "scoped_thread_state_change.h"

namespace art {
namespace verifier {

class DexVerifierMetadataTest : public CommonRuntimeTest {
 public:
  mirror::Class* FindClassByName(const std::string& name, ScopedObjectAccess* soa)
      SHARED_REQUIRES(Locks::mutator_lock_) {
    StackHandleScope<1> hs(Thread::Current());
    Handle<mirror::ClassLoader> class_loader_handle(
        hs.NewHandle(soa->Decode<mirror::ClassLoader*>(class_loader_)));
    mirror::Class* result = class_linker_->FindClass(Thread::Current(),
                                                     name.c_str(),
                                                     class_loader_handle);
    DCHECK(result != nullptr) << name;
    return result;
  }

  void LoadDexFile(ScopedObjectAccess* soa) SHARED_REQUIRES(Locks::mutator_lock_) {
    class_loader_ = LoadDex("DexVerifierMetadata");
    std::vector<const DexFile*> dex_files = GetDexFiles(class_loader_);
    CHECK_EQ(dex_files.size(), 1u);
    dex_file_ = dex_files.front();

    mirror::ClassLoader* loader = soa->Decode<mirror::ClassLoader*>(class_loader_);
    class_linker_ = Runtime::Current()->GetClassLinker();
    class_linker_->RegisterDexFile(*dex_file_, loader);

    klass_Main_ = FindClassByName("LMain;", soa);
    CHECK(klass_Main_ != nullptr);

    metadata_.reset(new DexVerifierMetadata(*dex_file_));
  }

  bool ProcessMetadata() SHARED_REQUIRES(Locks::mutator_lock_) {
    // Dump metadata into a set of strings.
    {
      std::ostringstream os;
      metadata_->Dump(os);
      std::istringstream is(os.str());
      std::string line;
      while (std::getline(is, line)) {
        metadata_dump_.insert(line);
      }
    }

    // Try to satisfy the dependencies.
    return metadata_->Verify(class_loader_);
  }

  bool VerifyMethod(const std::string& method_name) {
    ScopedObjectAccess soa(Thread::Current());
    LoadDexFile(&soa);

    StackHandleScope<2> hs(Thread::Current());
    Handle<mirror::ClassLoader> class_loader_handle(
        hs.NewHandle(soa.Decode<mirror::ClassLoader*>(class_loader_)));
    Handle<mirror::DexCache> dex_cache_handle(hs.NewHandle(klass_Main_->GetDexCache()));

    const DexFile::ClassDef* class_def = klass_Main_->GetClassDef();
    const uint8_t* class_data = dex_file_->GetClassData(*class_def);
    CHECK(class_data != nullptr);

    ClassDataItemIterator it(*dex_file_, class_data);
    while (it.HasNextStaticField() || it.HasNextInstanceField()) {
      it.Next();
    }

    ArtMethod* method = nullptr;
    while (it.HasNextDirectMethod()) {
      ArtMethod* resolved_method = class_linker_->ResolveMethod<ClassLinker::kNoICCECheckForCache>(
          *dex_file_,
          it.GetMemberIndex(),
          dex_cache_handle,
          class_loader_handle,
          nullptr,
          it.GetMethodInvokeType(*class_def));
      CHECK(resolved_method != nullptr);
      if (method_name == resolved_method->GetName()) {
        method = resolved_method;
        break;
      }
      it.Next();
    }
    CHECK(method != nullptr);

    MethodVerifier verifier(Thread::Current(),
                            dex_file_,
                            dex_cache_handle,
                            class_loader_handle,
                            *class_def,
                            it.GetMethodCodeItem(),
                            it.GetMemberIndex(),
                            method,
                            it.GetMethodAccessFlags(),
                            true /* can_load_classes */,
                            true /* allow_soft_failures */,
                            true /* need_precise_constants */,
                            false /* verify to dump */,
                            true /* allow_thread_suspension */,
                            metadata_.get());
    verifier.Verify();
    CHECK(ProcessMetadata());
    return !verifier.HasFailures();
  }

  bool TestAssignabilityRecording(const std::string& dst,
                                  const std::string& src,
                                  bool is_strict,
                                  bool is_assignable) {
    ScopedObjectAccess soa(Thread::Current());
    LoadDexFile(&soa);
    metadata_->RecordAssignabilityTest(FindClassByName(dst, &soa),
                                       FindClassByName(src, &soa),
                                       is_strict,
                                       is_assignable);
    return ProcessMetadata();
  }

  bool HasDependency(const std::string& str) {
    return ContainsElement(metadata_dump_, str) || ContainsElement(metadata_dump_, str + " ");
  }

  const DexFile* dex_file_;
  jobject class_loader_;
  ClassLinker* class_linker_;
  mirror::Class* klass_Main_;

  std::unique_ptr<DexVerifierMetadata> metadata_;
  std::set<std::string> metadata_dump_;
};

TEST_F(DexVerifierMetadataTest, StringToId) {
  ScopedObjectAccess soa(Thread::Current());
  LoadDexFile(&soa);

  uint32_t id_Main1 = metadata_->GetIdFromString("LMain;");
  ASSERT_TRUE(metadata_->IsStringIdInDexFile(id_Main1));
  ASSERT_EQ("LMain;", metadata_->GetStringFromId(id_Main1));

  uint32_t id_Main2 = metadata_->GetIdFromString("LMain;");
  ASSERT_TRUE(metadata_->IsStringIdInDexFile(id_Main2));
  ASSERT_EQ("LMain;", metadata_->GetStringFromId(id_Main2));

  uint32_t id_Lorem1 = metadata_->GetIdFromString("Lorem ipsum");
  ASSERT_FALSE(metadata_->IsStringIdInDexFile(id_Lorem1));
  ASSERT_EQ("Lorem ipsum", metadata_->GetStringFromId(id_Lorem1));

  uint32_t id_Lorem2 = metadata_->GetIdFromString("Lorem ipsum");
  ASSERT_FALSE(metadata_->IsStringIdInDexFile(id_Lorem2));
  ASSERT_EQ("Lorem ipsum", metadata_->GetStringFromId(id_Lorem2));

  ASSERT_EQ(id_Main1, id_Main2);
  ASSERT_EQ(id_Lorem1, id_Lorem2);
  ASSERT_NE(id_Main1, id_Lorem1);
}

TEST_F(DexVerifierMetadataTest, Assignable_BothInBoot) {
  ASSERT_TRUE(TestAssignabilityRecording(/* dst */ "Ljava/util/TimeZone;",
                                         /* src */ "Ljava/util/SimpleTimeZone;",
                                         /* is_strict */ true,
                                         /* is_assignable */ true));
  ASSERT_TRUE(HasDependency(
      "type Ljava/util/TimeZone; assignable from Ljava/util/SimpleTimeZone;"));
}

TEST_F(DexVerifierMetadataTest, Assignable_DestinationInBoot1) {
  ASSERT_TRUE(TestAssignabilityRecording(/* dst */ "Ljava/net/Socket;",
                                         /* src */ "LMySSLSocket;",
                                         /* is_strict */ true,
                                         /* is_assignable */ true));
  ASSERT_TRUE(HasDependency(
      "type Ljava/net/Socket; assignable from LMySSLSocket;"));
}

TEST_F(DexVerifierMetadataTest, Assignable_DestinationInBoot2) {
  ASSERT_TRUE(TestAssignabilityRecording(/* dst */ "Ljava/util/TimeZone;",
                                         /* src */ "LMySimpleTimeZone;",
                                         /* is_strict */ true,
                                         /* is_assignable */ true));
  ASSERT_TRUE(HasDependency("type Ljava/util/TimeZone; assignable from LMySimpleTimeZone;"));
}

TEST_F(DexVerifierMetadataTest, Assignable_DestinationInBoot3) {
  ASSERT_TRUE(TestAssignabilityRecording(/* dst */ "Ljava/util/Collection;",
                                         /* src */ "LMyThreadSet;",
                                         /* is_strict */ true,
                                         /* is_assignable */ true));
  ASSERT_TRUE(HasDependency("type Ljava/util/Collection; assignable from LMyThreadSet;"));
}

TEST_F(DexVerifierMetadataTest, Assignable_BothArrays) {
  ASSERT_TRUE(TestAssignabilityRecording(/* dst */ "[Ljava/util/TimeZone;",
                                         /* src */ "[Ljava/util/SimpleTimeZone;",
                                         /* is_strict */ true,
                                         /* is_assignable */ true));
  ASSERT_TRUE(HasDependency(
      "type Ljava/util/TimeZone; assignable from Ljava/util/SimpleTimeZone;"));
}

TEST_F(DexVerifierMetadataTest, NotAssignable_BothInBoot) {
  ASSERT_TRUE(TestAssignabilityRecording(/* dst */ "Ljava/lang/Exception;",
                                         /* src */ "Ljava/util/SimpleTimeZone;",
                                         /* is_strict */ true,
                                         /* is_assignable */ false));
  ASSERT_TRUE(HasDependency(
      "type Ljava/lang/Exception; not assignable from Ljava/util/SimpleTimeZone;"));
}

TEST_F(DexVerifierMetadataTest, NotAssignable_DestinationInBoot1) {
  ASSERT_TRUE(TestAssignabilityRecording(/* dst */ "Ljava/lang/Exception;",
                                         /* src */ "LMySSLSocket;",
                                         /* is_strict */ true,
                                         /* is_assignable */ false));
  ASSERT_TRUE(HasDependency("type Ljava/lang/Exception; not assignable from LMySSLSocket;"));
}

TEST_F(DexVerifierMetadataTest, NotAssignable_DestinationInBoot2) {
  ASSERT_TRUE(TestAssignabilityRecording(/* dst */ "Ljava/lang/Exception;",
                                         /* src */ "LMySimpleTimeZone;",
                                         /* is_strict */ true,
                                         /* is_assignable */ false));
  ASSERT_TRUE(HasDependency("type Ljava/lang/Exception; not assignable from LMySimpleTimeZone;"));
}

TEST_F(DexVerifierMetadataTest, NotAssignable_BothArrays) {
  ASSERT_TRUE(TestAssignabilityRecording(/* dst */ "[Ljava/lang/Exception;",
                                         /* src */ "[Ljava/util/SimpleTimeZone;",
                                         /* is_strict */ true,
                                         /* is_assignable */ false));
  ASSERT_TRUE(HasDependency(
      "type Ljava/lang/Exception; not assignable from Ljava/util/SimpleTimeZone;"));
}

TEST_F(DexVerifierMetadataTest, ArgumentType_ResolvedClass) {
  ASSERT_TRUE(VerifyMethod("ArgumentType_ResolvedClass"));
  ASSERT_TRUE(HasDependency("class Ljava/lang/Thread; public"));
}

TEST_F(DexVerifierMetadataTest, ArgumentType_ResolvedReferenceArray) {
  ASSERT_TRUE(VerifyMethod("ArgumentType_ResolvedReferenceArray"));
  ASSERT_TRUE(HasDependency("class [Ljava/lang/Thread; public final abstract"));
}

TEST_F(DexVerifierMetadataTest, ArgumentType_ResolvedPrimitiveArray) {
  ASSERT_TRUE(VerifyMethod("ArgumentType_ResolvedPrimitiveArray"));
  // We do not record primitive arrays.
  ASSERT_FALSE(HasDependency("class [B public final abstract"));
}

TEST_F(DexVerifierMetadataTest, ArgumentType_UnresolvedClass) {
  ASSERT_TRUE(VerifyMethod("ArgumentType_UnresolvedClass"));
  ASSERT_TRUE(HasDependency("class LUnresolvedClass; unresolved"));
}

TEST_F(DexVerifierMetadataTest, ArgumentType_UnresolvedSuper) {
  ASSERT_TRUE(VerifyMethod("ArgumentType_UnresolvedSuper"));
  ASSERT_TRUE(HasDependency("class LMySetWithUnresolvedSuper; unresolved"));
}

TEST_F(DexVerifierMetadataTest, ReturnType_Reference) {
  ASSERT_TRUE(VerifyMethod("ReturnType_Reference"));
  ASSERT_TRUE(HasDependency(
      "type Ljava/lang/Throwable; assignable from Ljava/lang/IllegalStateException;"));
}

TEST_F(DexVerifierMetadataTest, ReturnType_Array) {
  ASSERT_FALSE(VerifyMethod("ReturnType_Array"));
  ASSERT_TRUE(HasDependency(
      "type Ljava/lang/Integer; not assignable from Ljava/lang/IllegalStateException;"));
}

TEST_F(DexVerifierMetadataTest, InvokeArgumentType) {
  ASSERT_TRUE(VerifyMethod("InvokeArgumentType"));
  ASSERT_TRUE(HasDependency("class Ljava/text/SimpleDateFormat; public"));
  ASSERT_TRUE(HasDependency("class Ljava/util/SimpleTimeZone; public"));
  ASSERT_TRUE(HasDependency("virtual method Ljava/text/SimpleDateFormat;->"
      "setTimeZone(Ljava/util/TimeZone;)V public in Ljava/text/DateFormat;"));
  ASSERT_TRUE(HasDependency(
      "type Ljava/util/TimeZone; assignable from Ljava/util/SimpleTimeZone;"));
}

TEST_F(DexVerifierMetadataTest, MergeTypes_RegisterLines) {
  ASSERT_TRUE(VerifyMethod("MergeTypes_RegisterLines"));
  ASSERT_TRUE(HasDependency(
      "type Ljava/lang/Exception; assignable from LMySocketTimeoutException;"));
  ASSERT_TRUE(HasDependency(
      "type Ljava/lang/Exception; assignable from Ljava/util/concurrent/TimeoutException;"));
}

TEST_F(DexVerifierMetadataTest, MergeTypes_IfInstanceOf) {
  ASSERT_TRUE(VerifyMethod("MergeTypes_IfInstanceOf"));
  ASSERT_TRUE(HasDependency(
      "type Ljava/lang/Exception; assignable from Ljava/net/SocketTimeoutException;"));
  ASSERT_TRUE(HasDependency(
      "type Ljava/lang/Exception; assignable from Ljava/util/concurrent/TimeoutException;"));
  ASSERT_TRUE(HasDependency(
      "type Ljava/net/SocketTimeoutException; not assignable from Ljava/lang/Exception;"));
}

TEST_F(DexVerifierMetadataTest, MergeTypes_Unresolved) {
  ASSERT_TRUE(VerifyMethod("MergeTypes_Unresolved"));
  ASSERT_TRUE(HasDependency(
      "type Ljava/lang/Exception; assignable from Ljava/net/SocketTimeoutException;"));
  ASSERT_TRUE(HasDependency(
      "type Ljava/lang/Exception; assignable from Ljava/util/concurrent/TimeoutException;"));
}

TEST_F(DexVerifierMetadataTest, ConstClass_Resolved) {
  ASSERT_TRUE(VerifyMethod("ConstClass_Resolved"));
  ASSERT_TRUE(HasDependency("class Ljava/lang/IllegalStateException; public"));
}

TEST_F(DexVerifierMetadataTest, ConstClass_Unresolved) {
  ASSERT_TRUE(VerifyMethod("ConstClass_Unresolved"));
  ASSERT_TRUE(HasDependency("class LUnresolvedClass; unresolved"));
}

TEST_F(DexVerifierMetadataTest, CheckCast_Resolved) {
  ASSERT_TRUE(VerifyMethod("CheckCast_Resolved"));
  ASSERT_TRUE(HasDependency("class Ljava/lang/IllegalStateException; public"));
}

TEST_F(DexVerifierMetadataTest, CheckCast_Unresolved) {
  ASSERT_TRUE(VerifyMethod("CheckCast_Unresolved"));
  ASSERT_TRUE(HasDependency("class LUnresolvedClass; unresolved"));
}

TEST_F(DexVerifierMetadataTest, InstanceOf_Resolved) {
  ASSERT_TRUE(VerifyMethod("InstanceOf_Resolved"));
  ASSERT_TRUE(HasDependency("class Ljava/lang/IllegalStateException; public"));
}

TEST_F(DexVerifierMetadataTest, InstanceOf_Unresolved) {
  ASSERT_TRUE(VerifyMethod("InstanceOf_Unresolved"));
  ASSERT_TRUE(HasDependency("class LUnresolvedClass; unresolved"));
}

TEST_F(DexVerifierMetadataTest, NewInstance_Resolved) {
  ASSERT_TRUE(VerifyMethod("NewInstance_Resolved"));
  ASSERT_TRUE(HasDependency("class Ljava/lang/IllegalStateException; public"));
}

TEST_F(DexVerifierMetadataTest, NewInstance_Unresolved) {
  ASSERT_TRUE(VerifyMethod("NewInstance_Unresolved"));
  ASSERT_TRUE(HasDependency("class LUnresolvedClass; unresolved"));
}

TEST_F(DexVerifierMetadataTest, NewArray_Resolved) {
  ASSERT_TRUE(VerifyMethod("NewArray_Resolved"));
  ASSERT_TRUE(HasDependency("class [Ljava/lang/IllegalStateException; public final abstract"));
}

TEST_F(DexVerifierMetadataTest, NewArray_Unresolved) {
  ASSERT_TRUE(VerifyMethod("NewArray_Unresolved"));
  ASSERT_TRUE(HasDependency("class [LUnresolvedClass; unresolved"));
}

TEST_F(DexVerifierMetadataTest, Throw) {
  ASSERT_TRUE(VerifyMethod("Throw"));
  ASSERT_TRUE(HasDependency(
      "type Ljava/lang/Throwable; assignable from Ljava/lang/IllegalStateException;"));
}

TEST_F(DexVerifierMetadataTest, MoveException_Resolved) {
  ASSERT_TRUE(VerifyMethod("MoveException_Resolved"));
  ASSERT_TRUE(HasDependency("class Ljava/io/InterruptedIOException; public"));
  ASSERT_TRUE(HasDependency("class Ljava/net/SocketTimeoutException; public"));
  ASSERT_TRUE(HasDependency("class Ljava/util/zip/ZipException; public"));

  // Testing that all exception types are assignable to Throwable.
  ASSERT_TRUE(HasDependency(
      "type Ljava/lang/Throwable; assignable from Ljava/io/InterruptedIOException;"));
  ASSERT_TRUE(HasDependency(
      "type Ljava/lang/Throwable; assignable from Ljava/net/SocketTimeoutException;"));
  ASSERT_TRUE(HasDependency(
      "type Ljava/lang/Throwable; assignable from Ljava/util/zip/ZipException;"));

  // Testing that the merge type is assignable to Throwable.
  ASSERT_TRUE(HasDependency("type Ljava/lang/Throwable; assignable from Ljava/io/IOException;"));

  // Merging of exception types.
  ASSERT_TRUE(HasDependency(
      "type Ljava/io/IOException; assignable from Ljava/io/InterruptedIOException;"));
  ASSERT_TRUE(HasDependency(
      "type Ljava/io/IOException; assignable from Ljava/util/zip/ZipException;"));
  ASSERT_TRUE(HasDependency(
      "type Ljava/io/InterruptedIOException; assignable from Ljava/net/SocketTimeoutException;"));
}

TEST_F(DexVerifierMetadataTest, MoveException_Unresolved) {
  ASSERT_FALSE(VerifyMethod("MoveException_Unresolved"));
  ASSERT_TRUE(HasDependency("class LUnresolvedException; unresolved"));
}

TEST_F(DexVerifierMetadataTest, StaticField_Resolved_DeclaredInReferenced) {
  ASSERT_TRUE(VerifyMethod("StaticField_Resolved_DeclaredInReferenced"));
  ASSERT_TRUE(HasDependency("class Ljava/lang/System; public final"));
  ASSERT_TRUE(HasDependency("field Ljava/lang/System;->out:Ljava/io/PrintStream; "
      "public final static in Ljava/lang/System;"));
}

TEST_F(DexVerifierMetadataTest, StaticField_Resolved_DeclaredInSuperclass1) {
  ASSERT_TRUE(VerifyMethod("StaticField_Resolved_DeclaredInSuperclass1"));
  ASSERT_TRUE(HasDependency("class Ljava/util/SimpleTimeZone; public"));
  ASSERT_TRUE(HasDependency("field Ljava/util/SimpleTimeZone;->LONG:I "
      "public final static in Ljava/util/TimeZone;"));
}

TEST_F(DexVerifierMetadataTest, StaticField_Resolved_DeclaredInSuperclass2) {
  ASSERT_TRUE(VerifyMethod("StaticField_Resolved_DeclaredInSuperclass2"));
  ASSERT_TRUE(HasDependency("field LMySimpleTimeZone;->SHORT:I "
      "public final static in Ljava/util/TimeZone;"));
}

TEST_F(DexVerifierMetadataTest, StaticField_Resolved_DeclaredInInterface1) {
  ASSERT_TRUE(VerifyMethod("StaticField_Resolved_DeclaredInInterface1"));
  ASSERT_TRUE(HasDependency("class Ljavax/xml/transform/dom/DOMResult; public"));
  ASSERT_TRUE(HasDependency("field Ljavax/xml/transform/dom/DOMResult;->"
      "PI_ENABLE_OUTPUT_ESCAPING:Ljava/lang/String; "
      "public final static in Ljavax/xml/transform/Result;"));
}

TEST_F(DexVerifierMetadataTest, StaticField_Resolved_DeclaredInInterface2) {
  ASSERT_TRUE(VerifyMethod("StaticField_Resolved_DeclaredInInterface2"));
  ASSERT_TRUE(HasDependency("field LMyDOMResult;->PI_ENABLE_OUTPUT_ESCAPING:Ljava/lang/String; "
      "public final static in Ljavax/xml/transform/Result;"));
}

TEST_F(DexVerifierMetadataTest, StaticField_Resolved_DeclaredInInterface3) {
  ASSERT_TRUE(VerifyMethod("StaticField_Resolved_DeclaredInInterface3"));
  ASSERT_TRUE(HasDependency("field LMyResult;->PI_ENABLE_OUTPUT_ESCAPING:Ljava/lang/String; "
      "public final static in Ljavax/xml/transform/Result;"));
}

TEST_F(DexVerifierMetadataTest, StaticField_Resolved_DeclaredInInterface4) {
  ASSERT_TRUE(VerifyMethod("StaticField_Resolved_DeclaredInInterface4"));
  ASSERT_TRUE(HasDependency("field LMyDocument;->ELEMENT_NODE:S "
      "public final static in Lorg/w3c/dom/Node;"));
}

TEST_F(DexVerifierMetadataTest, StaticField_Unresolved_ReferrerInBoot) {
  ASSERT_TRUE(VerifyMethod("StaticField_Unresolved_ReferrerInBoot"));
  ASSERT_TRUE(HasDependency("class Ljava/util/TimeZone; public abstract"));
  ASSERT_TRUE(HasDependency("field Ljava/util/TimeZone;->x:I unresolved"));
}

TEST_F(DexVerifierMetadataTest, StaticField_Unresolved_ReferrerInDex) {
  ASSERT_TRUE(VerifyMethod("StaticField_Unresolved_ReferrerInDex"));
  ASSERT_TRUE(HasDependency("field LMyThreadSet;->x:I unresolved"));
}

TEST_F(DexVerifierMetadataTest, InstanceField_Resolved_DeclaredInReferenced) {
  ASSERT_TRUE(VerifyMethod("InstanceField_Resolved_DeclaredInReferenced"));
  ASSERT_TRUE(HasDependency("class Ljava/io/InterruptedIOException; public"));
  ASSERT_TRUE(HasDependency("field Ljava/io/InterruptedIOException;->bytesTransferred:I "
      "public in Ljava/io/InterruptedIOException;"));
  ASSERT_TRUE(HasDependency(
      "type Ljava/io/InterruptedIOException; assignable from LMySocketTimeoutException;"));
}

TEST_F(DexVerifierMetadataTest, InstanceField_Resolved_DeclaredInSuperclass1) {
  ASSERT_TRUE(VerifyMethod("InstanceField_Resolved_DeclaredInSuperclass1"));
  ASSERT_TRUE(HasDependency("class Ljava/net/SocketTimeoutException; public"));
  ASSERT_TRUE(HasDependency("field Ljava/net/SocketTimeoutException;->bytesTransferred:I "
      "public in Ljava/io/InterruptedIOException;"));
  ASSERT_TRUE(HasDependency(
      "type Ljava/io/InterruptedIOException; assignable from LMySocketTimeoutException;"));
}

TEST_F(DexVerifierMetadataTest, InstanceField_Resolved_DeclaredInSuperclass2) {
  ASSERT_TRUE(VerifyMethod("InstanceField_Resolved_DeclaredInSuperclass2"));
  ASSERT_TRUE(HasDependency("field LMySocketTimeoutException;->bytesTransferred:I "
      "public in Ljava/io/InterruptedIOException;"));
  ASSERT_TRUE(HasDependency(
      "type Ljava/io/InterruptedIOException; assignable from LMySocketTimeoutException;"));
}

TEST_F(DexVerifierMetadataTest, InstanceField_Unresolved_ReferrerInBoot) {
  ASSERT_TRUE(VerifyMethod("InstanceField_Unresolved_ReferrerInBoot"));
  ASSERT_TRUE(HasDependency("class Ljava/io/InterruptedIOException; public"));
  ASSERT_TRUE(HasDependency("field Ljava/io/InterruptedIOException;->x:I unresolved"));
}

TEST_F(DexVerifierMetadataTest, InstanceField_Unresolved_ReferrerInDex) {
  ASSERT_TRUE(VerifyMethod("InstanceField_Unresolved_ReferrerInDex"));
  ASSERT_TRUE(HasDependency("field LMyThreadSet;->x:I unresolved"));
}

TEST_F(DexVerifierMetadataTest, InvokeStatic_Resolved_DeclaredInReferenced) {
  ASSERT_TRUE(VerifyMethod("InvokeStatic_Resolved_DeclaredInReferenced"));
  ASSERT_TRUE(HasDependency("class Ljava/net/Socket; public"));
  ASSERT_TRUE(HasDependency("direct method Ljava/net/Socket;->"
      "setSocketImplFactory(Ljava/net/SocketImplFactory;)V public static in Ljava/net/Socket;"));
}

TEST_F(DexVerifierMetadataTest, InvokeStatic_Resolved_DeclaredInSuperclass1) {
  ASSERT_TRUE(VerifyMethod("InvokeStatic_Resolved_DeclaredInSuperclass1"));
  ASSERT_TRUE(HasDependency("class Ljavax/net/ssl/SSLSocket; public abstract"));
  ASSERT_TRUE(HasDependency("direct method Ljavax/net/ssl/SSLSocket;->"
      "setSocketImplFactory(Ljava/net/SocketImplFactory;)V public static in Ljava/net/Socket;"));
}

TEST_F(DexVerifierMetadataTest, InvokeStatic_Resolved_DeclaredInSuperclass2) {
  ASSERT_TRUE(VerifyMethod("InvokeStatic_Resolved_DeclaredInSuperclass2"));
  ASSERT_TRUE(HasDependency("direct method LMySSLSocket;->"
      "setSocketImplFactory(Ljava/net/SocketImplFactory;)V public static in Ljava/net/Socket;"));
}

TEST_F(DexVerifierMetadataTest, InvokeStatic_DeclaredInInterface1) {
  ASSERT_TRUE(VerifyMethod("InvokeStatic_DeclaredInInterface1"));
  ASSERT_TRUE(HasDependency("class Ljava/util/Map$Entry; public abstract interface"));
  ASSERT_TRUE(HasDependency("direct method Ljava/util/Map$Entry;->"
      "comparingByKey()Ljava/util/Comparator; public static in Ljava/util/Map$Entry;"));
}

TEST_F(DexVerifierMetadataTest, InvokeStatic_DeclaredInInterface2) {
  ASSERT_FALSE(VerifyMethod("InvokeStatic_DeclaredInInterface2"));
  ASSERT_TRUE(HasDependency("class Ljava/util/AbstractMap$SimpleEntry; public"));
  ASSERT_TRUE(HasDependency("direct method Ljava/util/AbstractMap$SimpleEntry;->"
      "comparingByKey()Ljava/util/Comparator; unresolved"));
}

TEST_F(DexVerifierMetadataTest, InvokeStatic_Unresolved1) {
  ASSERT_FALSE(VerifyMethod("InvokeStatic_Unresolved1"));
  ASSERT_TRUE(HasDependency("class Ljavax/net/ssl/SSLSocket; public abstract"));
  ASSERT_TRUE(HasDependency("direct method Ljavax/net/ssl/SSLSocket;->x()V unresolved"));
}

TEST_F(DexVerifierMetadataTest, InvokeStatic_Unresolved2) {
  ASSERT_FALSE(VerifyMethod("InvokeStatic_Unresolved2"));
  ASSERT_TRUE(HasDependency("direct method LMySSLSocket;->x()V unresolved"));
}

TEST_F(DexVerifierMetadataTest, InvokeDirect_Resolved_DeclaredInReferenced) {
  ASSERT_TRUE(VerifyMethod("InvokeDirect_Resolved_DeclaredInReferenced"));
  ASSERT_TRUE(HasDependency("class Ljava/net/Socket; public"));
  ASSERT_TRUE(HasDependency(
      "direct method Ljava/net/Socket;-><init>()V public in Ljava/net/Socket;"));
}

TEST_F(DexVerifierMetadataTest, InvokeDirect_Resolved_DeclaredInSuperclass1) {
  ASSERT_FALSE(VerifyMethod("InvokeDirect_Resolved_DeclaredInSuperclass1"));
  ASSERT_TRUE(HasDependency("class Ljavax/net/ssl/SSLSocket; public abstract"));
  ASSERT_TRUE(HasDependency(
      "direct method Ljavax/net/ssl/SSLSocket;->checkOldImpl()V private in Ljava/net/Socket;"));
}

TEST_F(DexVerifierMetadataTest, InvokeDirect_Resolved_DeclaredInSuperclass2) {
  ASSERT_FALSE(VerifyMethod("InvokeDirect_Resolved_DeclaredInSuperclass2"));
  ASSERT_TRUE(HasDependency(
      "direct method LMySSLSocket;->checkOldImpl()V private in Ljava/net/Socket;"));
}

TEST_F(DexVerifierMetadataTest, InvokeDirect_Unresolved1) {
  ASSERT_FALSE(VerifyMethod("InvokeDirect_Unresolved1"));
  ASSERT_TRUE(HasDependency("class Ljavax/net/ssl/SSLSocket; public abstract"));
  ASSERT_TRUE(HasDependency("direct method Ljavax/net/ssl/SSLSocket;->x()V unresolved"));
}

TEST_F(DexVerifierMetadataTest, InvokeDirect_Unresolved2) {
  ASSERT_FALSE(VerifyMethod("InvokeDirect_Unresolved2"));
  ASSERT_TRUE(HasDependency("direct method LMySSLSocket;->x()V unresolved"));
}

TEST_F(DexVerifierMetadataTest, InvokeVirtual_Resolved_DeclaredInReferenced) {
  ASSERT_TRUE(VerifyMethod("InvokeVirtual_Resolved_DeclaredInReferenced"));
  ASSERT_TRUE(HasDependency("class Ljava/lang/Throwable; public"));
  ASSERT_TRUE(HasDependency("virtual method Ljava/lang/Throwable;->getMessage()Ljava/lang/String; "
      "public in Ljava/lang/Throwable;"));
  // Type dependency on `this` argument.
  ASSERT_TRUE(HasDependency(
      "type Ljava/lang/Throwable; assignable from LMySocketTimeoutException;"));
}

TEST_F(DexVerifierMetadataTest, InvokeVirtual_Resolved_DeclaredInSuperclass1) {
  ASSERT_TRUE(VerifyMethod("InvokeVirtual_Resolved_DeclaredInSuperclass1"));
  ASSERT_TRUE(HasDependency("class Ljava/io/InterruptedIOException; public"));
  ASSERT_TRUE(HasDependency("virtual method Ljava/io/InterruptedIOException;->"
      "getMessage()Ljava/lang/String; public in Ljava/lang/Throwable;"));
  // Type dependency on `this` argument.
  ASSERT_TRUE(HasDependency(
      "type Ljava/lang/Throwable; assignable from LMySocketTimeoutException;"));
}

TEST_F(DexVerifierMetadataTest, InvokeVirtual_Resolved_DeclaredInSuperclass2) {
  ASSERT_TRUE(VerifyMethod("InvokeVirtual_Resolved_DeclaredInSuperclass2"));
  ASSERT_TRUE(HasDependency("virtual method LMySocketTimeoutException;->"
      "getMessage()Ljava/lang/String; public in Ljava/lang/Throwable;"));
}

TEST_F(DexVerifierMetadataTest, InvokeVirtual_Resolved_DeclaredInSuperinterface) {
  ASSERT_TRUE(VerifyMethod("InvokeVirtual_Resolved_DeclaredInSuperinterface"));
  ASSERT_TRUE(HasDependency("virtual method LMyThreadSet;->size()I "
      "public abstract in Ljava/util/Set;"));
}

TEST_F(DexVerifierMetadataTest, InvokeVirtual_Unresolved1) {
  ASSERT_FALSE(VerifyMethod("InvokeVirtual_Unresolved1"));
  ASSERT_TRUE(HasDependency("class Ljava/io/InterruptedIOException; public"));
  ASSERT_TRUE(HasDependency("virtual method Ljava/io/InterruptedIOException;->x()V unresolved"));
}

TEST_F(DexVerifierMetadataTest, InvokeVirtual_Unresolved2) {
  ASSERT_FALSE(VerifyMethod("InvokeVirtual_Unresolved2"));
  ASSERT_TRUE(HasDependency("virtual method LMySocketTimeoutException;->x()V unresolved"));
}

TEST_F(DexVerifierMetadataTest, InvokeVirtual_ActuallyDirect) {
  ASSERT_FALSE(VerifyMethod("InvokeVirtual_ActuallyDirect"));
  ASSERT_TRUE(HasDependency("virtual method LMyThread;->activeCount()I unresolved"));
  ASSERT_TRUE(HasDependency(
      "direct method LMyThread;->activeCount()I public static in Ljava/lang/Thread;"));
}

TEST_F(DexVerifierMetadataTest, InvokeInterface_Resolved_DeclaredInReferenced) {
  ASSERT_TRUE(VerifyMethod("InvokeInterface_Resolved_DeclaredInReferenced"));
  ASSERT_TRUE(HasDependency("class Ljava/lang/Runnable; public abstract interface"));
  ASSERT_TRUE(HasDependency("interface method Ljava/lang/Runnable;->run()V "
      "public abstract in Ljava/lang/Runnable;"));
}

TEST_F(DexVerifierMetadataTest, InvokeInterface_Resolved_DeclaredInSuperclass) {
  ASSERT_FALSE(VerifyMethod("InvokeInterface_Resolved_DeclaredInSuperclass"));
  ASSERT_TRUE(HasDependency("interface method LMyThread;->join()V unresolved"));
}

TEST_F(DexVerifierMetadataTest, InvokeInterface_Resolved_DeclaredInSuperinterface1) {
  ASSERT_FALSE(VerifyMethod("InvokeInterface_Resolved_DeclaredInSuperinterface1"));
  ASSERT_TRUE(HasDependency("interface method LMyThreadSet;->run()V "
      "public abstract in Ljava/lang/Runnable;"));
}

TEST_F(DexVerifierMetadataTest, InvokeInterface_Resolved_DeclaredInSuperinterface2) {
  ASSERT_FALSE(VerifyMethod("InvokeInterface_Resolved_DeclaredInSuperinterface2"));
  ASSERT_TRUE(HasDependency("interface method LMyThreadSet;->isEmpty()Z "
      "public abstract in Ljava/util/Set;"));
}

TEST_F(DexVerifierMetadataTest, InvokeInterface_Unresolved1) {
  ASSERT_FALSE(VerifyMethod("InvokeInterface_Unresolved1"));
  ASSERT_TRUE(HasDependency("class Ljava/lang/Runnable; public abstract interface"));
  ASSERT_TRUE(HasDependency("interface method Ljava/lang/Runnable;->x()V unresolved"));
}

TEST_F(DexVerifierMetadataTest, InvokeInterface_Unresolved2) {
  ASSERT_FALSE(VerifyMethod("InvokeInterface_Unresolved2"));
  ASSERT_TRUE(HasDependency("interface method LMyThreadSet;->x()V unresolved"));
}

TEST_F(DexVerifierMetadataTest, InvokeSuper_ThisAssignable) {
  ASSERT_TRUE(VerifyMethod("InvokeSuper_ThisAssignable"));
  ASSERT_TRUE(HasDependency("class Ljava/lang/Runnable; public abstract interface"));
  ASSERT_TRUE(HasDependency("type Ljava/lang/Runnable; assignable from LMain;"));
  ASSERT_TRUE(HasDependency("interface method Ljava/lang/Runnable;->run()V "
      "public abstract in Ljava/lang/Runnable;"));
}

TEST_F(DexVerifierMetadataTest, InvokeSuper_ThisNotAssignable) {
  ASSERT_FALSE(VerifyMethod("InvokeSuper_ThisNotAssignable"));
  ASSERT_TRUE(HasDependency("class Ljava/lang/Integer; public final"));
  ASSERT_TRUE(HasDependency("type Ljava/lang/Integer; not assignable from LMain;"));
  ASSERT_TRUE(HasDependency("virtual method Ljava/lang/Integer;->intValue()I "
      "public in Ljava/lang/Integer;"));
}

}  // namespace verifier
}  // namespace art
