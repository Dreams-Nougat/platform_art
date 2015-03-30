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

#include "elf_writer_quick.h"

#include <unordered_map>

#include "base/logging.h"
#include "base/unix_file/fd_file.h"
#include "buffered_output_stream.h"
#include "driver/compiler_driver.h"
#include "dwarf.h"
#include "dwarf/debug_frame_writer.h"
#include "dwarf/debug_line_writer.h"
#include "elf_builder.h"
#include "elf_file.h"
#include "elf_utils.h"
#include "file_output_stream.h"
#include "globals.h"
#include "leb128.h"
#include "oat.h"
#include "oat_writer.h"
#include "utils.h"

namespace art {

static void PushByte(std::vector<uint8_t>* buf, int data) {
  buf->push_back(data & 0xff);
}

static uint32_t PushStr(std::vector<uint8_t>* buf, const char* str, const char* def = nullptr) {
  if (str == nullptr) {
    str = def;
  }

  uint32_t offset = buf->size();
  for (size_t i = 0; str[i] != '\0'; ++i) {
    buf->push_back(str[i]);
  }
  buf->push_back('\0');
  return offset;
}

static uint32_t PushStr(std::vector<uint8_t>* buf, const std::string &str) {
  uint32_t offset = buf->size();
  buf->insert(buf->end(), str.begin(), str.end());
  buf->push_back('\0');
  return offset;
}

static void UpdateWord(std::vector<uint8_t>* buf, int offset, int data) {
  (*buf)[offset+0] = data;
  (*buf)[offset+1] = data >> 8;
  (*buf)[offset+2] = data >> 16;
  (*buf)[offset+3] = data >> 24;
}

static void PushHalf(std::vector<uint8_t>* buf, int data) {
  buf->push_back(data & 0xff);
  buf->push_back((data >> 8) & 0xff);
}

template <typename Elf_Word, typename Elf_Sword, typename Elf_Addr,
          typename Elf_Dyn, typename Elf_Sym, typename Elf_Ehdr,
          typename Elf_Phdr, typename Elf_Shdr>
bool ElfWriterQuick<Elf_Word, Elf_Sword, Elf_Addr, Elf_Dyn,
  Elf_Sym, Elf_Ehdr, Elf_Phdr, Elf_Shdr>::Create(File* elf_file,
                            OatWriter* oat_writer,
                            const std::vector<const DexFile*>& dex_files,
                            const std::string& android_root,
                            bool is_host,
                            const CompilerDriver& driver) {
  ElfWriterQuick elf_writer(driver, elf_file);
  return elf_writer.Write(oat_writer, dex_files, android_root, is_host);
}

std::vector<uint8_t>* ConstructCIEFrameX86(bool is_x86_64) {
  std::vector<uint8_t>* cfi_info = new std::vector<uint8_t>;

  // Length (will be filled in later in this routine).
  if (is_x86_64) {
    Push32(cfi_info, 0xffffffff);  // Indicates 64bit
    Push32(cfi_info, 0);
    Push32(cfi_info, 0);
  } else {
    Push32(cfi_info, 0);
  }

  // CIE id: always 0.
  if (is_x86_64) {
    Push32(cfi_info, 0);
    Push32(cfi_info, 0);
  } else {
    Push32(cfi_info, 0);
  }

  // Version: always 1.
  cfi_info->push_back(0x01);

  // Augmentation: 'zR\0'
  cfi_info->push_back(0x7a);
  cfi_info->push_back(0x52);
  cfi_info->push_back(0x0);

  // Code alignment: 1.
  EncodeUnsignedLeb128(1, cfi_info);

  // Data alignment.
  if (is_x86_64) {
    EncodeSignedLeb128(-8, cfi_info);
  } else {
    EncodeSignedLeb128(-4, cfi_info);
  }

  // Return address register.
  if (is_x86_64) {
    // R16(RIP)
    cfi_info->push_back(0x10);
  } else {
    // R8(EIP)
    cfi_info->push_back(0x08);
  }

  // Augmentation length: 1.
  cfi_info->push_back(1);

  // Augmentation data.
  if (is_x86_64) {
    // 0x04 ((DW_EH_PE_absptr << 4) | DW_EH_PE_udata8).
    cfi_info->push_back(0x04);
  } else {
    // 0x03 ((DW_EH_PE_absptr << 4) | DW_EH_PE_udata4).
    cfi_info->push_back(0x03);
  }

  // Initial instructions.
  if (is_x86_64) {
    // DW_CFA_def_cfa R7(RSP) 8.
    cfi_info->push_back(0x0c);
    cfi_info->push_back(0x07);
    cfi_info->push_back(0x08);

    // DW_CFA_offset R16(RIP) 1 (* -8).
    cfi_info->push_back(0x90);
    cfi_info->push_back(0x01);
  } else {
    // DW_CFA_def_cfa R4(ESP) 4.
    cfi_info->push_back(0x0c);
    cfi_info->push_back(0x04);
    cfi_info->push_back(0x04);

    // DW_CFA_offset R8(EIP) 1 (* -4).
    cfi_info->push_back(0x88);
    cfi_info->push_back(0x01);
  }

  // Padding to a multiple of 4
  while ((cfi_info->size() & 3) != 0) {
    // DW_CFA_nop is encoded as 0.
    cfi_info->push_back(0);
  }

  // Set the length of the CIE inside the generated bytes.
  if (is_x86_64) {
    uint32_t length = cfi_info->size() - 12;
    UpdateWord(cfi_info, 4, length);
  } else {
    uint32_t length = cfi_info->size() - 4;
    UpdateWord(cfi_info, 0, length);
  }
  return cfi_info;
}

std::vector<uint8_t>* ConstructCIEFrame(InstructionSet isa) {
  switch (isa) {
    case kX86:
      return ConstructCIEFrameX86(false);
    case kX86_64:
      return ConstructCIEFrameX86(true);

    default:
      // Not implemented.
      return nullptr;
  }
}

class OatWriterWrapper FINAL : public CodeOutput {
 public:
  explicit OatWriterWrapper(OatWriter* oat_writer) : oat_writer_(oat_writer) {}

