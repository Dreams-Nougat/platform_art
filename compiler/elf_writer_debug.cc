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

#include "elf_writer_debug.h"

#include <unordered_set>

#include "base/casts.h"
#include "compiled_method.h"
#include "driver/compiler_driver.h"
#include "dex_file-inl.h"
#include "dwarf/headers.h"
#include "dwarf/register.h"
#include "elf_builder.h"
#include "oat_writer.h"
#include "utils.h"
#include "stack_map.h"

namespace art {
namespace dwarf {

static void WriteCIE(InstructionSet isa,
                     CFIFormat format,
                     std::vector<uint8_t>* buffer) {
  // Scratch registers should be marked as undefined.  This tells the
  // debugger that its value in the previous frame is not recoverable.
  bool is64bit = Is64BitInstructionSet(isa);
  switch (isa) {
    case kArm:
    case kThumb2: {
      DebugFrameOpCodeWriter<> opcodes;
      opcodes.DefCFA(Reg::ArmCore(13), 0);  // R13(SP).
      // core registers.
      for (int reg = 0; reg < 13; reg++) {
        if (reg < 4 || reg == 12) {
          opcodes.Undefined(Reg::ArmCore(reg));
        } else {
          opcodes.SameValue(Reg::ArmCore(reg));
        }
      }
      // fp registers.
      for (int reg = 0; reg < 32; reg++) {
        if (reg < 16) {
          opcodes.Undefined(Reg::ArmFp(reg));
        } else {
          opcodes.SameValue(Reg::ArmFp(reg));
        }
      }
      auto return_reg = Reg::ArmCore(14);  // R14(LR).
      WriteCIE(is64bit, return_reg, opcodes, format, buffer);
      return;
    }
    case kArm64: {
      DebugFrameOpCodeWriter<> opcodes;
      opcodes.DefCFA(Reg::Arm64Core(31), 0);  // R31(SP).
      // core registers.
      for (int reg = 0; reg < 30; reg++) {
        if (reg < 8 || reg == 16 || reg == 17) {
          opcodes.Undefined(Reg::Arm64Core(reg));
        } else {
          opcodes.SameValue(Reg::Arm64Core(reg));
        }
      }
      // fp registers.
      for (int reg = 0; reg < 32; reg++) {
        if (reg < 8 || reg >= 16) {
          opcodes.Undefined(Reg::Arm64Fp(reg));
        } else {
          opcodes.SameValue(Reg::Arm64Fp(reg));
        }
      }
      auto return_reg = Reg::Arm64Core(30);  // R30(LR).
      WriteCIE(is64bit, return_reg, opcodes, format, buffer);
      return;
    }
    case kMips:
    case kMips64: {
      DebugFrameOpCodeWriter<> opcodes;
      opcodes.DefCFA(Reg::MipsCore(29), 0);  // R29(SP).
      // core registers.
      for (int reg = 1; reg < 26; reg++) {
        if (reg < 16 || reg == 24 || reg == 25) {  // AT, V*, A*, T*.
          opcodes.Undefined(Reg::MipsCore(reg));
        } else {
          opcodes.SameValue(Reg::MipsCore(reg));
        }
      }
      auto return_reg = Reg::MipsCore(31);  // R31(RA).
      WriteCIE(is64bit, return_reg, opcodes, format, buffer);
      return;
    }
    case kX86: {
      // FIXME: Add fp registers once libunwind adds support for them. Bug: 20491296
      constexpr bool generate_opcodes_for_x86_fp = false;
      DebugFrameOpCodeWriter<> opcodes;
      opcodes.DefCFA(Reg::X86Core(4), 4);   // R4(ESP).
      opcodes.Offset(Reg::X86Core(8), -4);  // R8(EIP).
      // core registers.
      for (int reg = 0; reg < 8; reg++) {
        if (reg <= 3) {
          opcodes.Undefined(Reg::X86Core(reg));
        } else if (reg == 4) {
          // Stack pointer.
        } else {
          opcodes.SameValue(Reg::X86Core(reg));
        }
      }
      // fp registers.
      if (generate_opcodes_for_x86_fp) {
        for (int reg = 0; reg < 8; reg++) {
          opcodes.Undefined(Reg::X86Fp(reg));
        }
      }
      auto return_reg = Reg::X86Core(8);  // R8(EIP).
      WriteCIE(is64bit, return_reg, opcodes, format, buffer);
      return;
    }
    case kX86_64: {
      DebugFrameOpCodeWriter<> opcodes;
      opcodes.DefCFA(Reg::X86_64Core(4), 8);  // R4(RSP).
      opcodes.Offset(Reg::X86_64Core(16), -8);  // R16(RIP).
      // core registers.
      for (int reg = 0; reg < 16; reg++) {
        if (reg == 4) {
          // Stack pointer.
        } else if (reg < 12 && reg != 3 && reg != 5) {  // except EBX and EBP.
          opcodes.Undefined(Reg::X86_64Core(reg));
        } else {
          opcodes.SameValue(Reg::X86_64Core(reg));
        }
      }
      // fp registers.
      for (int reg = 0; reg < 16; reg++) {
        if (reg < 12) {
          opcodes.Undefined(Reg::X86_64Fp(reg));
        } else {
          opcodes.SameValue(Reg::X86_64Fp(reg));
        }
      }
      auto return_reg = Reg::X86_64Core(16);  // R16(RIP).
      WriteCIE(is64bit, return_reg, opcodes, format, buffer);
      return;
    }
    case kNone:
      break;
  }
  LOG(FATAL) << "Can not write CIE frame for ISA " << isa;
  UNREACHABLE();
}

template<typename ElfTypes>
void WriteCFISection(ElfBuilder<ElfTypes>* builder,
                     const std::vector<OatWriter::DebugInfo>& method_infos,
                     CFIFormat format) {
  CHECK(format == dwarf::DW_DEBUG_FRAME_FORMAT ||
        format == dwarf::DW_EH_FRAME_FORMAT);
  typedef typename ElfTypes::Addr Elf_Addr;

  std::vector<uint32_t> binary_search_table;
  std::vector<uintptr_t> patch_locations;
  if (format == DW_EH_FRAME_FORMAT) {
    binary_search_table.reserve(2 * method_infos.size());
  } else {
    patch_locations.reserve(method_infos.size());
  }

  // Write .eh_frame/.debug_frame section.
  auto* cfi_section = (format == dwarf::DW_DEBUG_FRAME_FORMAT
                       ? builder->GetDebugFrame()
                       : builder->GetEhFrame());
  {
    cfi_section->Start();
    const bool is64bit = Is64BitInstructionSet(builder->GetIsa());
    const Elf_Addr text_address = builder->GetText()->GetAddress();
    const Elf_Addr cfi_address = cfi_section->GetAddress();
    const Elf_Addr cie_address = cfi_address;
    Elf_Addr buffer_address = cfi_address;
    std::vector<uint8_t> buffer;  // Small temporary buffer.
    WriteCIE(builder->GetIsa(), format, &buffer);
    cfi_section->WriteFully(buffer.data(), buffer.size());
    buffer_address += buffer.size();
    buffer.clear();
    for (const OatWriter::DebugInfo& mi : method_infos) {
      if (!mi.deduped_) {  // Only one FDE per unique address.
        ArrayRef<const uint8_t> opcodes = mi.compiled_method_->GetCFIInfo();
        if (!opcodes.empty()) {
          const Elf_Addr code_address = text_address + mi.low_pc_;
          if (format == DW_EH_FRAME_FORMAT) {
            binary_search_table.push_back(
                dchecked_integral_cast<uint32_t>(code_address));
            binary_search_table.push_back(
                dchecked_integral_cast<uint32_t>(buffer_address));
          }
          WriteFDE(is64bit, cfi_address, cie_address,
                   code_address, mi.high_pc_ - mi.low_pc_,
                   opcodes, format, buffer_address, &buffer,
                   &patch_locations);
          cfi_section->WriteFully(buffer.data(), buffer.size());
          buffer_address += buffer.size();
          buffer.clear();
        }
      }
    }
    cfi_section->End();
  }

  if (format == DW_EH_FRAME_FORMAT) {
    auto* header_section = builder->GetEhFrameHdr();
    header_section->Start();
    uint32_t header_address = dchecked_integral_cast<int32_t>(header_section->GetAddress());
    // Write .eh_frame_hdr section.
    std::vector<uint8_t> buffer;
    Writer<> header(&buffer);
    header.PushUint8(1);  // Version.
    // Encoding of .eh_frame pointer - libunwind does not honor datarel here,
    // so we have to use pcrel which means relative to the pointer's location.
    header.PushUint8(DW_EH_PE_pcrel | DW_EH_PE_sdata4);
    // Encoding of binary search table size.
    header.PushUint8(DW_EH_PE_udata4);
    // Encoding of binary search table addresses - libunwind supports only this
    // specific combination, which means relative to the start of .eh_frame_hdr.
    header.PushUint8(DW_EH_PE_datarel | DW_EH_PE_sdata4);
    // .eh_frame pointer
    header.PushInt32(cfi_section->GetAddress() - (header_address + 4u));
    // Binary search table size (number of entries).
    header.PushUint32(dchecked_integral_cast<uint32_t>(binary_search_table.size()/2));
    header_section->WriteFully(buffer.data(), buffer.size());
    // Binary search table.
    for (size_t i = 0; i < binary_search_table.size(); i++) {
      // Make addresses section-relative since we know the header address now.
      binary_search_table[i] -= header_address;
    }
    header_section->WriteFully(binary_search_table.data(), binary_search_table.size());
    header_section->End();
  } else {
    builder->WritePatches(".debug_frame.oat_patches", &patch_locations);
  }
}

struct CompilationUnit {
  std::vector<const OatWriter::DebugInfo*> methods_;
  size_t debug_line_offset_ = 0;
  uint32_t low_pc_ = 0xFFFFFFFFU;
  uint32_t high_pc_ = 0;
};

// Helper class to write .debug_info and its supporting sections.
template<typename ElfTypes>
class DebugInfoWriter {
  typedef typename ElfTypes::Addr Elf_Addr;

