
typedef std::vector<DexFile::LocalInfo> LocalInfos;

void LocalInfoCallback(void* ctx, const DexFile::LocalInfo& entry) {
  static_cast<LocalInfos*>(ctx)->push_back(entry);
}

std::vector<const char*> GetParamNames(const MethodDebugInfo* mi) {
  std::vector<const char*> names;
  if (mi->code_item_ != nullptr) {
    const uint8_t* stream = mi->dex_file_->GetDebugInfoStream(mi->code_item_);
    if (stream != nullptr) {
      DecodeUnsignedLeb128(&stream);  // line.
      uint32_t parameters_size = DecodeUnsignedLeb128(&stream);
      for (uint32_t i = 0; i < parameters_size; ++i) {
        uint32_t id = DecodeUnsignedLeb128P1(&stream);
        names.push_back(mi->dex_file_->StringDataByIdx(id));
      }
    }
  }
  return names;
}


// Helper class to write .debug_info and its supporting sections.
template<typename ElfTypes>
class DebugInfoWriter {
  typedef typename ElfTypes::Addr Elf_Addr;

 public:
  explicit DebugInfoWriter(ElfBuilder<ElfTypes>* builder)
      : builder_(builder),
        debug_abbrev_(&debug_abbrev_buffer_) {
  }

  void Start() {
    builder_->GetDebugInfo()->Start();
  }

  void WriteCompilationUnit(const CompilationUnit& compilation_unit) {
    CompilationUnitWriter writer(this);
    writer.Write(compilation_unit);
  }

  void WriteTypes(const ArrayRef<mirror::Class*>& types) SHARED_REQUIRES(Locks::mutator_lock_) {
    CompilationUnitWriter writer(this);
    writer.Write(types);
  }

  void End(bool write_oat_patches) {
    builder_->GetDebugInfo()->End();
    if (write_oat_patches) {
      builder_->WritePatches(".debug_info.oat_patches",
                             ArrayRef<const uintptr_t>(debug_info_patches_));
    }
    builder_->WriteSection(".debug_abbrev", &debug_abbrev_buffer_);
    if (!debug_loc_.empty()) {
      builder_->WriteSection(".debug_loc", &debug_loc_);
    }
    if (!debug_ranges_.empty()) {
      builder_->WriteSection(".debug_ranges", &debug_ranges_);
    }
  }

 private:
  ElfBuilder<ElfTypes>* builder_;
  std::vector<uintptr_t> debug_info_patches_;
  std::vector<uint8_t> debug_abbrev_buffer_;
  DebugAbbrevWriter<> debug_abbrev_;
  std::vector<uint8_t> debug_loc_;
  std::vector<uint8_t> debug_ranges_;

  std::unordered_set<const char*> defined_dex_classes_;  // For CHECKs only.
};

// Helper class to write one compilation unit.
// It holds helper methods and temporary state.
class CompilationUnitWriter {
 public:
  explicit CompilationUnitWriter(DebugInfoWriter* owner)
    : owner_(owner),
      info_(Is64BitInstructionSet(owner_->builder_->GetIsa()), &owner->debug_abbrev_) {
  }

