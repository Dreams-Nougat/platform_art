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

#ifndef ART_COMPILER_OPTIMIZING_REFERENCE_TYPE_PROPAGATION_H_
#define ART_COMPILER_OPTIMIZING_REFERENCE_TYPE_PROPAGATION_H_

#include "base/arena_containers.h"
#include "driver/dex_compilation_unit.h"
#include "handle_scope-inl.h"
#include "nodes.h"
#include "obj_ptr.h"
#include "optimization.h"
#include "optimizing_compiler_stats.h"

namespace art {

/**
 * Propagates reference types to instructions.
 */
class ReferenceTypePropagation : public HOptimization {
 public:
  ReferenceTypePropagation(HGraph* graph,
                           Handle<mirror::DexCache> hint_dex_cache,
                           VariableSizedHandleScope* handles,
                           bool is_first_run,
                           const char* name = kReferenceTypePropagationPassName);

  // Visit a single instruction.
  void Visit(HInstruction* instruction);

  void Run() OVERRIDE;

  // Returns true if klass is admissible to the propagation: non-null and resolved.
  // For an array type, we also check if the component type is admissible.
  static bool IsAdmissible(mirror::Class* klass) REQUIRES_SHARED(Locks::mutator_lock_) {
    return klass != nullptr &&
           klass->IsResolved() &&
           (!klass->IsArrayClass() || IsAdmissible(klass->GetComponentType()));
  }

  static constexpr const char* kReferenceTypePropagationPassName = "reference_type_propagation";

 private:
  class HandleCache {
   public:
    explicit HandleCache(VariableSizedHandleScope* handles) : handles_(handles) { }

    template <typename T>
    MutableHandle<T> NewHandle(T* object) REQUIRES_SHARED(Locks::mutator_lock_) {
      return handles_->NewHandle(object);
    }

    template <typename T>
    MutableHandle<T> NewHandle(ObjPtr<T> object) REQUIRES_SHARED(Locks::mutator_lock_) {
      return handles_->NewHandle(object);
    }

    ReferenceTypeInfo::TypeHandle GetObjectClassHandle();
    ReferenceTypeInfo::TypeHandle GetClassClassHandle();
    ReferenceTypeInfo::TypeHandle GetStringClassHandle();
    ReferenceTypeInfo::TypeHandle GetThrowableClassHandle();

   private:
    VariableSizedHandleScope* handles_;

    ReferenceTypeInfo::TypeHandle object_class_handle_;
    ReferenceTypeInfo::TypeHandle class_class_handle_;
    ReferenceTypeInfo::TypeHandle string_class_handle_;
    ReferenceTypeInfo::TypeHandle throwable_class_handle_;
  };

  class RTPVisitor;

  void VisitPhi(HPhi* phi);
  void VisitBasicBlock(HBasicBlock* block);
  void UpdateBoundType(HBoundType* bound_type) REQUIRES_SHARED(Locks::mutator_lock_);
  void UpdatePhi(HPhi* phi) REQUIRES_SHARED(Locks::mutator_lock_);
  void BoundTypeForIfNotNull(HBasicBlock* block);
  void BoundTypeForIfInstanceOf(HBasicBlock* block);
  void ProcessWorklist();
  void AddToWorklist(HInstruction* instr);
  void AddDependentInstructionsToWorklist(HInstruction* instr);

  bool UpdateNullability(HInstruction* instr);
  bool UpdateReferenceTypeInfo(HInstruction* instr);

  static void UpdateArrayGet(HArrayGet* instr, HandleCache* handle_cache)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ReferenceTypeInfo MergeTypes(const ReferenceTypeInfo& a, const ReferenceTypeInfo& b)
      REQUIRES_SHARED(Locks::mutator_lock_);

  void ValidateTypes();

  // Note: hint_dex_cache_ is usually, but not necessarily, the dex cache associated with
  // graph_->GetDexFile(). Since we may look up also in other dex files, it's used only
  // as a hint, to reduce the number of calls to the costly ClassLinker::FindDexCache().
  Handle<mirror::DexCache> hint_dex_cache_;
  HandleCache handle_cache_;

  ArenaVector<HInstruction*> worklist_;

  // Whether this reference type propagation is the first run we are doing.
  const bool is_first_run_;

  static constexpr size_t kDefaultWorklistSize = 8;

  friend class ReferenceTypePropagationTest;

  DISALLOW_COPY_AND_ASSIGN(ReferenceTypePropagation);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_REFERENCE_TYPE_PROPAGATION_H_
