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

#include "quick_exception_handler.h"

#include "arch/context.h"
#include "art_method-inl.h"
#include "dex_instruction.h"
#include "entrypoints/entrypoint_utils.h"
#include "entrypoints/runtime_asm_entrypoints.h"
#include "handle_scope-inl.h"
#include "mirror/class-inl.h"
#include "mirror/class_loader.h"
#include "mirror/throwable.h"
#include "verifier/method_verifier.h"

namespace art {

static constexpr bool kDebugExceptionDelivery = false;
static constexpr size_t kInvalidFrameDepth = 0xffffffff;

QuickExceptionHandler::QuickExceptionHandler(Thread* self, bool is_deoptimization)
  : self_(self), context_(self->GetLongJumpContext()), is_deoptimization_(is_deoptimization),
    method_tracing_active_(is_deoptimization ||
                           Runtime::Current()->GetInstrumentation()->AreExitStubsInstalled()),
    handler_quick_frame_(nullptr), handler_quick_frame_pc_(0), handler_method_(nullptr),
    handler_dex_pc_(0), clear_exception_(false), handler_frame_depth_(kInvalidFrameDepth) {
}

// Finds catch handler.
class CatchBlockStackVisitor FINAL : public StackVisitor {
 public:
  CatchBlockStackVisitor(Thread* self, Context* context, Handle<mirror::Throwable>* exception,
                         QuickExceptionHandler* exception_handler)
      SHARED_REQUIRES(Locks::mutator_lock_)
      : StackVisitor(self, context, StackVisitor::StackWalkKind::kIncludeInlinedFrames),
        exception_(exception),
        exception_handler_(exception_handler) {
  }

  bool VisitFrame() OVERRIDE SHARED_REQUIRES(Locks::mutator_lock_) {
    ArtMethod* method = GetMethod();
    exception_handler_->SetHandlerFrameDepth(GetFrameDepth());
    if (method == nullptr) {
      // This is the upcall, we remember the frame and last pc so that we may long jump to them.
      exception_handler_->SetHandlerQuickFramePc(GetCurrentQuickFramePc());
      exception_handler_->SetHandlerQuickFrame(GetCurrentQuickFrame());
      uint32_t next_dex_pc;
      ArtMethod* next_art_method;
      bool has_next = GetNextMethodAndDexPc(&next_art_method, &next_dex_pc);
      // Report the method that did the down call as the handler.
      exception_handler_->SetHandlerDexPc(next_dex_pc);
      exception_handler_->SetHandlerMethod(next_art_method);
      if (!has_next) {
        // No next method? Check exception handler is set up for the unhandled exception handler
        // case.
        DCHECK_EQ(0U, exception_handler_->GetHandlerDexPc());
        DCHECK(nullptr == exception_handler_->GetHandlerMethod());
      }
      return false;  // End stack walk.
    }
    if (method->IsRuntimeMethod()) {
      // Ignore callee save method.
      DCHECK(method->IsCalleeSaveMethod());
      return true;
    }
    return HandleTryItems(method);
  }

 private:
  bool HandleTryItems(ArtMethod* method)
      SHARED_REQUIRES(Locks::mutator_lock_) {
    uint32_t dex_pc = DexFile::kDexNoIndex;
    if (!method->IsNative()) {
      dex_pc = GetDexPc();
    }
    if (dex_pc != DexFile::kDexNoIndex) {
      bool clear_exception = false;
      StackHandleScope<1> hs(GetThread());
      Handle<mirror::Class> to_find(hs.NewHandle((*exception_)->GetClass()));
      uint32_t found_dex_pc = method->FindCatchBlock(to_find, dex_pc, &clear_exception);
      exception_handler_->SetClearException(clear_exception);
      if (found_dex_pc != DexFile::kDexNoIndex) {
        exception_handler_->SetHandlerMethod(method);
        exception_handler_->SetHandlerDexPc(found_dex_pc);
        exception_handler_->SetHandlerQuickFramePc(
            method->ToNativeQuickPc(found_dex_pc, /* is_catch_handler */ true));
        exception_handler_->SetHandlerQuickFrame(GetCurrentQuickFrame());
        return false;  // End stack walk.
      } else if (UNLIKELY(GetThread()->HasDebuggerShadowFrames())) {
        // We are going to unwind this frame. Did we prepare a shadow frame for debugging?
        size_t frame_id = GetFrameId();
        ShadowFrame* frame = GetThread()->FindDebuggerShadowFrame(frame_id);
        if (frame != nullptr) {
          // We will not execute this shadow frame so we can safely deallocate it.
          GetThread()->RemoveDebuggerShadowFrameMapping(frame_id);
          ShadowFrame::DeleteDeoptimizedFrame(frame);
        }
      }
    }
    return true;  // Continue stack walk.
  }