  // Helper class to write one compilation unit.
  // It holds helper methods and temporary state.
  class CompilationUnitWriter {
   public:
    explicit CompilationUnitWriter(DebugInfoWriter* owner)
      : owner_(owner),
        debug_abbrev_offset_(owner_->debug_abbrev_.size()),
        info_(Is64BitInstructionSet(owner_->builder_->GetIsa()), &owner_->debug_abbrev_) {
    }

    void Write(const CompilationUnit& compilation_unit) {
      CHECK(!compilation_unit.methods_.empty());
      const Elf_Addr text_address = owner_->builder_->GetText()->GetAddress();

      std::vector<uint8_t>* debug_str = &owner_->debug_str_;
      info_.StartTag(DW_TAG_compile_unit);
      info_.WriteStrp(DW_AT_producer, "Android dex2oat", debug_str);
      info_.WriteData1(DW_AT_language, DW_LANG_Java);
      info_.WriteAddr(DW_AT_low_pc, text_address + compilation_unit.low_pc_);
      info_.WriteAddr(DW_AT_high_pc, text_address + compilation_unit.high_pc_);
      info_.WriteData4(DW_AT_stmt_list, compilation_unit.debug_line_offset_);

      const char* last_dex_class_desc = nullptr;
      for (auto mi : compilation_unit.methods_) {
        const DexFile* dex = mi->dex_file_;
        const DexFile::MethodId& dex_method = dex->GetMethodId(mi->dex_method_index_);
        const DexFile::ProtoId& dex_proto = dex->GetMethodPrototype(dex_method);
        const DexFile::TypeList* dex_params = dex->GetProtoParameters(dex_proto);
        const char* dex_class_desc = dex->GetMethodDeclaringClassDescriptor(dex_method);

        bool is_optiming = mi->compiled_method_->GetQuickCode().size() > 0 &&
                           mi->compiled_method_->GetGcMap().size() == 0 &&
                           mi->code_item_ != nullptr;

        // Enclose the method in correct class definition.
        if (last_dex_class_desc != dex_class_desc) {
          if (last_dex_class_desc != nullptr) {
            EndClassTag(last_dex_class_desc);
          }
          StartClassTag(dex_class_desc);
          // Check that each class is defined only once.
          CHECK(owner_->defined_dex_classes_.insert(dex_class_desc).second);
          last_dex_class_desc = dex_class_desc;
        }

        std::vector<const char*> param_names;
        if (mi->code_item_ != nullptr) {
          const uint8_t* stream = dex->GetDebugInfoStream(mi->code_item_);
          if (stream != nullptr) {
            DecodeUnsignedLeb128(&stream);  // line.
            uint32_t parameters_size = DecodeUnsignedLeb128(&stream);
            for (uint32_t i = 0; i < parameters_size; ++i) {
              uint32_t id = DecodeUnsignedLeb128P1(&stream);
              param_names.push_back(mi->dex_file_->StringDataByIdx(id));
            }
          }
        }

        int start_depth = info_.Depth();
        info_.StartTag(DW_TAG_subprogram);
        info_.WriteStrp(DW_AT_name, dex->GetMethodName(dex_method), debug_str);
        info_.WriteAddr(DW_AT_low_pc, text_address + mi->low_pc_);
        info_.WriteAddr(DW_AT_high_pc, text_address + mi->high_pc_);
        uint8_t frame_base[] = { DW_OP_call_frame_cfa };
        info_.WriteExprLoc(DW_AT_frame_base, &frame_base, sizeof(frame_base));
        WriteLazyType(dex->GetReturnTypeDescriptor(dex_proto));
        if (dex_params != nullptr) {
          uint32_t vreg = !is_optiming ? 0 :
            mi->code_item_->registers_size_ - mi->code_item_->ins_size_;
          if ((mi->access_flags_ & kAccStatic) == 0) {
            vreg++;
          }
          for (uint32_t i = 0; i < dex_params->Size(); ++i) {
            info_.StartTag(DW_TAG_formal_parameter);
            // Parameter names may not be always available.
            if (i < param_names.size() && param_names[i] != nullptr) {
              info_.WriteStrp(DW_AT_name, param_names[i], debug_str);
            }
            // Write the type.
            const char* type_desc = dex->StringByTypeIdx(dex_params->GetTypeItem(i).type_idx_);
            WriteLazyType(type_desc);
            // Write the stack location of the parameter.
            if (is_optiming) {
              const void* raw_code_info = mi->compiled_method_->GetVmapTable().data();
              CHECK(raw_code_info != nullptr);
              CodeInfo code_info(raw_code_info);
              StackMapEncoding encoding = code_info.ExtractEncoding();
              for (uint32_t s = 0; s < code_info.GetNumberOfStackMaps(); s++) {
                StackMap stack_map = code_info.GetStackMapAt(s, encoding);
                if (stack_map.IsValid() && stack_map.HasDexRegisterMap(encoding)) {
                  DexRegisterMap dex_register_map = code_info.GetDexRegisterMapOf(
                      stack_map, encoding, mi->code_item_->registers_size_);
                  DexRegisterLocation reg_loc = dex_register_map.GetDexRegisterLocation(
                      vreg, mi->code_item_->registers_size_, code_info, encoding);
                  if (reg_loc.GetKind() == DexRegisterLocation::Kind::kInStack) {
                    uint8_t location[8] = { DW_OP_fbreg };
                    uint8_t* end = EncodeSignedLeb128(&location[1], reg_loc.GetValue());
                    info_.WriteExprLoc(DW_AT_location, &location[0], end - location);
                    break;
                  }
                }
              }
            }
            vreg += type_desc[0] == 'D' || type_desc[0] == 'J' ? 2 : 1;
            info_.EndTag();
          }
          if (is_optiming) {
            CHECK_EQ(vreg, mi->code_item_->registers_size_);
          }
        }
        info_.EndTag();
        CHECK_EQ(info_.Depth(), start_depth);  // Balanced start/end.
      }
      if (last_dex_class_desc != nullptr) {
        EndClassTag(last_dex_class_desc);
      }
      CHECK_EQ(info_.Depth(), 1);
      FinishLazyTypes();
      info_.EndTag();  // DW_TAG_compile_unit
      std::vector<uint8_t> buffer;
      buffer.reserve(info_.data()->size() + KB);
      size_t offset = owner_->builder_->GetDebugInfo()->GetSize();
      WriteDebugInfoCU(debug_abbrev_offset_, info_, offset, &buffer, &owner_->debug_info_patches_);
      owner_->builder_->GetDebugInfo()->WriteFully(buffer.data(), buffer.size());
    }