  void SetCodeOffset(size_t offset) {
    oat_writer_->SetOatDataOffset(offset);
  }
  bool Write(OutputStream* out) OVERRIDE {
    return oat_writer_->Write(out);
  }
 private:
  OatWriter* const oat_writer_;
};

template <typename Elf_Word, typename Elf_Sword, typename Elf_Addr,
          typename Elf_Dyn, typename Elf_Sym, typename Elf_Ehdr,
          typename Elf_Phdr, typename Elf_Shdr>
static void WriteDebugSymbols(const CompilerDriver* compiler_driver,
                              ElfBuilder<Elf_Word, Elf_Sword, Elf_Addr, Elf_Dyn,
                                         Elf_Sym, Elf_Ehdr, Elf_Phdr, Elf_Shdr>* builder,
                              OatWriter* oat_writer);

template <typename Elf_Word, typename Elf_Sword, typename Elf_Addr,
          typename Elf_Dyn, typename Elf_Sym, typename Elf_Ehdr,
          typename Elf_Phdr, typename Elf_Shdr>
bool ElfWriterQuick<Elf_Word, Elf_Sword, Elf_Addr, Elf_Dyn,
  Elf_Sym, Elf_Ehdr, Elf_Phdr, Elf_Shdr>::Write(OatWriter* oat_writer,
                           const std::vector<const DexFile*>& dex_files_unused ATTRIBUTE_UNUSED,
                           const std::string& android_root_unused ATTRIBUTE_UNUSED,
                           bool is_host_unused ATTRIBUTE_UNUSED) {
  constexpr bool debug = false;
  const OatHeader& oat_header = oat_writer->GetOatHeader();
  Elf_Word oat_data_size = oat_header.GetExecutableOffset();
  uint32_t oat_exec_size = oat_writer->GetSize() - oat_data_size;
  uint32_t oat_bss_size = oat_writer->GetBssSize();

  OatWriterWrapper wrapper(oat_writer);

  std::unique_ptr<ElfBuilder<Elf_Word, Elf_Sword, Elf_Addr, Elf_Dyn,
                             Elf_Sym, Elf_Ehdr, Elf_Phdr, Elf_Shdr> > builder(
      new ElfBuilder<Elf_Word, Elf_Sword, Elf_Addr, Elf_Dyn,
                     Elf_Sym, Elf_Ehdr, Elf_Phdr, Elf_Shdr>(
          &wrapper,
          elf_file_,
          compiler_driver_->GetInstructionSet(),
          0,
          oat_data_size,
          oat_data_size,
          oat_exec_size,
          RoundUp(oat_data_size + oat_exec_size, kPageSize),
          oat_bss_size,
          compiler_driver_->GetCompilerOptions().GetIncludeDebugSymbols(),
          debug));

  if (!builder->Init()) {
    return false;
  }

  if (compiler_driver_->GetCompilerOptions().GetIncludeDebugSymbols()) {
    WriteDebugSymbols(compiler_driver_, builder.get(), oat_writer);
  }

  if (compiler_driver_->GetCompilerOptions().GetIncludePatchInformation()) {
    ElfRawSectionBuilder<Elf_Word, Elf_Sword, Elf_Shdr> oat_patches(
        ".oat_patches", SHT_OAT_PATCH, 0, NULL, 0, sizeof(uintptr_t), sizeof(uintptr_t));
    const std::vector<uintptr_t>& locations = oat_writer->GetAbsolutePatchLocations();
    const uint8_t* begin = reinterpret_cast<const uint8_t*>(&locations[0]);
    const uint8_t* end = begin + locations.size() * sizeof(locations[0]);
    oat_patches.GetBuffer()->assign(begin, end);
    if (debug) {
      LOG(INFO) << "Prepared .oat_patches for " << locations.size() << " patches.";
    }
    builder->RegisterRawSection(oat_patches);
  }

  return builder->Write();
}

// TODO: rewriting it using DexFile::DecodeDebugInfo needs unneeded stuff.
static void GetLineInfoForJava(const uint8_t* dbgstream, DefaultSrcMap* dex2line) {
  if (dbgstream == nullptr) {
    return;
  }

  int adjopcode;
  uint32_t dex_offset = 0;
  uint32_t java_line = DecodeUnsignedLeb128(&dbgstream);

  // skip parameters
  for (uint32_t param_count = DecodeUnsignedLeb128(&dbgstream); param_count != 0; --param_count) {
    DecodeUnsignedLeb128(&dbgstream);
  }

  for (bool is_end = false; is_end == false; ) {
    uint8_t opcode = *dbgstream;
    dbgstream++;
    switch (opcode) {
    case DexFile::DBG_END_SEQUENCE:
      is_end = true;
      break;

    case DexFile::DBG_ADVANCE_PC:
      dex_offset += DecodeUnsignedLeb128(&dbgstream);
      break;

    case DexFile::DBG_ADVANCE_LINE:
      java_line += DecodeSignedLeb128(&dbgstream);
      break;

    case DexFile::DBG_START_LOCAL:
    case DexFile::DBG_START_LOCAL_EXTENDED:
      DecodeUnsignedLeb128(&dbgstream);
      DecodeUnsignedLeb128(&dbgstream);
      DecodeUnsignedLeb128(&dbgstream);

      if (opcode == DexFile::DBG_START_LOCAL_EXTENDED) {
        DecodeUnsignedLeb128(&dbgstream);
      }
      break;

    case DexFile::DBG_END_LOCAL:
    case DexFile::DBG_RESTART_LOCAL:
      DecodeUnsignedLeb128(&dbgstream);
      break;

    case DexFile::DBG_SET_PROLOGUE_END:
    case DexFile::DBG_SET_EPILOGUE_BEGIN:
    case DexFile::DBG_SET_FILE:
      break;

    default:
      adjopcode = opcode - DexFile::DBG_FIRST_SPECIAL;
      dex_offset += adjopcode / DexFile::DBG_LINE_RANGE;
      java_line += DexFile::DBG_LINE_BASE + (adjopcode % DexFile::DBG_LINE_RANGE);
      dex2line->push_back({dex_offset, static_cast<int32_t>(java_line)});
      break;
    }
  }
}

/*
 * @brief Generate the DWARF debug_info and debug_abbrev sections
 * @param oat_writer The Oat file Writer.
 * @param dbg_info Compilation unit information.
 * @param dbg_abbrev Abbreviations used to generate dbg_info.
 * @param dbg_str Debug strings.
 */
static void FillInCFIInformation(OatWriter* oat_writer,
                                 std::vector<uint8_t>* dbg_info,
                                 std::vector<uint8_t>* dbg_abbrev,
                                 std::vector<uint8_t>* dbg_str,
                                 std::vector<uint8_t>* dbg_line,
                                 uint32_t text_section_offset) {
  const std::vector<OatWriter::DebugInfo>& method_infos = oat_writer->GetCFIMethodInfo();

  uint32_t producer_str_offset = PushStr(dbg_str, "Android dex2oat");

  constexpr bool use_64bit_addresses = false;

  // Create the debug_abbrev section with boilerplate information.
  // We only care about low_pc and high_pc right now for the compilation
  // unit and methods.

  // Tag 1: Compilation unit: DW_TAG_compile_unit.
  PushByte(dbg_abbrev, 1);
  PushByte(dbg_abbrev, dwarf::DW_TAG_compile_unit);

  // There are children (the methods).
  PushByte(dbg_abbrev, dwarf::DW_CHILDREN_yes);

  // DW_AT_producer DW_FORM_data1.
  // REVIEW: we can get rid of dbg_str section if
  // DW_FORM_string (immediate string) was used everywhere instead of
  // DW_FORM_strp (ref to string from .debug_str section).
  // DW_FORM_strp makes sense only if we reuse the strings.
  PushByte(dbg_abbrev, dwarf::DW_AT_producer);
  PushByte(dbg_abbrev, dwarf::DW_FORM_strp);

  // DW_LANG_Java DW_FORM_data1.
  PushByte(dbg_abbrev, dwarf::DW_AT_language);
  PushByte(dbg_abbrev, dwarf::DW_FORM_data1);

  // DW_AT_low_pc DW_FORM_addr.
  PushByte(dbg_abbrev, dwarf::DW_AT_low_pc);
  PushByte(dbg_abbrev, dwarf::DW_FORM_addr);

  // DW_AT_high_pc DW_FORM_addr.
  PushByte(dbg_abbrev, dwarf::DW_AT_high_pc);
  PushByte(dbg_abbrev, dwarf::DW_FORM_addr);

  if (dbg_line != nullptr) {
    // DW_AT_stmt_list DW_FORM_sec_offset.
    PushByte(dbg_abbrev, dwarf::DW_AT_stmt_list);
    PushByte(dbg_abbrev, dwarf::DW_FORM_data4);
  }

  // End of DW_TAG_compile_unit.
  PushByte(dbg_abbrev, 0);  // DW_AT.
  PushByte(dbg_abbrev, 0);  // DW_FORM.

  // Tag 2: Compilation unit: DW_TAG_subprogram.
  PushByte(dbg_abbrev, 2);
  PushByte(dbg_abbrev, dwarf::DW_TAG_subprogram);

  // There are no children.
  PushByte(dbg_abbrev, dwarf::DW_CHILDREN_no);

  // Name of the method.
  PushByte(dbg_abbrev, dwarf::DW_AT_name);
  PushByte(dbg_abbrev, dwarf::DW_FORM_strp);

  // DW_AT_low_pc DW_FORM_addr.
  PushByte(dbg_abbrev, dwarf::DW_AT_low_pc);
  PushByte(dbg_abbrev, dwarf::DW_FORM_addr);

  // DW_AT_high_pc DW_FORM_addr.
  PushByte(dbg_abbrev, dwarf::DW_AT_high_pc);
  PushByte(dbg_abbrev, dwarf::DW_FORM_addr);

  // End of DW_TAG_subprogram.
  PushByte(dbg_abbrev, 0);  // DW_AT.
  PushByte(dbg_abbrev, 0);  // DW_FORM.

  // End of abbrevs for compilation unit
  PushByte(dbg_abbrev, 0);

  // Start the debug_info section with the header information
  // 'unit_length' will be filled in later.
  int cunit_length = dbg_info->size();
  Push32(dbg_info, 0);

  // 'version' - 3.
  PushHalf(dbg_info, 3);

  // Offset into .debug_abbrev section (always 0).
  Push32(dbg_info, 0);

  // Address size: 4 or 8.
  PushByte(dbg_info, use_64bit_addresses ? 8 : 4);

  // Start the description for the compilation unit.
  // This uses tag 1.
  PushByte(dbg_info, 1);

  // The producer is Android dex2oat.
  Push32(dbg_info, producer_str_offset);

  // The language is Java.
  PushByte(dbg_info, dwarf::DW_LANG_Java);

  // low_pc and high_pc.
  uint32_t cunit_low_pc = static_cast<uint32_t>(-1);
  uint32_t cunit_high_pc = 0;
  for (auto method_info : method_infos) {
    cunit_low_pc = std::min(cunit_low_pc, method_info.low_pc_);
    cunit_high_pc = std::max(cunit_high_pc, method_info.high_pc_);
  }
  Push32(dbg_info, cunit_low_pc + text_section_offset);
  Push32(dbg_info, cunit_high_pc + text_section_offset);

  if (dbg_line != nullptr) {
    // Line number table offset.
    Push32(dbg_info, dbg_line->size());
  }

  for (auto method_info : method_infos) {
    // Start a new TAG: subroutine (2).
    PushByte(dbg_info, 2);

    // Enter name, low_pc, high_pc.
    Push32(dbg_info, PushStr(dbg_str, method_info.method_name_));
    Push32(dbg_info, method_info.low_pc_ + text_section_offset);
    Push32(dbg_info, method_info.high_pc_ + text_section_offset);
  }

  if (dbg_line != nullptr) {
    // TODO: in gdb info functions <regexp> - reports Java functions, but
    // source file is <unknown> because .debug_line is formed as one
    // compilation unit. To fix this it is possible to generate
    // a separate compilation unit for every distinct Java source.
    // Each of the these compilation units can have several non-adjacent
    // method ranges.

    std::vector<dwarf::DebugLineWriter<>::FileEntry> files;
    std::unordered_map<std::string, size_t> files_map;
    std::vector<std::string> directories;
    std::unordered_map<std::string, size_t> directories_map;

    int code_factor_bits_ = 0;
    int isa = -1;
    switch (oat_writer->GetOatHeader().GetInstructionSet()) {
      case kThumb2:
        code_factor_bits_ = 1;  // 16-bit instuctions
        isa = 1;  // DW_ISA_ARM_thumb.
        break;
      case kArm:
        code_factor_bits_ = 2;  // 32-bit instructions
        isa = 2;  // DW_ISA_ARM_arm.
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

    dwarf::DebugLineOpCodeWriter<> opcodes(use_64bit_addresses, code_factor_bits_);
    opcodes.SetAddress(text_section_offset + cunit_low_pc);
    if (isa != -1) {
      opcodes.SetISA(isa);
    }
    DefaultSrcMap dex2line_map;
    for (size_t i = 0; i < method_infos.size(); i++) {
      const OatWriter::DebugInfo& method_info = method_infos[i];

      // Addresses in the line table should be unique and increasing.
      if (method_info.deduped_) {
        continue;
      }

      // Get and deduplicate directory and filename.
      int file_index = 0;  // 0 - primary source file of the compilation.
      if (method_info.src_file_name_ != nullptr) {
        std::string file_name(method_info.src_file_name_);
        size_t file_name_slash = file_name.find_last_of('/');
        std::string class_name(method_info.class_descriptor_);
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
          files.push_back(dwarf::DebugLineWriter<>::FileEntry {
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
      dex2line_map.clear();
      GetLineInfoForJava(method_info.dbgstream_, &dex2line_map);
      uint32_t low_pc = text_section_offset + method_info.low_pc_;
      if (file_index != 0 && !dex2line_map.empty()) {
        bool first = true;
        for (SrcMapElem pc2dex : method_info.compiled_method_->GetSrcMappingTable()) {
          uint32_t pc = pc2dex.from_;
          int dex = pc2dex.to_;
          auto dex2line = dex2line_map.Find(static_cast<uint32_t>(dex));
          if (dex2line.first) {
            int line = dex2line.second;
            if (first) {
              first = false;
              if (pc > 0) {
                // Assume that any preceding code is prologue.
                int first_line = dex2line_map.front().to_;
                // Prologue is not a sensible place for a breakpoint.
                opcodes.NegateStmt();
                opcodes.AddRow(low_pc, first_line);
                opcodes.NegateStmt();
                opcodes.SetPrologueEnd();
              }
              opcodes.AddRow(low_pc + pc, line);
            } else if (line != opcodes.CurrentLine()) {
              opcodes.AddRow(low_pc + pc, line);
            }
          }
        }
      } else {
        // line 0 - instruction cannot be attributed to any source line.
        opcodes.AddRow(low_pc, 0);
      }
    }

    opcodes.AdvancePC(text_section_offset + cunit_high_pc);
    opcodes.EndSequence();

    dwarf::DebugLineWriter<> dbg_line_writer(dbg_line);
    dbg_line_writer.WriteTable(directories, files, opcodes);
  }

  // One byte terminator.
  PushByte(dbg_info, 0);

  // We have now walked all the methods.  Fill in lengths.
  UpdateWord(dbg_info, cunit_length, dbg_info->size() - cunit_length - 4);
}

template <typename Elf_Word, typename Elf_Sword, typename Elf_Addr,
          typename Elf_Dyn, typename Elf_Sym, typename Elf_Ehdr,
          typename Elf_Phdr, typename Elf_Shdr>
// Do not inline to avoid Clang stack frame problems. b/18738594
NO_INLINE
static void WriteDebugSymbols(const CompilerDriver* compiler_driver,
                              ElfBuilder<Elf_Word, Elf_Sword, Elf_Addr, Elf_Dyn,
                                         Elf_Sym, Elf_Ehdr, Elf_Phdr, Elf_Shdr>* builder,
                              OatWriter* oat_writer) {
  std::unique_ptr<std::vector<uint8_t>> cfi_info(
      ConstructCIEFrame(compiler_driver->GetInstructionSet()));

  Elf_Addr text_section_address = builder->GetTextBuilder().GetSection()->sh_addr;

  // Iterate over the compiled methods.
  const std::vector<OatWriter::DebugInfo>& method_info = oat_writer->GetCFIMethodInfo();
  ElfSymtabBuilder<Elf_Word, Elf_Sword, Elf_Addr, Elf_Sym, Elf_Shdr>* symtab =
      builder->GetSymtabBuilder();
  for (auto it = method_info.begin(); it != method_info.end(); ++it) {
    // Set last bit to 1 to indicate Thumb2 code.
    int thumb_offset = it->compiled_method_->CodeDelta();
    symtab->AddSymbol(it->method_name_, &builder->GetTextBuilder(),
                      it->low_pc_ + thumb_offset, true,
                      it->high_pc_ - it->low_pc_, STB_GLOBAL, STT_FUNC);

    // Conforming to aaelf, add $t mapping symbol to indicate start of a sequence of thumb2
    // instructions, so that disassembler tools can correctly disassemble.
    if (it->compiled_method_->GetInstructionSet() == kThumb2) {
      symtab->AddSymbol("$t", &builder->GetTextBuilder(), it->low_pc_ & ~1, true,
                        0, STB_LOCAL, STT_NOTYPE);
    }

    // Include CFI for compiled method, if possible.
    if (cfi_info.get() != nullptr) {
      DCHECK(it->compiled_method_ != nullptr);

      // Copy in the FDE, if present
      const SwapVector<uint8_t>* fde = it->compiled_method_->GetCFIInfo();
      if (fde != nullptr) {
        // Copy the information into cfi_info and then fix the address in the new copy.
        int cur_offset = cfi_info->size();
        cfi_info->insert(cfi_info->end(), fde->begin(), fde->end());

        bool is_64bit = *(reinterpret_cast<const uint32_t*>(fde->data())) == 0xffffffff;

        // Set the 'CIE_pointer' field.
        uint64_t CIE_pointer = cur_offset + (is_64bit ? 12 : 4);
        uint64_t offset_to_update = CIE_pointer;
        if (is_64bit) {
          (*cfi_info)[offset_to_update+0] = CIE_pointer;
          (*cfi_info)[offset_to_update+1] = CIE_pointer >> 8;
          (*cfi_info)[offset_to_update+2] = CIE_pointer >> 16;
          (*cfi_info)[offset_to_update+3] = CIE_pointer >> 24;
          (*cfi_info)[offset_to_update+4] = CIE_pointer >> 32;
          (*cfi_info)[offset_to_update+5] = CIE_pointer >> 40;
          (*cfi_info)[offset_to_update+6] = CIE_pointer >> 48;
          (*cfi_info)[offset_to_update+7] = CIE_pointer >> 56;
        } else {
          (*cfi_info)[offset_to_update+0] = CIE_pointer;
          (*cfi_info)[offset_to_update+1] = CIE_pointer >> 8;
          (*cfi_info)[offset_to_update+2] = CIE_pointer >> 16;
          (*cfi_info)[offset_to_update+3] = CIE_pointer >> 24;
        }

        // Set the 'initial_location' field.
        offset_to_update += is_64bit ? 8 : 4;
        if (is_64bit) {
          const uint64_t quick_code_start = it->low_pc_ + text_section_address;
          (*cfi_info)[offset_to_update+0] = quick_code_start;
          (*cfi_info)[offset_to_update+1] = quick_code_start >> 8;
          (*cfi_info)[offset_to_update+2] = quick_code_start >> 16;
          (*cfi_info)[offset_to_update+3] = quick_code_start >> 24;
          (*cfi_info)[offset_to_update+4] = quick_code_start >> 32;
          (*cfi_info)[offset_to_update+5] = quick_code_start >> 40;
          (*cfi_info)[offset_to_update+6] = quick_code_start >> 48;
          (*cfi_info)[offset_to_update+7] = quick_code_start >> 56;
        } else {
          const uint32_t quick_code_start = it->low_pc_ + text_section_address;
          (*cfi_info)[offset_to_update+0] = quick_code_start;
          (*cfi_info)[offset_to_update+1] = quick_code_start >> 8;
          (*cfi_info)[offset_to_update+2] = quick_code_start >> 16;
          (*cfi_info)[offset_to_update+3] = quick_code_start >> 24;
        }
      }
    }
  }

  bool hasCFI = (cfi_info.get() != nullptr);
  bool hasLineInfo = false;
  for (auto& dbg_info : oat_writer->GetCFIMethodInfo()) {
    if (dbg_info.dbgstream_ != nullptr &&
        !dbg_info.compiled_method_->GetSrcMappingTable().empty()) {
      hasLineInfo = true;
      break;
    }
  }

  if (hasLineInfo || hasCFI) {
    ElfRawSectionBuilder<Elf_Word, Elf_Sword, Elf_Shdr> debug_info(".debug_info",
                                                                   SHT_PROGBITS,
                                                                   0, nullptr, 0, 1, 0);
    ElfRawSectionBuilder<Elf_Word, Elf_Sword, Elf_Shdr> debug_abbrev(".debug_abbrev",
                                                                     SHT_PROGBITS,
                                                                     0, nullptr, 0, 1, 0);
    ElfRawSectionBuilder<Elf_Word, Elf_Sword, Elf_Shdr> debug_str(".debug_str",
                                                                  SHT_PROGBITS,
                                                                  0, nullptr, 0, 1, 0);
    ElfRawSectionBuilder<Elf_Word, Elf_Sword, Elf_Shdr> debug_line(".debug_line",
                                                                   SHT_PROGBITS,
                                                                   0, nullptr, 0, 1, 0);

    FillInCFIInformation(oat_writer, debug_info.GetBuffer(),
                         debug_abbrev.GetBuffer(), debug_str.GetBuffer(),
                         hasLineInfo ? debug_line.GetBuffer() : nullptr,
                         text_section_address);

    builder->RegisterRawSection(debug_info);
    builder->RegisterRawSection(debug_abbrev);

    if (hasCFI) {
      ElfRawSectionBuilder<Elf_Word, Elf_Sword, Elf_Shdr> eh_frame(".eh_frame",
                                                                   SHT_PROGBITS,
                                                                   SHF_ALLOC,
                                                                   nullptr, 0, 4, 0);
      eh_frame.SetBuffer(std::move(*cfi_info.get()));
      builder->RegisterRawSection(eh_frame);
    }

    if (hasLineInfo) {
      builder->RegisterRawSection(debug_line);
    }

    builder->RegisterRawSection(debug_str);
  }
}

// Explicit instantiations
template class ElfWriterQuick<Elf32_Word, Elf32_Sword, Elf32_Addr, Elf32_Dyn,
                              Elf32_Sym, Elf32_Ehdr, Elf32_Phdr, Elf32_Shdr>;
template class ElfWriterQuick<Elf64_Word, Elf64_Sword, Elf64_Addr, Elf64_Dyn,
                              Elf64_Sym, Elf64_Ehdr, Elf64_Phdr, Elf64_Shdr>;

}  // namespace art