  // The exception we're looking for the catch block of.
  Handle<mirror::Throwable>* exception_;
  // The quick exception handler we're visiting for.
  QuickExceptionHandler* const exception_handler_;

  DISALLOW_COPY_AND_ASSIGN(CatchBlockStackVisitor);
};

void QuickExceptionHandler::FindCatch(mirror::Throwable* exception) {
  DCHECK(!is_deoptimization_);
  if (kDebugExceptionDelivery) {
    mirror::String* msg = exception->GetDetailMessage();
    std::string str_msg(msg != nullptr ? msg->ToModifiedUtf8() : "");
    self_->DumpStack(LOG(INFO) << "Delivering exception: " << PrettyTypeOf(exception)
                     << ": " << str_msg << "\n");
  }
  StackHandleScope<1> hs(self_);
  Handle<mirror::Throwable> exception_ref(hs.NewHandle(exception));

  // Walk the stack to find catch handler.
  CatchBlockStackVisitor visitor(self_, context_, &exception_ref, this);
  visitor.WalkStack(true);

  if (kDebugExceptionDelivery) {
    if (*handler_quick_frame_ == nullptr) {
      LOG(INFO) << "Handler is upcall";
    }
    if (handler_method_ != nullptr) {
      const DexFile& dex_file = *handler_method_->GetDeclaringClass()->GetDexCache()->GetDexFile();
      int line_number = dex_file.GetLineNumFromPC(handler_method_, handler_dex_pc_);
      LOG(INFO) << "Handler: " << PrettyMethod(handler_method_) << " (line: " << line_number << ")";
    }
  }
  if (clear_exception_) {
    // Exception was cleared as part of delivery.
    DCHECK(!self_->IsExceptionPending());
  } else {
    // Put exception back in root set with clear throw location.
    self_->SetException(exception_ref.Get());
  }
  // If the handler is in optimized code, we need to set the catch environment.
  if (*handler_quick_frame_ != nullptr &&
      handler_method_ != nullptr &&
      handler_method_->IsOptimized(sizeof(void*))) {
    SetCatchEnvironmentForOptimizedHandler(&visitor);
  }
}

static VRegKind ToVRegKind(DexRegisterLocation::Kind kind) {
  // Slightly hacky since we cannot map DexRegisterLocationKind and VRegKind
  // one to one. However, StackVisitor::GetVRegFromOptimizedCode only needs to
  // distinguish between core/FPU registers and low/high bits on 64-bit.
  switch (kind) {
    case DexRegisterLocation::Kind::kConstant:
    case DexRegisterLocation::Kind::kInStack:
      // VRegKind is ignored.
      return VRegKind::kUndefined;

    case DexRegisterLocation::Kind::kInRegister:
      // Selects core register. For 64-bit registers, selects low 32 bits.
      return VRegKind::kLongLoVReg;

    case DexRegisterLocation::Kind::kInRegisterHigh:
      // Selects core register. For 64-bit registers, selects high 32 bits.
      return VRegKind::kLongHiVReg;

    case DexRegisterLocation::Kind::kInFpuRegister:
      // Selects FPU register. For 64-bit registers, selects low 32 bits.
      return VRegKind::kDoubleLoVReg;

    case DexRegisterLocation::Kind::kInFpuRegisterHigh:
      // Selects FPU register. For 64-bit registers, selects high 32 bits.
      return VRegKind::kDoubleHiVReg;

    default:
      LOG(FATAL) << "Unexpected vreg location "
                 << DexRegisterLocation::PrettyDescriptor(kind);
      UNREACHABLE();
  }
}

void QuickExceptionHandler::SetCatchEnvironmentForOptimizedHandler(StackVisitor* stack_visitor) {
  DCHECK(!is_deoptimization_);
  DCHECK(*handler_quick_frame_ != nullptr) << "Method should not be called on upcall exceptions";
  DCHECK(handler_method_ != nullptr && handler_method_->IsOptimized(sizeof(void*)));

  if (kDebugExceptionDelivery) {
    self_->DumpStack(LOG(INFO) << "Setting catch phis: ");
  }

  const size_t number_of_vregs = handler_method_->GetCodeItem()->registers_size_;
  CodeInfo code_info = handler_method_->GetOptimizedCodeInfo();
  StackMapEncoding encoding = code_info.ExtractEncoding();

  // Find stack map of the throwing instruction.
  StackMap throw_stack_map =
      code_info.GetStackMapForNativePcOffset(stack_visitor->GetNativePcOffset(), encoding);
  DCHECK(throw_stack_map.IsValid());
  DexRegisterMap throw_vreg_map =
      code_info.GetDexRegisterMapOf(throw_stack_map, encoding, number_of_vregs);

  // Find stack map of the catch block.
  StackMap catch_stack_map = code_info.GetCatchStackMapForDexPc(GetHandlerDexPc(), encoding);
  DCHECK(catch_stack_map.IsValid());
  DexRegisterMap catch_vreg_map =
      code_info.GetDexRegisterMapOf(catch_stack_map, encoding, number_of_vregs);

  // Copy values between them.
  for (uint16_t vreg = 0; vreg < number_of_vregs; ++vreg) {
    DexRegisterLocation::Kind catch_location =
        catch_vreg_map.GetLocationKind(vreg, number_of_vregs, code_info, encoding);
    if (catch_location == DexRegisterLocation::Kind::kNone) {
      continue;
    }
    DCHECK(catch_location == DexRegisterLocation::Kind::kInStack);

    // Get vreg value from its current location.
    uint32_t vreg_value;
    VRegKind vreg_kind = ToVRegKind(throw_vreg_map.GetLocationKind(vreg,
                                                                   number_of_vregs,
                                                                   code_info,
                                                                   encoding));
    bool get_vreg_success = stack_visitor->GetVReg(stack_visitor->GetMethod(),
                                                   vreg,
                                                   vreg_kind,
                                                   &vreg_value);
    CHECK(get_vreg_success) << "VReg " << vreg << " was optimized out ("
                            << "method=" << PrettyMethod(stack_visitor->GetMethod()) << ", "
                            << "dex_pc=" << stack_visitor->GetDexPc() << ", "
                            << "native_pc_offset=" << stack_visitor->GetNativePcOffset() << ")";

    // Copy value to the catch phi's stack slot.
    int32_t slot_offset = catch_vreg_map.GetStackOffsetInBytes(vreg,
                                                               number_of_vregs,
                                                               code_info,
                                                               encoding);
    ArtMethod** frame_top = stack_visitor->GetCurrentQuickFrame();
    uint8_t* slot_address = reinterpret_cast<uint8_t*>(frame_top) + slot_offset;
    uint32_t* slot_ptr = reinterpret_cast<uint32_t*>(slot_address);
    *slot_ptr = vreg_value;
  }
}

// Prepares deoptimization.
class DeoptimizeStackVisitor FINAL : public StackVisitor {
 public:
  DeoptimizeStackVisitor(Thread* self, Context* context, QuickExceptionHandler* exception_handler)
      SHARED_REQUIRES(Locks::mutator_lock_)
      : StackVisitor(self, context, StackVisitor::StackWalkKind::kIncludeInlinedFrames),
        exception_handler_(exception_handler),
        prev_shadow_frame_(nullptr),
        stacked_shadow_frame_pushed_(false) {
  }