    // Some types are difficult to define as we go since they need
    // to be enclosed in the right set of namespaces. Therefore we
    // just define all types lazily at the end of compilation unit.
    void WriteLazyType(const char* type_descriptor) {
      if (type_descriptor != nullptr && type_descriptor[0] != 'V') {
        lazy_types_.push_back(std::make_pair(info_.size(), type_descriptor));
        info_.WriteRef4(DW_AT_type, 0);
      }
    }

    void FinishLazyTypes() {
      for (const auto& lazy_type : lazy_types_) {
        info_.UpdateUint32(lazy_type.first, WriteType(lazy_type.second));
      }
      lazy_types_.clear();
    }

   private:
    // Convert dex type descriptor to DWARF.
    // Returns offset in the compilation unit.
    uint32_t WriteType(const char* desc) {
      std::string desc_string(desc);
      const auto& it = type_cache_.find(desc_string);
      if (it != type_cache_.end()) {
        return it->second;
      }

      std::vector<uint8_t>* debug_str = &owner_->debug_str_;
      uint32_t offset;
      if (*desc == 'L') {
        // Class type. For example: Lpackage/name;
        uint32_t declaration_offset = StartClassTag(desc);
        info_.WriteFlag(DW_AT_declaration, true);
        EndClassTag(desc);
        // Translate references to pointers.
        offset = info_.StartTag(DW_TAG_pointer_type);
        info_.WriteRef(DW_AT_type, declaration_offset);
        info_.EndTag();
      } else if (*desc == '[') {
        // Array type.
        uint32_t pointee = WriteType(desc + 1);
        offset = info_.StartTag(DW_TAG_pointer_type);
        info_.WriteRef(DW_AT_type, pointee);
        info_.EndTag();
      } else {
        // Primitive types.
        const char* name;
        uint32_t encoding;
        uint32_t byte_size;
        switch (*desc) {
        case 'B':
          name = "byte";
          encoding = DW_ATE_signed;
          byte_size = 4;
          break;
        case 'C':
          name = "char";
          encoding = DW_ATE_unsigned_char;
          byte_size = 4;
          break;
        case 'D':
          name = "double";
          encoding = DW_ATE_float;
          byte_size = 8;
          break;
        case 'F':
          name = "float";
          encoding = DW_ATE_float;
          byte_size = 4;
          break;
        case 'I':
          name = "int";
          encoding = DW_ATE_signed;
          byte_size = 4;
          break;
        case 'J':
          name = "long";
          encoding = DW_ATE_signed;
          byte_size = 8;
          break;
        case 'S':
          name = "short";
          encoding = DW_ATE_signed;
          byte_size = 4;
          break;
        case 'Z':
          name = "boolean";
          encoding = DW_ATE_boolean;
          byte_size = 4;
          break;
        case 'V':
          LOG(FATAL) << "Void type should not be encoded";
          UNREACHABLE();
        default:
          LOG(FATAL) << "Unknown dex type descriptor: " << desc;
          UNREACHABLE();
        }
        offset = info_.StartTag(DW_TAG_base_type);
        info_.WriteStrp(DW_AT_name, name, debug_str);
        info_.WriteData1(DW_AT_encoding, encoding);
        info_.WriteData1(DW_AT_byte_size, byte_size);
        info_.EndTag();
      }

      // TODO: Optimize memory usage by using const char* as key.
      type_cache_.emplace(std::move(desc_string), offset);
      return offset;
    }

