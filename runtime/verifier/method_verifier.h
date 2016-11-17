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

#ifndef ART_RUNTIME_VERIFIER_METHOD_VERIFIER_H_
#define ART_RUNTIME_VERIFIER_METHOD_VERIFIER_H_

#include <memory>
#include <sstream>
#include <vector>

#include "base/arena_allocator.h"
#include "base/macros.h"
#include "base/scoped_arena_containers.h"
#include "base/stl_util.h"
#include "base/value_object.h"
#include "dex_file.h"
#include "dex_file_types.h"
#include "handle.h"
#include "instruction_flags.h"
#include "method_reference.h"
#include "register_line.h"
#include "reg_type_cache.h"
#include "verifier_log_mode.h"

namespace art {

class CompilerCallbacks;
class Instruction;
struct ReferenceMap2Visitor;
class Thread;
class VariableIndentationOutputStream;

namespace verifier {

class MethodVerifier;
class RegisterLine;
using RegisterLineArenaUniquePtr = std::unique_ptr<RegisterLine, RegisterLineArenaDelete>;
class RegType;

/*
 * "Direct" and "virtual" methods are stored independently. The type of call used to invoke the
 * method determines which list we search, and whether we travel up into superclasses.
 *
 * (<clinit>, <init>, and methods declared "private" or "static" are stored in the "direct" list.
 * All others are stored in the "virtual" list.)
 */
enum MethodType {
  METHOD_UNKNOWN  = 0,
  METHOD_DIRECT,      // <init>, private
  METHOD_STATIC,      // static
  METHOD_VIRTUAL,     // virtual
  METHOD_SUPER,       // super
  METHOD_INTERFACE    // interface
};
std::ostream& operator<<(std::ostream& os, const MethodType& rhs);

/*
 * An enumeration of problems that can turn up during verification.
 * Both VERIFY_ERROR_BAD_CLASS_SOFT and VERIFY_ERROR_BAD_CLASS_HARD denote failures that cause
 * the entire class to be rejected. However, VERIFY_ERROR_BAD_CLASS_SOFT denotes a soft failure
 * that can potentially be corrected, and the verifier will try again at runtime.
 * VERIFY_ERROR_BAD_CLASS_HARD denotes a hard failure that can't be corrected, and will cause
 * the class to remain uncompiled. Other errors denote verification errors that cause bytecode
 * to be rewritten to fail at runtime.
 */
enum VerifyError {
  VERIFY_ERROR_BAD_CLASS_HARD = 1,        // VerifyError; hard error that skips compilation.
  VERIFY_ERROR_BAD_CLASS_SOFT = 2,        // VerifyError; soft error that verifies again at runtime.

  VERIFY_ERROR_NO_CLASS = 4,              // NoClassDefFoundError.
  VERIFY_ERROR_NO_FIELD = 8,              // NoSuchFieldError.
  VERIFY_ERROR_NO_METHOD = 16,            // NoSuchMethodError.
  VERIFY_ERROR_ACCESS_CLASS = 32,         // IllegalAccessError.
  VERIFY_ERROR_ACCESS_FIELD = 64,         // IllegalAccessError.
  VERIFY_ERROR_ACCESS_METHOD = 128,       // IllegalAccessError.
  VERIFY_ERROR_CLASS_CHANGE = 256,        // IncompatibleClassChangeError.
  VERIFY_ERROR_INSTANTIATION = 512,       // InstantiationError.
  // For opcodes that don't have complete verifier support,  we need a way to continue
  // execution at runtime without attempting to re-verify (since we know it will fail no
  // matter what). Instead, run as the interpreter in a special "do access checks" mode
  // which will perform verifier-like checking on the fly.
  VERIFY_ERROR_FORCE_INTERPRETER = 1024,  // Skip the verification phase at runtime;
                                          // force the interpreter to do access checks.
                                          // (sets a soft fail at compile time).
  VERIFY_ERROR_LOCKING = 2048,            // Could not guarantee balanced locking. This should be
                                          // punted to the interpreter with access checks.
};
std::ostream& operator<<(std::ostream& os, const VerifyError& rhs);

// We don't need to store the register data for many instructions, because we either only need
// it at branch points (for verification) or GC points and branches (for verification +
// type-precise register analysis).
enum RegisterTrackingMode {
  kTrackRegsBranches,
  kTrackCompilerInterestPoints,
  kTrackRegsAll,
};

// A mapping from a dex pc to the register line statuses as they are immediately prior to the
// execution of that instruction.
class PcToRegisterLineTable {
 public:
  explicit PcToRegisterLineTable(ScopedArenaAllocator& arena);
  ~PcToRegisterLineTable();