  void Write(const CompilationUnit& compilation_unit) {
    CHECK(!compilation_unit.methods_.empty());
    const Elf_Addr text_address = owner_->builder_->GetText()->Exists()
        ? owner_->builder_->GetText()->GetAddress()
        : 0;
    const uintptr_t cu_size = compilation_unit.high_pc_ - compilation_unit.low_pc_;

    info_.StartTag(DW_TAG_compile_unit);
    info_.WriteString(DW_AT_producer, "Android dex2oat");
    info_.WriteData1(DW_AT_language, DW_LANG_Java);
    info_.WriteString(DW_AT_comp_dir, "$JAVA_SRC_ROOT");
    info_.WriteAddr(DW_AT_low_pc, text_address + compilation_unit.low_pc_);
    info_.WriteUdata(DW_AT_high_pc, dchecked_integral_cast<uint32_t>(cu_size));
    info_.WriteSecOffset(DW_AT_stmt_list, compilation_unit.debug_line_offset_);

    const char* last_dex_class_desc = nullptr;
    for (auto mi : compilation_unit.methods_) {
      const DexFile* dex = mi->dex_file_;
      const DexFile::CodeItem* dex_code = mi->code_item_;
      const DexFile::MethodId& dex_method = dex->GetMethodId(mi->dex_method_index_);
      const DexFile::ProtoId& dex_proto = dex->GetMethodPrototype(dex_method);
      const DexFile::TypeList* dex_params = dex->GetProtoParameters(dex_proto);
      const char* dex_class_desc = dex->GetMethodDeclaringClassDescriptor(dex_method);
      const bool is_static = (mi->access_flags_ & kAccStatic) != 0;

      // Enclose the method in correct class definition.
      if (last_dex_class_desc != dex_class_desc) {
        if (last_dex_class_desc != nullptr) {
          EndClassTag();
        }
        // Write reference tag for the class we are about to declare.
        size_t reference_tag_offset = info_.StartTag(DW_TAG_reference_type);
        type_cache_.emplace(std::string(dex_class_desc), reference_tag_offset);
        size_t type_attrib_offset = info_.size();
        info_.WriteRef4(DW_AT_type, 0);
        info_.EndTag();
        // Declare the class that owns this method.
        size_t class_offset = StartClassTag(dex_class_desc);
        info_.UpdateUint32(type_attrib_offset, class_offset);
        info_.WriteFlagPresent(DW_AT_declaration);
        // Check that each class is defined only once.
        bool unique = owner_->defined_dex_classes_.insert(dex_class_desc).second;
        CHECK(unique) << "Redefinition of " << dex_class_desc;
        last_dex_class_desc = dex_class_desc;
      }

      int start_depth = info_.Depth();
      info_.StartTag(DW_TAG_subprogram);
      WriteName(dex->GetMethodName(dex_method));
      info_.WriteAddr(DW_AT_low_pc, text_address + mi->low_pc_);
      info_.WriteUdata(DW_AT_high_pc, dchecked_integral_cast<uint32_t>(mi->high_pc_-mi->low_pc_));
      std::vector<uint8_t> expr_buffer;
      Expression expr(&expr_buffer);
      expr.WriteOpCallFrameCfa();
      info_.WriteExprLoc(DW_AT_frame_base, expr);
      WriteLazyType(dex->GetReturnTypeDescriptor(dex_proto));

      // Write parameters. DecodeDebugLocalInfo returns them as well, but it does not
      // guarantee order or uniqueness so it is safer to iterate over them manually.
      // DecodeDebugLocalInfo might not also be available if there is no debug info.
      std::vector<const char*> param_names = GetParamNames(mi);
      uint32_t arg_reg = 0;
      if (!is_static) {
        info_.StartTag(DW_TAG_formal_parameter);
        WriteName("this");
        info_.WriteFlagPresent(DW_AT_artificial);
        WriteLazyType(dex_class_desc);
        if (dex_code != nullptr) {
          // Write the stack location of the parameter.
          const uint32_t vreg = dex_code->registers_size_ - dex_code->ins_size_ + arg_reg;
          const bool is64bitValue = false;
          WriteRegLocation(mi, vreg, is64bitValue, compilation_unit.low_pc_);
        }
        arg_reg++;
        info_.EndTag();
      }
      if (dex_params != nullptr) {
        for (uint32_t i = 0; i < dex_params->Size(); ++i) {
          info_.StartTag(DW_TAG_formal_parameter);
          // Parameter names may not be always available.
          if (i < param_names.size()) {
            WriteName(param_names[i]);
          }
          // Write the type.
          const char* type_desc = dex->StringByTypeIdx(dex_params->GetTypeItem(i).type_idx_);
          WriteLazyType(type_desc);
          const bool is64bitValue = type_desc[0] == 'D' || type_desc[0] == 'J';
          if (dex_code != nullptr) {
            // Write the stack location of the parameter.
            const uint32_t vreg = dex_code->registers_size_ - dex_code->ins_size_ + arg_reg;
            WriteRegLocation(mi, vreg, is64bitValue, compilation_unit.low_pc_);
          }
          arg_reg += is64bitValue ? 2 : 1;
          info_.EndTag();
        }
        if (dex_code != nullptr) {
          DCHECK_EQ(arg_reg, dex_code->ins_size_);
        }
      }

      // Write local variables.
      LocalInfos local_infos;
      if (dex->DecodeDebugLocalInfo(dex_code,
                                    is_static,
                                    mi->dex_method_index_,
                                    LocalInfoCallback,
                                    &local_infos)) {
        for (const DexFile::LocalInfo& var : local_infos) {
          if (var.reg_ < dex_code->registers_size_ - dex_code->ins_size_) {
            info_.StartTag(DW_TAG_variable);
            WriteName(var.name_);
            WriteLazyType(var.descriptor_);
            bool is64bitValue = var.descriptor_[0] == 'D' || var.descriptor_[0] == 'J';
            WriteRegLocation(mi, var.reg_, is64bitValue, compilation_unit.low_pc_,
                             var.start_address_, var.end_address_);
            info_.EndTag();
          }
        }
      }

      info_.EndTag();
      CHECK_EQ(info_.Depth(), start_depth);  // Balanced start/end.
    }
    if (last_dex_class_desc != nullptr) {
      EndClassTag();
    }
    FinishLazyTypes();
    CloseNamespacesAboveDepth(0);
    info_.EndTag();  // DW_TAG_compile_unit
    CHECK_EQ(info_.Depth(), 0);
    std::vector<uint8_t> buffer;
    buffer.reserve(info_.data()->size() + KB);
    const size_t offset = owner_->builder_->GetDebugInfo()->GetSize();
    // All compilation units share single table which is at the start of .debug_abbrev.
    const size_t debug_abbrev_offset = 0;
    WriteDebugInfoCU(debug_abbrev_offset, info_, offset, &buffer, &owner_->debug_info_patches_);
    owner_->builder_->GetDebugInfo()->WriteFully(buffer.data(), buffer.size());
  }