    // Start DW_TAG_class_type tag nested in DW_TAG_namespace tags.
    // Returns offset of the class tag in the compilation unit.
    uint32_t StartClassTag(const char* desc) {
      DCHECK(desc != nullptr && desc[0] == 'L');
      // Enclose the type in namespace tags.
      const char* end;
      for (desc = desc + 1; (end = strchr(desc, '/')) != nullptr; desc = end + 1) {
        info_.StartTag(DW_TAG_namespace);
        info_.WriteStrp(DW_AT_name, desc, end - desc, &owner_->debug_str_);
      }
      // Start the class tag.
      uint32_t offset = info_.StartTag(DW_TAG_class_type);
      end = strchr(desc, ';');
      CHECK(end != nullptr);
      info_.WriteStrp(DW_AT_name, desc, end - desc, &owner_->debug_str_);
      return offset;
    }

    void EndClassTag(const char* desc) {
      DCHECK(desc != nullptr && desc[0] == 'L');
      // End the class tag.
      info_.EndTag();
      // Close namespace tags.
      const char* end;
      for (desc = desc + 1; (end = strchr(desc, '/')) != nullptr; desc = end + 1) {
        info_.EndTag();
      }
    }

    // For access to the ELF sections.
    DebugInfoWriter<ElfTypes>* owner_;
    // Offset of our abbrevs from start of .debug_abbrev.
    size_t debug_abbrev_offset_;
    // Temporary buffer to create and store the entries.
    DebugInfoEntryWriter<> info_;
    // Caches of already translated type descriptors.
    std::unordered_map<std::string, uint32_t> type_cache_;  // type_desc -> cu_offset.
    // List of 32-bit references which need to be resolved to a type later.
    std::vector<std::pair<uint32_t, const char*>> lazy_types_;  // cu_offset -> type_desc.
  };