  // Initialize the RegisterTable. Every instruction address can have a different set of information
  // about what's in which register, but for verification purposes we only need to store it at
  // branch target addresses (because we merge into that).
  void Init(RegisterTrackingMode mode, InstructionFlags* flags, uint32_t insns_size,
            uint16_t registers_size, MethodVerifier* verifier);

  RegisterLine* GetLine(size_t idx) const {
    return register_lines_[idx].get();
  }

 private:
  ScopedArenaVector<RegisterLineArenaUniquePtr> register_lines_;

  DISALLOW_COPY_AND_ASSIGN(PcToRegisterLineTable);
};

// The verifier
class MethodVerifier {
 public:
  enum FailureKind {
    kNoFailure,
    kSoftFailure,
    kHardFailure,
  };

  static bool CanCompilerHandleVerificationFailure(uint32_t encountered_failure_types) {
    constexpr uint32_t unresolved_mask = verifier::VerifyError::VERIFY_ERROR_NO_CLASS
        | verifier::VerifyError::VERIFY_ERROR_ACCESS_CLASS
        | verifier::VerifyError::VERIFY_ERROR_ACCESS_FIELD
        | verifier::VerifyError::VERIFY_ERROR_ACCESS_METHOD;
    return (encountered_failure_types & (~unresolved_mask)) == 0;
  }

  // Verify a class. Returns "kNoFailure" on success.
  static FailureKind VerifyClass(Thread* self,
                                 mirror::Class* klass,
                                 CompilerCallbacks* callbacks,
                                 bool allow_soft_failures,
                                 HardFailLogMode log_level,
                                 std::string* error)
      REQUIRES_SHARED(Locks::mutator_lock_);
  static FailureKind VerifyClass(Thread* self,
                                 const DexFile* dex_file,
                                 Handle<mirror::DexCache> dex_cache,
                                 Handle<mirror::ClassLoader> class_loader,
                                 const DexFile::ClassDef& class_def,
                                 CompilerCallbacks* callbacks,
                                 bool allow_soft_failures,
                                 HardFailLogMode log_level,
                                 std::string* error)
      REQUIRES_SHARED(Locks::mutator_lock_);

  static MethodVerifier* VerifyMethodAndDump(Thread* self,
                                             VariableIndentationOutputStream* vios,
                                             uint32_t method_idx,
                                             const DexFile* dex_file,
                                             Handle<mirror::DexCache> dex_cache,
                                             Handle<mirror::ClassLoader> class_loader,
                                             const DexFile::ClassDef& class_def,
                                             const DexFile::CodeItem* code_item, ArtMethod* method,
                                             uint32_t method_access_flags)
      REQUIRES_SHARED(Locks::mutator_lock_);

  uint8_t EncodePcToReferenceMapData() const;

  const DexFile& GetDexFile() const {
    DCHECK(dex_file_ != nullptr);
    return *dex_file_;
  }

  uint32_t DexFileVersion() const {
    return dex_file_->GetVersion();
  }

  RegTypeCache* GetRegTypeCache() {
    return &reg_types_;
  }

  // Log a verification failure.
  std::ostream& Fail(VerifyError error);

  // Log for verification information.
  std::ostream& LogVerifyInfo();

  // Dump the failures encountered by the verifier.
  std::ostream& DumpFailures(std::ostream& os);

  // Dump the state of the verifier, namely each instruction, what flags are set on it, register
  // information
  void Dump(std::ostream& os) REQUIRES_SHARED(Locks::mutator_lock_);
  void Dump(VariableIndentationOutputStream* vios) REQUIRES_SHARED(Locks::mutator_lock_);

  // Fills 'monitor_enter_dex_pcs' with the dex pcs of the monitor-enter instructions corresponding
  // to the locks held at 'dex_pc' in method 'm'.
  static void FindLocksAtDexPc(ArtMethod* m, uint32_t dex_pc,
                               std::vector<uint32_t>* monitor_enter_dex_pcs)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Returns the accessed field corresponding to the quick instruction's field
  // offset at 'dex_pc' in method 'm'.
  static ArtField* FindAccessedFieldAtDexPc(ArtMethod* m, uint32_t dex_pc)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Returns the invoked method corresponding to the quick instruction's vtable
  // index at 'dex_pc' in method 'm'.
  static ArtMethod* FindInvokedMethodAtDexPc(ArtMethod* m, uint32_t dex_pc)
      REQUIRES_SHARED(Locks::mutator_lock_);