  bool VisitFrame() OVERRIDE SHARED_REQUIRES(Locks::mutator_lock_) {
    exception_handler_->SetHandlerFrameDepth(GetFrameDepth());
    ArtMethod* method = GetMethod();
    if (method == nullptr) {
      // This is the upcall, we remember the frame and last pc so that we may long jump to them.
      exception_handler_->SetHandlerQuickFramePc(GetCurrentQuickFramePc());
      exception_handler_->SetHandlerQuickFrame(GetCurrentQuickFrame());
      if (!stacked_shadow_frame_pushed_) {
        // In case there is no deoptimized shadow frame for this upcall, we still
        // need to push a nullptr to the stack since there is always a matching pop after
        // the long jump.
        GetThread()->PushStackedShadowFrame(nullptr,
                                            StackedShadowFrameType::kDeoptimizationShadowFrame);
        stacked_shadow_frame_pushed_ = true;
      }
      return false;  // End stack walk.
    } else if (method->IsRuntimeMethod()) {
      // Ignore callee save method.
      DCHECK(method->IsCalleeSaveMethod());
      return true;
    } else if (method->IsNative()) {
      // If we return from JNI with a pending exception and want to deoptimize, we need to skip
      // the native method.
      // The top method is a runtime method, the native method comes next.
      CHECK_EQ(GetFrameDepth(), 1U);
      return true;
    } else {
      return HandleDeoptimization(method);
    }
  }

