/*
 * Copyright (C) 2014 The Android Open Source Project
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

# include "mir_method_info.h"

#include "driver/compiler_driver.h"
#include "driver/dex_compilation_unit.h"
#include "driver/compiler_driver-inl.h"
#include "mirror/class_loader.h"  // Only to allow casts in Handle<ClassLoader>.
#include "mirror/dex_cache.h"     // Only to allow casts in Handle<DexCache>.
#include "scoped_thread_state_change.h"
#include "handle_scope-inl.h"

namespace art {

void MirMethodLoweringInfo::Resolve(CompilerDriver* compiler_driver,
                                    const DexCompilationUnit* mUnit,
                                    MirMethodLoweringInfo* method_infos, size_t count) {
  if (kIsDebugBuild) {
    DCHECK(method_infos != nullptr);
    DCHECK_NE(count, 0u);
    for (auto it = method_infos, end = method_infos + count; it != end; ++it) {
      MirMethodLoweringInfo unresolved(it->MethodIndex(), it->GetInvokeType());
      unresolved.declaring_dex_file_ = it->declaring_dex_file_;
      if (it->target_dex_file_ != nullptr) {
        unresolved.target_dex_file_ = it->target_dex_file_;
        unresolved.target_method_idx_ = it->target_method_idx_;
      }
      if (kIsDebugBuild) {
        unresolved.CheckEquals(*it);
      }
    }
  }

  // We're going to resolve methods and check access in a tight loop. It's better to hold
  // the lock and needed references once than re-acquiring them again and again.
  ScopedObjectAccess soa(Thread::Current());
  StackHandleScope<4> hs(soa.Self());
  Handle<mirror::DexCache> dex_cache(hs.NewHandle(compiler_driver->GetDexCache(mUnit)));
  Handle<mirror::ClassLoader> class_loader(
      hs.NewHandle(compiler_driver->GetClassLoader(soa, mUnit)));
  Handle<mirror::Class> referrer_class(hs.NewHandle(
      compiler_driver->ResolveCompilingMethodsClass(soa, dex_cache, class_loader, mUnit)));
  auto current_dex_cache(hs.NewHandle<mirror::DexCache>(nullptr));
  // Even if the referrer class is unresolved (i.e. we're compiling a method without class
  // definition) we still want to resolve methods and record all available info.

  for (auto it = method_infos, end = method_infos + count; it != end; ++it) {
    // Remember devirtualized invoke target and set the called method to the default.
    MethodReference devirt_ref(it->target_dex_file_, it->target_method_idx_);
    MethodReference* devirt_target = (it->target_dex_file_ != nullptr) ? &devirt_ref : nullptr;
    InvokeType invoke_type = it->GetInvokeType();
    it->target_dex_file_ = it->declaring_dex_file_;
    it->target_method_idx_ = it->MethodIndex();
    mirror::ArtMethod* resolved_method = nullptr;
    if (it->declaring_dex_file_ == mUnit->GetDexFile()) {
      current_dex_cache.Assign(dex_cache.Get());
      resolved_method =
          compiler_driver->ResolveMethod(soa, dex_cache, class_loader, mUnit, it->MethodIndex(),
                                         invoke_type);
    } else {
      CHECK_EQ(invoke_type, kVirtual);
      // LOG(WARNING) << it->declaring_dex_file_ << " vs " << mUnit->GetDexFile() << " for " << invoke_type;
      // Don't devirt if we are in a different dex file since we can't have direct invokes in
      // another dex file unless we always put a direct / patch pointer.
      devirt_target = nullptr;
      auto* cl = mUnit->GetClassLinker();
      current_dex_cache.Assign(cl->FindDexCache(*it->declaring_dex_file_));
      CHECK(current_dex_cache.Get() != nullptr);
      resolved_method = cl->ResolveMethod(
          *it->declaring_dex_file_, it->MethodIndex(), current_dex_cache, class_loader,
          NullHandle<mirror::ArtMethod>(), invoke_type);
      DCHECK_EQ(resolved_method == nullptr, soa.Self()->IsExceptionPending());
      CHECK(resolved_method != nullptr);
      if (!Runtime::Current()->UseJit()) {
        // The method is guaranteed to be verified if we are the JIT, lets allow it.
        CHECK(!resolved_method->CheckIncompatibleClassChange(invoke_type));
      }
    }
    if (UNLIKELY(resolved_method == nullptr)) {
      continue;
    }
    compiler_driver->GetResolvedMethodDexFileLocation(resolved_method,
        &it->declaring_dex_file_, &it->declaring_class_idx_, &it->declaring_method_idx_);
    it->vtable_idx_ = compiler_driver->GetResolvedMethodVTableIndex(resolved_method, invoke_type);

    // MethodReference target_method(it->declaring_dex_file_, it->MethodIndex());
    MethodReference target_method(it->target_dex_file_, it->MethodIndex());
    int fast_path_flags = compiler_driver->IsFastInvoke(
        soa, dex_cache, class_loader, mUnit, referrer_class.Get(), resolved_method,
        &invoke_type, &target_method, devirt_target, &it->direct_code_, &it->direct_method_);
    const bool is_referrers_class = referrer_class.Get() == resolved_method->GetDeclaringClass();
    const bool is_class_initialized =
        compiler_driver->IsMethodsClassInitialized(referrer_class.Get(), resolved_method);
    uint16_t other_flags = it->flags_ &
        ~(kFlagFastPath | kFlagClassIsInitialized | (kInvokeTypeMask << kBitSharpTypeBegin));
    it->flags_ = other_flags |
        (fast_path_flags != 0 ? kFlagFastPath : 0u) |
        (static_cast<uint16_t>(invoke_type) << kBitSharpTypeBegin) |
        (is_referrers_class ? kFlagIsReferrersClass : 0u) |
        (is_class_initialized ? kFlagClassIsInitialized : 0u);
    it->target_dex_file_ = target_method.dex_file;
    it->target_method_idx_ = target_method.dex_method_index;
    it->stats_flags_ = fast_path_flags;
  }
}

}  // namespace art