  static void Init() REQUIRES_SHARED(Locks::mutator_lock_);
  static void Shutdown();

  bool CanLoadClasses() const {
    return can_load_classes_;
  }

  ~MethodVerifier();

  // Run verification on the method. Returns true if verification completes and false if the input
  // has an irrecoverable corruption.
  bool Verify() REQUIRES_SHARED(Locks::mutator_lock_);

  // Describe VRegs at the given dex pc.
  std::vector<int32_t> DescribeVRegs(uint32_t dex_pc);

  static void VisitStaticRoots(RootVisitor* visitor)
      REQUIRES_SHARED(Locks::mutator_lock_);
  void VisitRoots(RootVisitor* visitor, const RootInfo& roots)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Accessors used by the compiler via CompilerCallback
  const DexFile::CodeItem* CodeItem() const;
  RegisterLine* GetRegLine(uint32_t dex_pc);
  ALWAYS_INLINE const InstructionFlags& GetInstructionFlags(size_t index) const;
  ALWAYS_INLINE InstructionFlags& GetInstructionFlags(size_t index);
  mirror::ClassLoader* GetClassLoader() REQUIRES_SHARED(Locks::mutator_lock_);
  mirror::DexCache* GetDexCache() REQUIRES_SHARED(Locks::mutator_lock_);
  ArtMethod* GetMethod() const REQUIRES_SHARED(Locks::mutator_lock_);
  MethodReference GetMethodReference() const;
  uint32_t GetAccessFlags() const;
  bool HasCheckCasts() const;
  bool HasVirtualOrInterfaceInvokes() const;
  bool HasFailures() const;
  bool HasInstructionThatWillThrow() const {
    return have_any_pending_runtime_throw_failure_;
  }

  const RegType& ResolveCheckedClass(dex::TypeIndex class_idx)
      REQUIRES_SHARED(Locks::mutator_lock_);
  // Returns the method of a quick invoke or null if it cannot be found.
  ArtMethod* GetQuickInvokedMethod(const Instruction* inst, RegisterLine* reg_line,
                                           bool is_range, bool allow_failure)
      REQUIRES_SHARED(Locks::mutator_lock_);
  // Returns the access field of a quick field access (iget/iput-quick) or null
  // if it cannot be found.
  ArtField* GetQuickFieldAccess(const Instruction* inst, RegisterLine* reg_line)
      REQUIRES_SHARED(Locks::mutator_lock_);

  uint32_t GetEncounteredFailureTypes() {
    return encountered_failure_types_;
  }

  bool IsInstanceConstructor() const {
    return IsConstructor() && !IsStatic();
  }

  ScopedArenaAllocator& GetArena() {
    return arena_;
  }

 private:
  MethodVerifier(Thread* self,
                 const DexFile* dex_file,
                 Handle<mirror::DexCache> dex_cache,
                 Handle<mirror::ClassLoader> class_loader,
                 const DexFile::ClassDef& class_def,
                 const DexFile::CodeItem* code_item,
                 uint32_t method_idx,
                 ArtMethod* method,
                 uint32_t access_flags,
                 bool can_load_classes,
                 bool allow_soft_failures,
                 bool need_precise_constants,
                 bool verify_to_dump,
                 bool allow_thread_suspension)
      REQUIRES_SHARED(Locks::mutator_lock_);

  void UninstantiableError(const char* descriptor);
  static bool IsInstantiableOrPrimitive(mirror::Class* klass) REQUIRES_SHARED(Locks::mutator_lock_);

  // Is the method being verified a constructor? See the comment on the field.
  bool IsConstructor() const {
    return is_constructor_;
  }

  // Is the method verified static?
  bool IsStatic() const {
    return (method_access_flags_ & kAccStatic) != 0;
  }

  // Adds the given string to the beginning of the last failure message.
  void PrependToLastFailMessage(std::string);

  // Adds the given string to the end of the last failure message.
  void AppendToLastFailMessage(const std::string& append);

  // Verification result for method(s). Includes a (maximum) failure kind, and (the union of)
  // all failure types.
  struct FailureData : ValueObject {
    FailureKind kind = kNoFailure;
    uint32_t types = 0U;