  void Write(const ArrayRef<mirror::Class*>& types) SHARED_REQUIRES(Locks::mutator_lock_) {
    info_.StartTag(DW_TAG_compile_unit);
    info_.WriteString(DW_AT_producer, "Android dex2oat");
    info_.WriteData1(DW_AT_language, DW_LANG_Java);

    // Base class references to be patched at the end.
    std::map<size_t, mirror::Class*> base_class_references;

    // Already written declarations or definitions.
    std::map<mirror::Class*, size_t> class_declarations;

    std::vector<uint8_t> expr_buffer;
    for (mirror::Class* type : types) {
      if (type->IsPrimitive()) {
        // For primitive types the definition and the declaration is the same.
        if (type->GetPrimitiveType() != Primitive::kPrimVoid) {
          WriteTypeDeclaration(type->GetDescriptor(nullptr));
        }
      } else if (type->IsArrayClass()) {
        mirror::Class* element_type = type->GetComponentType();
        uint32_t component_size = type->GetComponentSize();
        uint32_t data_offset = mirror::Array::DataOffset(component_size).Uint32Value();
        uint32_t length_offset = mirror::Array::LengthOffset().Uint32Value();

        CloseNamespacesAboveDepth(0);  // Declare in root namespace.
        info_.StartTag(DW_TAG_array_type);
        std::string descriptor_string;
        WriteLazyType(element_type->GetDescriptor(&descriptor_string));
        WriteLinkageName(type);
        info_.WriteUdata(DW_AT_data_member_location, data_offset);
        info_.StartTag(DW_TAG_subrange_type);
        Expression count_expr(&expr_buffer);
        count_expr.WriteOpPushObjectAddress();
        count_expr.WriteOpPlusUconst(length_offset);
        count_expr.WriteOpDerefSize(4);  // Array length is always 32-bit wide.
        info_.WriteExprLoc(DW_AT_count, count_expr);
        info_.EndTag();  // DW_TAG_subrange_type.
        info_.EndTag();  // DW_TAG_array_type.
      } else if (type->IsInterface()) {
        // Skip.  Variables cannot have an interface as a dynamic type.
        // We do not expose the interface information to the debugger in any way.
      } else {
        std::string descriptor_string;
        const char* desc = type->GetDescriptor(&descriptor_string);
        size_t class_offset = StartClassTag(desc);
        class_declarations.emplace(type, class_offset);

        if (!type->IsVariableSize()) {
          info_.WriteUdata(DW_AT_byte_size, type->GetObjectSize());
        }

        WriteLinkageName(type);

        if (type->IsObjectClass()) {
          // Generate artificial member which is used to get the dynamic type of variable.
          // The run-time value of this field will correspond to linkage name of some type.
          // We need to do it only once in j.l.Object since all other types inherit it.
          info_.StartTag(DW_TAG_member);
          WriteName(".dynamic_type");
          WriteLazyType(sizeof(uintptr_t) == 8 ? "J" : "I");
          info_.WriteFlagPresent(DW_AT_artificial);
          // Create DWARF expression to get the value of the methods_ field.
          Expression expr(&expr_buffer);
          // The address of the object has been implicitly pushed on the stack.
          // Dereference the klass_ field of Object (32-bit; possibly poisoned).
          DCHECK_EQ(type->ClassOffset().Uint32Value(), 0u);
          DCHECK_EQ(sizeof(mirror::HeapReference<mirror::Class>), 4u);
          expr.WriteOpDerefSize(4);
          if (kPoisonHeapReferences) {
            expr.WriteOpNeg();
            // DWARF stack is pointer sized. Ensure that the high bits are clear.
            expr.WriteOpConstu(0xFFFFFFFF);
            expr.WriteOpAnd();
          }
          // Add offset to the methods_ field.
          expr.WriteOpPlusUconst(mirror::Class::MethodsOffset().Uint32Value());
          // Top of stack holds the location of the field now.
          info_.WriteExprLoc(DW_AT_data_member_location, expr);
          info_.EndTag();  // DW_TAG_member.
        }

        // Base class.
        mirror::Class* base_class = type->GetSuperClass();
        if (base_class != nullptr) {
          info_.StartTag(DW_TAG_inheritance);
          base_class_references.emplace(info_.size(), base_class);
          info_.WriteRef4(DW_AT_type, 0);
          info_.WriteUdata(DW_AT_data_member_location, 0);
          info_.WriteSdata(DW_AT_accessibility, DW_ACCESS_public);
          info_.EndTag();  // DW_TAG_inheritance.
        }

        // Member variables.
        for (uint32_t i = 0, count = type->NumInstanceFields(); i < count; ++i) {
          ArtField* field = type->GetInstanceField(i);
          info_.StartTag(DW_TAG_member);
          WriteName(field->GetName());
          WriteLazyType(field->GetTypeDescriptor());
          info_.WriteUdata(DW_AT_data_member_location, field->GetOffset().Uint32Value());
          uint32_t access_flags = field->GetAccessFlags();
          if (access_flags & kAccPublic) {
            info_.WriteSdata(DW_AT_accessibility, DW_ACCESS_public);
          } else if (access_flags & kAccProtected) {
            info_.WriteSdata(DW_AT_accessibility, DW_ACCESS_protected);
          } else if (access_flags & kAccPrivate) {
            info_.WriteSdata(DW_AT_accessibility, DW_ACCESS_private);
          }
          info_.EndTag();  // DW_TAG_member.
        }

        if (type->IsStringClass()) {
          // Emit debug info about an artifical class member for java.lang.String which represents
          // the first element of the data stored in a string instance. Consumers of the debug
          // info will be able to read the content of java.lang.String based on the count (real
          // field) and based on the location of this data member.
          info_.StartTag(DW_TAG_member);
          WriteName("value");
          // We don't support fields with C like array types so we just say its type is java char.
          WriteLazyType("C");  // char.
          info_.WriteUdata(DW_AT_data_member_location,
                           mirror::String::ValueOffset().Uint32Value());
          info_.WriteSdata(DW_AT_accessibility, DW_ACCESS_private);
          info_.EndTag();  // DW_TAG_member.
        }

        EndClassTag();
      }
    }

    // Write base class declarations.
    for (const auto& base_class_reference : base_class_references) {
      size_t reference_offset = base_class_reference.first;
      mirror::Class* base_class = base_class_reference.second;
      const auto& it = class_declarations.find(base_class);
      if (it != class_declarations.end()) {
        info_.UpdateUint32(reference_offset, it->second);
      } else {
        // Declare base class.  We can not use the standard WriteLazyType
        // since we want to avoid the DW_TAG_reference_tag wrapping.
        std::string tmp_storage;
        const char* base_class_desc = base_class->GetDescriptor(&tmp_storage);
        size_t base_class_declaration_offset = StartClassTag(base_class_desc);
        info_.WriteFlagPresent(DW_AT_declaration);
        WriteLinkageName(base_class);
        EndClassTag();
        class_declarations.emplace(base_class, base_class_declaration_offset);
        info_.UpdateUint32(reference_offset, base_class_declaration_offset);
      }
    }

    FinishLazyTypes();
    CloseNamespacesAboveDepth(0);
    info_.EndTag();  // DW_TAG_compile_unit.
    CHECK_EQ(info_.Depth(), 0);
    std::vector<uint8_t> buffer;
    buffer.reserve(info_.data()->size() + KB);
    const size_t offset = owner_->builder_->GetDebugInfo()->GetSize();
    // All compilation units share single table which is at the start of .debug_abbrev.
    const size_t debug_abbrev_offset = 0;
    WriteDebugInfoCU(debug_abbrev_offset, info_, offset, &buffer, &owner_->debug_info_patches_);
    owner_->builder_->GetDebugInfo()->WriteFully(buffer.data(), buffer.size());
  }