 private:
  static VRegKind GetVRegKind(uint16_t reg, const std::vector<int32_t>& kinds) {
    return static_cast<VRegKind>(kinds.at(reg * 2));
  }

  bool HandleDeoptimization(ArtMethod* m) SHARED_REQUIRES(Locks::mutator_lock_) {
    const DexFile::CodeItem* code_item = m->GetCodeItem();
    CHECK(code_item != nullptr) << "No code item for " << PrettyMethod(m);
    uint16_t num_regs = code_item->registers_size_;
    uint32_t dex_pc = GetDexPc();
    StackHandleScope<2> hs(GetThread());  // Dex cache, class loader and method.
    mirror::Class* declaring_class = m->GetDeclaringClass();
    Handle<mirror::DexCache> h_dex_cache(hs.NewHandle(declaring_class->GetDexCache()));
    Handle<mirror::ClassLoader> h_class_loader(hs.NewHandle(declaring_class->GetClassLoader()));
    verifier::MethodVerifier verifier(GetThread(), h_dex_cache->GetDexFile(), h_dex_cache,
                                      h_class_loader, &m->GetClassDef(), code_item,
                                      m->GetDexMethodIndex(), m, m->GetAccessFlags(), true, true,
                                      true, true);
    bool verifier_success = verifier.Verify();
    CHECK(verifier_success) << PrettyMethod(m);
    // Check if a shadow frame already exists for debugger's set-local-value purpose.
    const size_t frame_id = GetFrameId();
    ShadowFrame* new_frame = GetThread()->FindDebuggerShadowFrame(frame_id);
    const bool* updated_vregs;
    if (new_frame == nullptr) {
      new_frame = ShadowFrame::CreateDeoptimizedFrame(num_regs, nullptr, m, dex_pc);
      updated_vregs = nullptr;
    } else {
      updated_vregs = GetThread()->GetUpdatedVRegFlags(frame_id);
      DCHECK(updated_vregs != nullptr);
    }
    {
      ScopedStackedShadowFramePusher pusher(GetThread(), new_frame,
                                            StackedShadowFrameType::kShadowFrameUnderConstruction);
      const std::vector<int32_t> kinds(verifier.DescribeVRegs(dex_pc));

      // Markers for dead values, used when the verifier knows a Dex register is undefined,
      // or when the compiler knows the register has not been initialized, or is not used
      // anymore in the method.
      static constexpr uint32_t kDeadValue = 0xEBADDE09;
      static constexpr uint64_t kLongDeadValue = 0xEBADDE09EBADDE09;
      for (uint16_t reg = 0; reg < num_regs; ++reg) {
        if (updated_vregs != nullptr && updated_vregs[reg]) {
          // Keep the value set by debugger.
          continue;
        }
        VRegKind kind = GetVRegKind(reg, kinds);
        switch (kind) {
          case kUndefined:
            new_frame->SetVReg(reg, kDeadValue);
            break;
          case kConstant:
            new_frame->SetVReg(reg, kinds.at((reg * 2) + 1));
            break;
          case kReferenceVReg: {
            uint32_t value = 0;
            // Check IsReferenceVReg in case the compiled GC map doesn't agree with the verifier.
            // We don't want to copy a stale reference into the shadow frame as a reference.
            // b/20736048
            if (GetVReg(m, reg, kind, &value) && IsReferenceVReg(m, reg)) {
              new_frame->SetVRegReference(reg, reinterpret_cast<mirror::Object*>(value));
            } else {
              new_frame->SetVReg(reg, kDeadValue);
            }
            break;
          }
          case kLongLoVReg:
            if (GetVRegKind(reg + 1, kinds) == kLongHiVReg) {
              // Treat it as a "long" register pair.
              uint64_t value = 0;
              if (GetVRegPair(m, reg, kLongLoVReg, kLongHiVReg, &value)) {
                new_frame->SetVRegLong(reg, value);
              } else {
                new_frame->SetVRegLong(reg, kLongDeadValue);
              }
            } else {
              uint32_t value = 0;
              if (GetVReg(m, reg, kind, &value)) {
                new_frame->SetVReg(reg, value);
              } else {
                new_frame->SetVReg(reg, kDeadValue);
              }
            }
            break;
          case kLongHiVReg:
            if (GetVRegKind(reg - 1, kinds) == kLongLoVReg) {
              // Nothing to do: we treated it as a "long" register pair.
            } else {
              uint32_t value = 0;
              if (GetVReg(m, reg, kind, &value)) {
                new_frame->SetVReg(reg, value);
              } else {
                new_frame->SetVReg(reg, kDeadValue);
              }
            }
            break;
          case kDoubleLoVReg:
            if (GetVRegKind(reg + 1, kinds) == kDoubleHiVReg) {
              uint64_t value = 0;
              if (GetVRegPair(m, reg, kDoubleLoVReg, kDoubleHiVReg, &value)) {
                // Treat it as a "double" register pair.
                new_frame->SetVRegLong(reg, value);
              } else {
                new_frame->SetVRegLong(reg, kLongDeadValue);
              }
            } else {
              uint32_t value = 0;
              if (GetVReg(m, reg, kind, &value)) {
                new_frame->SetVReg(reg, value);
              } else {
                new_frame->SetVReg(reg, kDeadValue);
              }
            }
            break;
          case kDoubleHiVReg:
            if (GetVRegKind(reg - 1, kinds) == kDoubleLoVReg) {
              // Nothing to do: we treated it as a "double" register pair.
            } else {
              uint32_t value = 0;
              if (GetVReg(m, reg, kind, &value)) {
                new_frame->SetVReg(reg, value);
              } else {
                new_frame->SetVReg(reg, kDeadValue);
              }
            }
            break;
          default:
            uint32_t value = 0;
            if (GetVReg(m, reg, kind, &value)) {
              new_frame->SetVReg(reg, value);
            } else {
              new_frame->SetVReg(reg, kDeadValue);
            }
            break;
        }
      }
    }
    if (updated_vregs != nullptr) {
      // Calling Thread::RemoveDebuggerShadowFrameMapping will also delete the updated_vregs
      // array so this must come after we processed the frame.
      GetThread()->RemoveDebuggerShadowFrameMapping(frame_id);
      DCHECK(GetThread()->FindDebuggerShadowFrame(frame_id) == nullptr);
    }
    if (prev_shadow_frame_ != nullptr) {
      prev_shadow_frame_->SetLink(new_frame);
    } else {
      // Will be popped after the long jump after DeoptimizeStack(),
      // right before interpreter::EnterInterpreterFromDeoptimize().
      stacked_shadow_frame_pushed_ = true;
      GetThread()->PushStackedShadowFrame(new_frame,
                                          StackedShadowFrameType::kDeoptimizationShadowFrame);
    }
    prev_shadow_frame_ = new_frame;
    return true;
  }