    // Merge src into this. Uses the most severe failure kind, and the union of types.
    void Merge(const FailureData& src);
  };

  // Verify all direct or virtual methods of a class. The method assumes that the iterator is
  // positioned correctly, and the iterator will be updated.
  template <bool kDirect>
  static FailureData VerifyMethods(Thread* self,
                                   ClassLinker* linker,
                                   const DexFile* dex_file,
                                   const DexFile::ClassDef& class_def,
                                   ClassDataItemIterator* it,
                                   Handle<mirror::DexCache> dex_cache,
                                   Handle<mirror::ClassLoader> class_loader,
                                   CompilerCallbacks* callbacks,
                                   bool allow_soft_failures,
                                   HardFailLogMode log_level,
                                   bool need_precise_constants,
                                   std::string* error_string)
      REQUIRES_SHARED(Locks::mutator_lock_);

  /*
   * Perform verification on a single method.
   *
   * We do this in three passes:
   *  (1) Walk through all code units, determining instruction locations,
   *      widths, and other characteristics.
   *  (2) Walk through all code units, performing static checks on
   *      operands.
   *  (3) Iterate through the method, checking type safety and looking
   *      for code flow problems.
   */
  static FailureData VerifyMethod(Thread* self,
                                  uint32_t method_idx,
                                  const DexFile* dex_file,
                                  Handle<mirror::DexCache> dex_cache,
                                  Handle<mirror::ClassLoader> class_loader,
                                  const DexFile::ClassDef& class_def_idx,
                                  const DexFile::CodeItem* code_item,
                                  ArtMethod* method,
                                  uint32_t method_access_flags,
                                  CompilerCallbacks* callbacks,
                                  bool allow_soft_failures,
                                  HardFailLogMode log_level,
                                  bool need_precise_constants,
                                  std::string* hard_failure_msg)
      REQUIRES_SHARED(Locks::mutator_lock_);

  void FindLocksAtDexPc() REQUIRES_SHARED(Locks::mutator_lock_);

  ArtField* FindAccessedFieldAtDexPc(uint32_t dex_pc)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ArtMethod* FindInvokedMethodAtDexPc(uint32_t dex_pc)
      REQUIRES_SHARED(Locks::mutator_lock_);

  SafeMap<uint32_t, std::set<uint32_t>>& FindStringInitMap()
      REQUIRES_SHARED(Locks::mutator_lock_);

  /*
   * Compute the width of the instruction at each address in the instruction stream, and store it in
   * insn_flags_. Addresses that are in the middle of an instruction, or that are part of switch
   * table data, are not touched (so the caller should probably initialize "insn_flags" to zero).
   *
   * The "new_instance_count_" and "monitor_enter_count_" fields in vdata are also set.
   *
   * Performs some static checks, notably:
   * - opcode of first instruction begins at index 0
   * - only documented instructions may appear
   * - each instruction follows the last
   * - last byte of last instruction is at (code_length-1)
   *
   * Logs an error and returns "false" on failure.
   */
  bool ComputeWidthsAndCountOps();

  /*
   * Set the "in try" flags for all instructions protected by "try" statements. Also sets the
   * "branch target" flags for exception handlers.
   *
   * Call this after widths have been set in "insn_flags".
   *
   * Returns "false" if something in the exception table looks fishy, but we're expecting the
   * exception table to be somewhat sane.
   */
  bool ScanTryCatchBlocks() REQUIRES_SHARED(Locks::mutator_lock_);

  /*
   * Perform static verification on all instructions in a method.
   *
   * Walks through instructions in a method calling VerifyInstruction on each.
   */
  bool VerifyInstructions();

  /*
   * Perform static verification on an instruction.
   *
   * As a side effect, this sets the "branch target" flags in InsnFlags.
   *
   * "(CF)" items are handled during code-flow analysis.
   *
   * v3 4.10.1
   * - target of each jump and branch instruction must be valid
   * - targets of switch statements must be valid
   * - operands referencing constant pool entries must be valid
   * - (CF) operands of getfield, putfield, getstatic, putstatic must be valid
   * - (CF) operands of method invocation instructions must be valid
   * - (CF) only invoke-direct can call a method starting with '<'
   * - (CF) <clinit> must never be called explicitly
   * - operands of instanceof, checkcast, new (and variants) must be valid
   * - new-array[-type] limited to 255 dimensions
   * - can't use "new" on an array class
   * - (?) limit dimensions in multi-array creation
   * - local variable load/store register values must be in valid range
   *
   * v3 4.11.1.2
   * - branches must be within the bounds of the code array
   * - targets of all control-flow instructions are the start of an instruction
   * - register accesses fall within range of allocated registers
   * - (N/A) access to constant pool must be of appropriate type
   * - code does not end in the middle of an instruction
   * - execution cannot fall off the end of the code
   * - (earlier) for each exception handler, the "try" area must begin and
   *   end at the start of an instruction (end can be at the end of the code)
   * - (earlier) for each exception handler, the handler must start at a valid
   *   instruction
   */
  bool VerifyInstruction(const Instruction* inst, uint32_t code_offset);