  // Linkage name uniquely identifies type.
  // It is used to determine the dynamic type of objects.
  // We use the methods_ field of class since it is unique and it is not moved by the GC.
  void WriteLinkageName(mirror::Class* type) SHARED_REQUIRES(Locks::mutator_lock_) {
    auto* methods_ptr = type->GetMethodsPtr();
    if (methods_ptr == nullptr) {
      // Some types might have no methods.  Allocate empty array instead.
      LinearAlloc* allocator = Runtime::Current()->GetLinearAlloc();
      void* storage = allocator->Alloc(Thread::Current(), sizeof(LengthPrefixedArray<ArtMethod>));
      methods_ptr = new (storage) LengthPrefixedArray<ArtMethod>(0);
      type->SetMethodsPtr(methods_ptr, 0, 0);
      DCHECK(type->GetMethodsPtr() != nullptr);
    }
    char name[32];
    snprintf(name, sizeof(name), "0x%" PRIXPTR, reinterpret_cast<uintptr_t>(methods_ptr));
    info_.WriteString(DW_AT_linkage_name, name);
  }

  // Some types are difficult to define as we go since they need
  // to be enclosed in the right set of namespaces. Therefore we
  // just define all types lazily at the end of compilation unit.
  void WriteLazyType(const char* type_descriptor) {
    if (type_descriptor != nullptr && type_descriptor[0] != 'V') {
      lazy_types_.emplace(std::string(type_descriptor), info_.size());
      info_.WriteRef4(DW_AT_type, 0);
    }
  }