  QuickExceptionHandler* const exception_handler_;
  ShadowFrame* prev_shadow_frame_;
  bool stacked_shadow_frame_pushed_;

  DISALLOW_COPY_AND_ASSIGN(DeoptimizeStackVisitor);
};

void QuickExceptionHandler::DeoptimizeStack() {
  DCHECK(is_deoptimization_);
  if (kDebugExceptionDelivery) {
    self_->DumpStack(LOG(INFO) << "Deoptimizing: ");
  }

  DeoptimizeStackVisitor visitor(self_, context_, this);
  visitor.WalkStack(true);

  // Restore deoptimization exception
  self_->SetException(Thread::GetDeoptimizationException());
}

// Unwinds all instrumentation stack frame prior to catch handler or upcall.
class InstrumentationStackVisitor : public StackVisitor {
 public:
  InstrumentationStackVisitor(Thread* self, size_t frame_depth)
      SHARED_REQUIRES(Locks::mutator_lock_)
      : StackVisitor(self, nullptr, StackVisitor::StackWalkKind::kIncludeInlinedFrames),
        frame_depth_(frame_depth),
        instrumentation_frames_to_pop_(0) {
    CHECK_NE(frame_depth_, kInvalidFrameDepth);
  }

  bool VisitFrame() SHARED_REQUIRES(Locks::mutator_lock_) {
    size_t current_frame_depth = GetFrameDepth();
    if (current_frame_depth < frame_depth_) {
      CHECK(GetMethod() != nullptr);
      if (UNLIKELY(reinterpret_cast<uintptr_t>(GetQuickInstrumentationExitPc()) == GetReturnPc())) {
        if (!IsInInlinedFrame()) {
          // We do not count inlined frames, because we do not instrument them. The reason we
          // include them in the stack walking is the check against `frame_depth_`, which is
          // given to us by a visitor that visits inlined frames.
          ++instrumentation_frames_to_pop_;
        }
      }
      return true;
    } else {
      // We reached the frame of the catch handler or the upcall.
      return false;
    }
  }