  /* Ensure that the register index is valid for this code item. */
  bool CheckRegisterIndex(uint32_t idx);

  /* Ensure that the wide register index is valid for this code item. */
  bool CheckWideRegisterIndex(uint32_t idx);

  // Perform static checks on a field Get or set instruction. All we do here is ensure that the
  // field index is in the valid range.
  bool CheckFieldIndex(uint32_t idx);

  // Perform static checks on a method invocation instruction. All we do here is ensure that the
  // method index is in the valid range.
  bool CheckMethodIndex(uint32_t idx);

  // Perform static checks on a "new-instance" instruction. Specifically, make sure the class
  // reference isn't for an array class.
  bool CheckNewInstance(dex::TypeIndex idx);

  /* Ensure that the string index is in the valid range. */
  bool CheckStringIndex(uint32_t idx);

  // Perform static checks on an instruction that takes a class constant. Ensure that the class
  // index is in the valid range.
  bool CheckTypeIndex(dex::TypeIndex idx);

  // Perform static checks on a "new-array" instruction. Specifically, make sure they aren't
  // creating an array of arrays that causes the number of dimensions to exceed 255.
  bool CheckNewArray(dex::TypeIndex idx);

  // Verify an array data table. "cur_offset" is the offset of the fill-array-data instruction.
  bool CheckArrayData(uint32_t cur_offset);

  // Verify that the target of a branch instruction is valid. We don't expect code to jump directly
  // into an exception handler, but it's valid to do so as long as the target isn't a
  // "move-exception" instruction. We verify that in a later stage.
  // The dex format forbids certain instructions from branching to themselves.
  // Updates "insn_flags_", setting the "branch target" flag.
  bool CheckBranchTarget(uint32_t cur_offset);

  // Verify a switch table. "cur_offset" is the offset of the switch instruction.
  // Updates "insn_flags_", setting the "branch target" flag.
  bool CheckSwitchTargets(uint32_t cur_offset);

  // Check the register indices used in a "vararg" instruction, such as invoke-virtual or
  // filled-new-array.
  // - vA holds word count (0-5), args[] have values.
  // There are some tests we don't do here, e.g. we don't try to verify that invoking a method that
  // takes a double is done with consecutive registers. This requires parsing the target method
  // signature, which we will be doing later on during the code flow analysis.
  bool CheckVarArgRegs(uint32_t vA, uint32_t arg[]);

  // Check the register indices used in a "vararg/range" instruction, such as invoke-virtual/range
  // or filled-new-array/range.
  // - vA holds word count, vC holds index of first reg.
  bool CheckVarArgRangeRegs(uint32_t vA, uint32_t vC);

  // Extract the relative offset from a branch instruction.
  // Returns "false" on failure (e.g. this isn't a branch instruction).
  bool GetBranchOffset(uint32_t cur_offset, int32_t* pOffset, bool* pConditional,
                       bool* selfOkay);

  /* Perform detailed code-flow analysis on a single method. */
  bool VerifyCodeFlow() REQUIRES_SHARED(Locks::mutator_lock_);

  // Set the register types for the first instruction in the method based on the method signature.
  // This has the side-effect of validating the signature.
  bool SetTypesFromSignature() REQUIRES_SHARED(Locks::mutator_lock_);