  void FinishLazyTypes() {
    for (const auto& lazy_type : lazy_types_) {
      info_.UpdateUint32(lazy_type.second, WriteTypeDeclaration(lazy_type.first));
    }
    lazy_types_.clear();
  }

 private:
  void WriteName(const char* name) {
    if (name != nullptr) {
      info_.WriteString(DW_AT_name, name);
    }
  }

  // Convert dex type descriptor to DWARF.
  // Returns offset in the compilation unit.
  size_t WriteTypeDeclaration(const std::string& desc) {
    DCHECK(!desc.empty());
    const auto& it = type_cache_.find(desc);
    if (it != type_cache_.end()) {
      return it->second;
    }

    size_t offset;
    if (desc[0] == 'L') {
      // Class type. For example: Lpackage/name;
      size_t class_offset = StartClassTag(desc.c_str());
      info_.WriteFlagPresent(DW_AT_declaration);
      EndClassTag();
      // Reference to the class type.
      offset = info_.StartTag(DW_TAG_reference_type);
      info_.WriteRef(DW_AT_type, class_offset);
      info_.EndTag();
    } else if (desc[0] == '[') {
      // Array type.
      size_t element_type = WriteTypeDeclaration(desc.substr(1));
      CloseNamespacesAboveDepth(0);  // Declare in root namespace.
      size_t array_type = info_.StartTag(DW_TAG_array_type);
      info_.WriteFlagPresent(DW_AT_declaration);
      info_.WriteRef(DW_AT_type, element_type);
      info_.EndTag();
      offset = info_.StartTag(DW_TAG_reference_type);
      info_.WriteRef4(DW_AT_type, array_type);
      info_.EndTag();
    } else {
      // Primitive types.
      DCHECK_EQ(desc.size(), 1u);

      const char* name;
      uint32_t encoding;
      uint32_t byte_size;
      switch (desc[0]) {
      case 'B':
        name = "byte";
        encoding = DW_ATE_signed;
        byte_size = 1;
        break;
      case 'C':
        name = "char";
        encoding = DW_ATE_UTF;
        byte_size = 2;
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
        byte_size = 2;
        break;
      case 'Z':
        name = "boolean";
        encoding = DW_ATE_boolean;
        byte_size = 1;
        break;
      case 'V':
        LOG(FATAL) << "Void type should not be encoded";
        UNREACHABLE();
      default:
        LOG(FATAL) << "Unknown dex type descriptor: \"" << desc << "\"";
        UNREACHABLE();
      }
      CloseNamespacesAboveDepth(0);  // Declare in root namespace.
      offset = info_.StartTag(DW_TAG_base_type);
      WriteName(name);
      info_.WriteData1(DW_AT_encoding, encoding);
      info_.WriteData1(DW_AT_byte_size, byte_size);
      info_.EndTag();
    }

    type_cache_.emplace(desc, offset);
    return offset;
  }