  size_t GetInstrumentationFramesToPop() const {
    return instrumentation_frames_to_pop_;
  }

 private:
  const size_t frame_depth_;
  size_t instrumentation_frames_to_pop_;

  DISALLOW_COPY_AND_ASSIGN(InstrumentationStackVisitor);
};

void QuickExceptionHandler::UpdateInstrumentationStack() {
  if (method_tracing_active_) {
    InstrumentationStackVisitor visitor(self_, handler_frame_depth_);
    visitor.WalkStack(true);

    size_t instrumentation_frames_to_pop = visitor.GetInstrumentationFramesToPop();
    instrumentation::Instrumentation* instrumentation = Runtime::Current()->GetInstrumentation();
    for (size_t i = 0; i < instrumentation_frames_to_pop; ++i) {
      instrumentation->PopMethodForUnwind(self_, is_deoptimization_);
    }
  }
}

void QuickExceptionHandler::DoLongJump() {
  // Place context back on thread so it will be available when we continue.
  self_->ReleaseLongJumpContext(context_);
  context_->SetSP(reinterpret_cast<uintptr_t>(handler_quick_frame_));
  CHECK_NE(handler_quick_frame_pc_, 0u);
  context_->SetPC(handler_quick_frame_pc_);
  context_->SmashCallerSaves();
  context_->DoLongJump();
  UNREACHABLE();
}

}  // namespace art