  /*
   * Perform code flow on a method.
   *
   * The basic strategy is as outlined in v3 4.11.1.2: set the "changed" bit on the first
   * instruction, process it (setting additional "changed" bits), and repeat until there are no
   * more.
   *
   * v3 4.11.1.1
   * - (N/A) operand stack is always the same size
   * - operand stack [registers] contain the correct types of values
   * - local variables [registers] contain the correct types of values
   * - methods are invoked with the appropriate arguments
   * - fields are assigned using values of appropriate types
   * - opcodes have the correct type values in operand registers
   * - there is never an uninitialized class instance in a local variable in code protected by an
   *   exception handler (operand stack is okay, because the operand stack is discarded when an
   *   exception is thrown) [can't know what's a local var w/o the debug info -- should fall out of
   *   register typing]
   *
   * v3 4.11.1.2
   * - execution cannot fall off the end of the code
   *
   * (We also do many of the items described in the "static checks" sections, because it's easier to
   * do them here.)
   *
   * We need an array of RegType values, one per register, for every instruction. If the method uses
   * monitor-enter, we need extra data for every register, and a stack for every "interesting"
   * instruction. In theory this could become quite large -- up to several megabytes for a monster
   * function.
   *
   * NOTE:
   * The spec forbids backward branches when there's an uninitialized reference in a register. The
   * idea is to prevent something like this:
   *   loop:
   *     move r1, r0
   *     new-instance r0, MyClass
   *     ...
   *     if-eq rN, loop  // once
   *   initialize r0
   *
   * This leaves us with two different instances, both allocated by the same instruction, but only
   * one is initialized. The scheme outlined in v3 4.11.1.4 wouldn't catch this, so they work around
   * it by preventing backward branches. We achieve identical results without restricting code
   * reordering by specifying that you can't execute the new-instance instruction if a register
   * contains an uninitialized instance created by that same instruction.
   */
  bool CodeFlowVerifyMethod() REQUIRES_SHARED(Locks::mutator_lock_);

  /*
   * Perform verification for a single instruction.
   *
   * This requires fully decoding the instruction to determine the effect it has on registers.
   *
   * Finds zero or more following instructions and sets the "changed" flag if execution at that
   * point needs to be (re-)evaluated. Register changes are merged into "reg_types_" at the target
   * addresses. Does not set or clear any other flags in "insn_flags_".
   */
  bool CodeFlowVerifyInstruction(uint32_t* start_guess)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Perform verification of a new array instruction
  void VerifyNewArray(const Instruction* inst, bool is_filled, bool is_range)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Helper to perform verification on puts of primitive type.
  void VerifyPrimitivePut(const RegType& target_type, const RegType& insn_type,
                          const uint32_t vregA) REQUIRES_SHARED(Locks::mutator_lock_);

  // Perform verification of an aget instruction. The destination register's type will be set to
  // be that of component type of the array unless the array type is unknown, in which case a
  // bottom type inferred from the type of instruction is used. is_primitive is false for an
  // aget-object.
  void VerifyAGet(const Instruction* inst, const RegType& insn_type,
                  bool is_primitive) REQUIRES_SHARED(Locks::mutator_lock_);

  // Perform verification of an aput instruction.
  void VerifyAPut(const Instruction* inst, const RegType& insn_type,
                  bool is_primitive) REQUIRES_SHARED(Locks::mutator_lock_);

  // Lookup instance field and fail for resolution violations
  ArtField* GetInstanceField(const RegType& obj_type, int field_idx)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Lookup static field and fail for resolution violations
  ArtField* GetStaticField(int field_idx) REQUIRES_SHARED(Locks::mutator_lock_);

  // Perform verification of an iget/sget/iput/sput instruction.
  enum class FieldAccessType {  // private
    kAccGet,
    kAccPut
  };
  template <FieldAccessType kAccType>
  void VerifyISFieldAccess(const Instruction* inst, const RegType& insn_type,
                           bool is_primitive, bool is_static)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template <FieldAccessType kAccType>
  void VerifyQuickFieldAccess(const Instruction* inst, const RegType& insn_type, bool is_primitive)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Resolves a class based on an index and performs access checks to ensure the referrer can
  // access the resolved class.
  const RegType& ResolveClassAndCheckAccess(dex::TypeIndex class_idx)
      REQUIRES_SHARED(Locks::mutator_lock_);

  /*
   * For the "move-exception" instruction at "work_insn_idx_", which must be at an exception handler
   * address, determine the Join of all exceptions that can land here. Fails if no matching
   * exception handler can be found or if the Join of exception types fails.
   */
  const RegType& GetCaughtExceptionType()
      REQUIRES_SHARED(Locks::mutator_lock_);

  /*
   * Resolves a method based on an index and performs access checks to ensure
   * the referrer can access the resolved method.
   * Does not throw exceptions.
   */
  ArtMethod* ResolveMethodAndCheckAccess(uint32_t method_idx, MethodType method_type)
      REQUIRES_SHARED(Locks::mutator_lock_);