 public:
  explicit DebugInfoWriter(ElfBuilder<ElfTypes>* builder) : builder_(builder) {
  }

  void Start() {
    builder_->GetDebugInfo()->Start();
  }

  void WriteCompilationUnit(const CompilationUnit& compilation_unit) {
    CompilationUnitWriter writer(this);
    writer.Write(compilation_unit);
  }

  void End() {
    builder_->GetDebugInfo()->End();
    builder_->WritePatches(".debug_info.oat_patches", &debug_info_patches_);
    builder_->WriteSection(".debug_abbrev", &debug_abbrev_);
    builder_->WriteSection(".debug_str", &debug_str_);
  }

 private:
  ElfBuilder<ElfTypes>* builder_;
  std::vector<uintptr_t> debug_info_patches_;
  std::vector<uint8_t> debug_abbrev_;
  std::vector<uint8_t> debug_str_;

  std::unordered_set<const char*> defined_dex_classes_;  // For CHECKs only.
};

template<typename ElfTypes>
class DebugLineWriter {
  typedef typename ElfTypes::Addr Elf_Addr;

 public:
  explicit DebugLineWriter(ElfBuilder<ElfTypes>* builder) : builder_(builder) {
  }

  void Start() {
    builder_->GetDebugLine()->Start();
  }