  // Start DW_TAG_class_type tag nested in DW_TAG_namespace tags.
  // Returns offset of the class tag in the compilation unit.
  size_t StartClassTag(const char* desc) {
    std::string name = SetNamespaceForClass(desc);
    size_t offset = info_.StartTag(DW_TAG_class_type);
    WriteName(name.c_str());
    return offset;
  }

  void EndClassTag() {
    info_.EndTag();
  }

  // Set the current namespace nesting to one required by the given class.
  // Returns the class name with namespaces, 'L', and ';' stripped.
  std::string SetNamespaceForClass(const char* desc) {
    DCHECK(desc != nullptr && desc[0] == 'L');
    desc++;  // Skip the initial 'L'.
    size_t depth = 0;
    for (const char* end; (end = strchr(desc, '/')) != nullptr; desc = end + 1, ++depth) {
      // Check whether the name at this depth is already what we need.
      if (depth < current_namespace_.size()) {
        const std::string& name = current_namespace_[depth];
        if (name.compare(0, name.size(), desc, end - desc) == 0) {
          continue;
        }
      }
      // Otherwise we need to open a new namespace tag at this depth.
      CloseNamespacesAboveDepth(depth);
      info_.StartTag(DW_TAG_namespace);
      std::string name(desc, end - desc);
      WriteName(name.c_str());
      current_namespace_.push_back(std::move(name));
    }
    CloseNamespacesAboveDepth(depth);
    return std::string(desc, strchr(desc, ';') - desc);
  }

  // Close namespace tags to reach the given nesting depth.
  void CloseNamespacesAboveDepth(size_t depth) {
    DCHECK_LE(depth, current_namespace_.size());
    while (current_namespace_.size() > depth) {
      info_.EndTag();
      current_namespace_.pop_back();
    }
  }

  // For access to the ELF sections.
  DebugInfoWriter<ElfTypes>* owner_;
  // Temporary buffer to create and store the entries.
  DebugInfoEntryWriter<> info_;
  // Cache of already translated type descriptors.
  std::map<std::string, size_t> type_cache_;  // type_desc -> definition_offset.
  // 32-bit references which need to be resolved to a type later.
  // Given type may be used multiple times.  Therefore we need a multimap.
  std::multimap<std::string, size_t> lazy_types_;  // type_desc -> patch_offset.
  // The current set of open namespace tags which are active and not closed yet.
  std::vector<std::string> current_namespace_;
};