  /*
   * Verify the arguments to a method. We're executing in "method", making
   * a call to the method reference in vB.
   *
   * If this is a "direct" invoke, we allow calls to <init>. For calls to
   * <init>, the first argument may be an uninitialized reference. Otherwise,
   * calls to anything starting with '<' will be rejected, as will any
   * uninitialized reference arguments.
   *
   * For non-static method calls, this will verify that the method call is
   * appropriate for the "this" argument.
   *
   * The method reference is in vBBBB. The "is_range" parameter determines
   * whether we use 0-4 "args" values or a range of registers defined by
   * vAA and vCCCC.
   *
   * Widening conversions on integers and references are allowed, but
   * narrowing conversions are not.
   *
   * Returns the resolved method on success, null on failure (with *failure
   * set appropriately).
   */
  ArtMethod* VerifyInvocationArgs(const Instruction* inst, MethodType method_type, bool is_range)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Similar checks to the above, but on the proto. Will be used when the method cannot be
  // resolved.
  void VerifyInvocationArgsUnresolvedMethod(const Instruction* inst, MethodType method_type,
                                            bool is_range)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template <class T>
  ArtMethod* VerifyInvocationArgsFromIterator(T* it, const Instruction* inst,
                                                      MethodType method_type, bool is_range,
                                                      ArtMethod* res_method)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ArtMethod* VerifyInvokeVirtualQuickArgs(const Instruction* inst, bool is_range)
  REQUIRES_SHARED(Locks::mutator_lock_);

  /*
   * Verify that the target instruction is not "move-exception". It's important that the only way
   * to execute a move-exception is as the first instruction of an exception handler.
   * Returns "true" if all is well, "false" if the target instruction is move-exception.
   */
  bool CheckNotMoveException(const uint16_t* insns, int insn_idx);

  /*
   * Verify that the target instruction is not "move-result". It is important that we cannot
   * branch to move-result instructions, but we have to make this a distinct check instead of
   * adding it to CheckNotMoveException, because it is legal to continue into "move-result"
   * instructions - as long as the previous instruction was an invoke, which is checked elsewhere.
   */
  bool CheckNotMoveResult(const uint16_t* insns, int insn_idx);

  /*
   * Verify that the target instruction is not "move-result" or "move-exception". This is to
   * be used when checking branch and switch instructions, but not instructions that can
   * continue.
   */
  bool CheckNotMoveExceptionOrMoveResult(const uint16_t* insns, int insn_idx);

  /*
  * Control can transfer to "next_insn". Merge the registers from merge_line into the table at
  * next_insn, and set the changed flag on the target address if any of the registers were changed.
  * In the case of fall-through, update the merge line on a change as its the working line for the
  * next instruction.
  * Returns "false" if an error is encountered.
  */
  bool UpdateRegisters(uint32_t next_insn, RegisterLine* merge_line, bool update_merge_line)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Return the register type for the method.
  const RegType& GetMethodReturnType() REQUIRES_SHARED(Locks::mutator_lock_);

  // Get a type representing the declaring class of the method.
  const RegType& GetDeclaringClass() REQUIRES_SHARED(Locks::mutator_lock_);

  InstructionFlags* CurrentInsnFlags();

  const RegType& DetermineCat1Constant(int32_t value, bool precise)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Try to create a register type from the given class. In case a precise type is requested, but
  // the class is not instantiable, a soft error (of type NO_CLASS) will be enqueued and a
  // non-precise reference will be returned.
  // Note: we reuse NO_CLASS as this will throw an exception at runtime, when the failing class is
  //       actually touched.
  const RegType& FromClass(const char* descriptor, mirror::Class* klass, bool precise)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // The thread we're verifying on.
  Thread* const self_;

  // Arena allocator.
  ArenaStack arena_stack_;
  ScopedArenaAllocator arena_;

  RegTypeCache reg_types_;

  PcToRegisterLineTable reg_table_;

  // Storage for the register status we're currently working on.
  RegisterLineArenaUniquePtr work_line_;

  // The address of the instruction we're currently working on, note that this is in 2 byte
  // quantities
  uint32_t work_insn_idx_;

  // Storage for the register status we're saving for later.
  RegisterLineArenaUniquePtr saved_line_;