  // Write line table for given set of methods.
  // Returns the number of bytes written.
  size_t WriteCompilationUnit(CompilationUnit& compilation_unit) {
    const bool is64bit = Is64BitInstructionSet(builder_->GetIsa());
    const Elf_Addr text_address = builder_->GetText()->GetAddress();

    compilation_unit.debug_line_offset_ = builder_->GetDebugLine()->GetSize();

    std::vector<FileEntry> files;
    std::unordered_map<std::string, size_t> files_map;
    std::vector<std::string> directories;
    std::unordered_map<std::string, size_t> directories_map;
    int code_factor_bits_ = 0;
    int dwarf_isa = -1;
    switch (builder_->GetIsa()) {
      case kArm:  // arm actually means thumb2.
      case kThumb2:
        code_factor_bits_ = 1;  // 16-bit instuctions
        dwarf_isa = 1;  // DW_ISA_ARM_thumb.
        break;
      case kArm64:
      case kMips:
      case kMips64:
        code_factor_bits_ = 2;  // 32-bit instructions
        break;
      case kNone:
      case kX86:
      case kX86_64:
        break;
    }
    DebugLineOpCodeWriter<> opcodes(is64bit, code_factor_bits_);
    opcodes.SetAddress(text_address + compilation_unit.low_pc_);
    if (dwarf_isa != -1) {
      opcodes.SetISA(dwarf_isa);
    }
    for (const OatWriter::DebugInfo* mi : compilation_unit.methods_) {
      // Ignore function if we have already generated line table
      // for the given address.  It would confuse the debugger.
      if (mi->deduped_) {
        continue;
      }

      struct DebugInfoCallbacks {
        static bool NewPosition(void* ctx, uint32_t address, uint32_t line) {
          auto* context = reinterpret_cast<DebugInfoCallbacks*>(ctx);
          context->dex2line_.push_back({address, static_cast<int32_t>(line)});
          return false;
        }
        DefaultSrcMap dex2line_;
      } debug_info_callbacks;

      Elf_Addr method_address = text_address + mi->low_pc_;

      const DexFile* dex = mi->dex_file_;
      if (mi->code_item_ != nullptr) {
        dex->DecodeDebugInfo(mi->code_item_,
                             (mi->access_flags_ & kAccStatic) != 0,
                             mi->dex_method_index_,
                             DebugInfoCallbacks::NewPosition,
                             nullptr,
                             &debug_info_callbacks);
      }

      // Get and deduplicate directory and filename.
      int file_index = 0;  // 0 - primary source file of the compilation.
      auto& dex_class_def = dex->GetClassDef(mi->class_def_index_);
      const char* source_file = dex->GetSourceFile(dex_class_def);
      if (source_file != nullptr) {
        std::string file_name(source_file);
        size_t file_name_slash = file_name.find_last_of('/');
        std::string class_name(dex->GetClassDescriptor(dex_class_def));
        size_t class_name_slash = class_name.find_last_of('/');
        std::string full_path(file_name);

        // Guess directory from package name.
        int directory_index = 0;  // 0 - current directory of the compilation.
        if (file_name_slash == std::string::npos &&  // Just filename.
            class_name.front() == 'L' &&  // Type descriptor for a class.
            class_name_slash != std::string::npos) {  // Has package name.
          std::string package_name = class_name.substr(1, class_name_slash - 1);
          auto it = directories_map.find(package_name);
          if (it == directories_map.end()) {
            directory_index = 1 + directories.size();
            directories_map.emplace(package_name, directory_index);
            directories.push_back(package_name);
          } else {
            directory_index = it->second;
          }
          full_path = package_name + "/" + file_name;
        }

        // Add file entry.
        auto it2 = files_map.find(full_path);
        if (it2 == files_map.end()) {
          file_index = 1 + files.size();
          files_map.emplace(full_path, file_index);
          files.push_back(FileEntry {
            file_name,
            directory_index,
            0,  // Modification time - NA.
            0,  // File size - NA.
          });
        } else {
          file_index = it2->second;
        }
      }
      opcodes.SetFile(file_index);

      // Generate mapping opcodes from PC to Java lines.
      const DefaultSrcMap& dex2line_map = debug_info_callbacks.dex2line_;
      if (file_index != 0 && !dex2line_map.empty()) {
        bool first = true;
        for (SrcMapElem pc2dex : mi->compiled_method_->GetSrcMappingTable()) {
          uint32_t pc = pc2dex.from_;
          int dex_pc = pc2dex.to_;
          auto dex2line = dex2line_map.Find(static_cast<uint32_t>(dex_pc));
          if (dex2line.first) {
            int line = dex2line.second;
            if (first) {
              first = false;
              if (pc > 0) {
                // Assume that any preceding code is prologue.
                int first_line = dex2line_map.front().to_;
                // Prologue is not a sensible place for a breakpoint.
                opcodes.NegateStmt();
                opcodes.AddRow(method_address, first_line);
                opcodes.NegateStmt();
                opcodes.SetPrologueEnd();
              }
              opcodes.AddRow(method_address + pc, line);
            } else if (line != opcodes.CurrentLine()) {
              opcodes.AddRow(method_address + pc, line);
            }
          }
        }
      } else {
        // line 0 - instruction cannot be attributed to any source line.
        opcodes.AddRow(method_address, 0);
      }
    }
    opcodes.AdvancePC(text_address + compilation_unit.high_pc_);
    opcodes.EndSequence();
    std::vector<uint8_t> buffer;
    buffer.reserve(opcodes.data()->size() + KB);
    size_t offset = builder_->GetDebugLine()->GetSize();
    WriteDebugLineTable(directories, files, opcodes, offset, &buffer, &debug_line_patches);
    builder_->GetDebugLine()->WriteFully(buffer.data(), buffer.size());
    return buffer.size();
  }