  const uint32_t dex_method_idx_;  // The method we're working on.
  // Its object representation if known.
  ArtMethod* mirror_method_ GUARDED_BY(Locks::mutator_lock_);
  const uint32_t method_access_flags_;  // Method's access flags.
  const RegType* return_type_;  // Lazily computed return type of the method.
  const DexFile* const dex_file_;  // The dex file containing the method.
  // The dex_cache for the declaring class of the method.
  Handle<mirror::DexCache> dex_cache_ GUARDED_BY(Locks::mutator_lock_);
  // The class loader for the declaring class of the method.
  Handle<mirror::ClassLoader> class_loader_ GUARDED_BY(Locks::mutator_lock_);
  const DexFile::ClassDef& class_def_;  // The class def of the declaring class of the method.
  const DexFile::CodeItem* const code_item_;  // The code item containing the code for the method.
  const RegType* declaring_class_;  // Lazily computed reg type of the method's declaring class.
  // Instruction widths and flags, one entry per code unit.
  // Owned, but not unique_ptr since insn_flags_ are allocated in arenas.
  ArenaUniquePtr<InstructionFlags[]> insn_flags_;
  // The dex PC of a FindLocksAtDexPc request, -1 otherwise.
  uint32_t interesting_dex_pc_;
  // The container into which FindLocksAtDexPc should write the registers containing held locks,
  // null if we're not doing FindLocksAtDexPc.
  std::vector<uint32_t>* monitor_enter_dex_pcs_;

  // The types of any error that occurs.
  std::vector<VerifyError> failures_;
  // Error messages associated with failures.
  std::vector<std::ostringstream*> failure_messages_;
  // Is there a pending hard failure?
  bool have_pending_hard_failure_;
  // Is there a pending runtime throw failure? A runtime throw failure is when an instruction
  // would fail at runtime throwing an exception. Such an instruction causes the following code
  // to be unreachable. This is set by Fail and used to ensure we don't process unreachable
  // instructions that would hard fail the verification.
  // Note: this flag is reset after processing each instruction.
  bool have_pending_runtime_throw_failure_;
  // Is there a pending experimental failure?
  bool have_pending_experimental_failure_;

  // A version of the above that is not reset and thus captures if there were *any* throw failures.
  bool have_any_pending_runtime_throw_failure_;

  // Info message log use primarily for verifier diagnostics.
  std::ostringstream info_messages_;

  // The number of occurrences of specific opcodes.
  size_t new_instance_count_;
  size_t monitor_enter_count_;

  // Bitset of the encountered failure types. Bits are according to the values in VerifyError.
  uint32_t encountered_failure_types_;

  const bool can_load_classes_;

  // Converts soft failures to hard failures when false. Only false when the compiler isn't
  // running and the verifier is called from the class linker.
  const bool allow_soft_failures_;

  // An optimization where instead of generating unique RegTypes for constants we use imprecise
  // constants that cover a range of constants. This isn't good enough for deoptimization that
  // avoids loading from registers in the case of a constant as the dex instruction set lost the
  // notion of whether a value should be in a floating point or general purpose register file.
  const bool need_precise_constants_;

  // Indicates the method being verified contains at least one check-cast or aput-object
  // instruction. Aput-object operations implicitly check for array-store exceptions, similar to
  // check-cast.
  bool has_check_casts_;

  // Indicates the method being verified contains at least one invoke-virtual/range
  // or invoke-interface/range.
  bool has_virtual_or_interface_invokes_;

  // Indicates whether we verify to dump the info. In that case we accept quickened instructions
  // even though we might detect to be a compiler. Should only be set when running
  // VerifyMethodAndDump.
  const bool verify_to_dump_;

  // Whether or not we call AllowThreadSuspension periodically, we want a way to disable this for
  // thread dumping checkpoints since we may get thread suspension at an inopportune time due to
  // FindLocksAtDexPC, resulting in deadlocks.
  const bool allow_thread_suspension_;

  // Whether the method seems to be a constructor. Note that this field exists as we can't trust
  // the flags in the dex file. Some older code does not mark methods named "<init>" and "<clinit>"
  // correctly.
  //
  // Note: this flag is only valid once Verify() has started.
  bool is_constructor_;

  // Link, for the method verifier root linked list.
  MethodVerifier* link_;

  friend class art::Thread;
  friend class VerifierDepsTest;

  DISALLOW_COPY_AND_ASSIGN(MethodVerifier);
};
std::ostream& operator<<(std::ostream& os, const MethodVerifier::FailureKind& rhs);

}  // namespace verifier
}  // namespace art

#endif  // ART_RUNTIME_VERIFIER_METHOD_VERIFIER_H_