  void End() {
    builder_->GetDebugLine()->End();
    builder_->WritePatches(".debug_line.oat_patches", &debug_line_patches);
  }

 private:
  ElfBuilder<ElfTypes>* builder_;
  std::vector<uintptr_t> debug_line_patches;
};

template<typename ElfTypes>
void WriteDebugSections(ElfBuilder<ElfTypes>* builder,
                        const std::vector<OatWriter::DebugInfo>& method_infos) {
  // Group the methods into compilation units based on source file.
  std::vector<CompilationUnit> compilation_units;
  const char* last_source_file = nullptr;
  for (const OatWriter::DebugInfo& mi : method_infos) {
    auto& dex_class_def = mi.dex_file_->GetClassDef(mi.class_def_index_);
    const char* source_file = mi.dex_file_->GetSourceFile(dex_class_def);
    if (compilation_units.empty() || source_file != last_source_file) {
      compilation_units.push_back(CompilationUnit());
    }
    CompilationUnit& cu = compilation_units.back();
    cu.methods_.push_back(&mi);
    cu.low_pc_ = std::min(cu.low_pc_, mi.low_pc_);
    cu.high_pc_ = std::max(cu.high_pc_, mi.high_pc_);
    last_source_file = source_file;
  }

  // Write .debug_line section.
  {
    DebugLineWriter<ElfTypes> line_writer(builder);
    line_writer.Start();
    for (auto& compilation_unit : compilation_units) {
      line_writer.WriteCompilationUnit(compilation_unit);
    }
    line_writer.End();
  }

  // Write .debug_info section.
  {
    DebugInfoWriter<ElfTypes> info_writer(builder);
    info_writer.Start();
    for (const auto& compilation_unit : compilation_units) {
      info_writer.WriteCompilationUnit(compilation_unit);
    }
    info_writer.End();
  }
}

// Explicit instantiations
template void WriteCFISection<ElfTypes32>(
    ElfBuilder<ElfTypes32>* builder,
    const std::vector<OatWriter::DebugInfo>& method_infos,
    CFIFormat format);
template void WriteCFISection<ElfTypes64>(
    ElfBuilder<ElfTypes64>* builder,
    const std::vector<OatWriter::DebugInfo>& method_infos,
    CFIFormat format);
template void WriteDebugSections<ElfTypes32>(
    ElfBuilder<ElfTypes32>* builder,
    const std::vector<OatWriter::DebugInfo>& method_infos);
template void WriteDebugSections<ElfTypes64>(
    ElfBuilder<ElfTypes64>* builder,
    const std::vector<OatWriter::DebugInfo>& method_infos);

}  // namespace dwarf
}  // namespace art
