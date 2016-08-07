/*
 * Copyright (C) 2016 The Android Open Source Project
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
 *
 * Header file of an in-memory representation of DEX files.
 */

#ifndef DEX_IR_INCLUDED
#define DEX_IR_INCLUDED

#include <stdint.h>
#include <map>
#include <vector>

#include "dex_file.h"

namespace art {
namespace dex_ir {

// Forward declarations for classes used in containers or pointed to.
class Header;
class StringId;
class TypeId;
class ProtoId;
class FieldId;
class MethodId;
class ClassDef;
class FieldItem;
class MethodItem;
class ArrayItem;
class CodeItem;
class TryItem;
class DebugInfoItem;
class AnnotationSetItem;
class AnnotationsDirectoryItem;
class MapList;
class MapItem;

class Builder {
 private:
  template<class T>
  class Map {
   public:
    Map() { }
    T Lookup(uint32_t index) {
      if (map_.find(index) == map_.end()) {
        return nullptr;
      } else {
        return map_[index];
      }
    }
    void Set(uint32_t index, T value) {
      map_[index] = value;
    }

   private:
    DISALLOW_COPY_AND_ASSIGN(Map);
    std::map<uint32_t, T> map_;
  };

 public:
  Builder(const DexFile& dex_file) : dex_file_(dex_file) { }

  static const Header* Build(const DexFile& dex_file);

  const DexFile& dex_file() const { return dex_file_; }
  Map<const StringId*>* string_ids() { return &string_ids_; }
  Map<const TypeId*>* types() { return &types_; }
  Map<const ProtoId*>* protos() { return &protos_; }
  Map<const FieldId*>* fields() { return &fields_; }
  Map<const MethodId*>* methods() { return &methods_; }

 private:
  DISALLOW_COPY_AND_ASSIGN(Builder);
  const DexFile& dex_file_;
  Map<const StringId*> string_ids_;
  Map<const TypeId*> types_;
  Map<const ProtoId*> protos_;
  Map<const FieldId*> fields_;
  Map<const MethodId*> methods_;
};

class Item {
 public:
  virtual ~Item() { }
  uint32_t offset() const { return offset_; }
  void setOffset(uint32_t offset) { offset_ = offset; }
 protected:
  uint32_t offset_ = 0;
};

class Header : public Item {
 public:
  Header(const DexFile::Header& disk_header, Builder& builder);
  ~Header() OVERRIDE { }

  const uint8_t* magic() const { return magic_; }
  uint32_t checksum() const { return checksum_; }
  const uint8_t* signature() const { return signature_; }
  uint32_t endian_tag() const { return endian_tag_; }
  std::vector<const StringId*>* string_ids() { return &string_ids_; }
  std::vector<const TypeId*>* type_ids() { return &type_ids_; }
  std::vector<const ProtoId*>* proto_ids() { return &proto_ids_; }
  std::vector<const FieldId*>* field_ids() { return &field_ids_; }
  std::vector<const MethodId*>* method_ids() { return &method_ids_; }
  std::vector<const ClassDef*>* classes() { return &classes_; }

 private:
  DISALLOW_COPY_AND_ASSIGN(Header);
  uint8_t magic_[8];
  uint32_t checksum_;
  uint8_t signature_[DexFile::kSha1DigestSize];
  uint32_t endian_tag_;
  std::vector<const StringId*> string_ids_;
  std::vector<const TypeId*> type_ids_;
  std::vector<const ProtoId*> proto_ids_;
  std::vector<const FieldId*> field_ids_;
  std::vector<const MethodId*> method_ids_;
  std::vector<const ClassDef*> classes_;
};

class StringId : public Item {
 public:
  static const StringId* Lookup(uint32_t index, Builder& builder) {
    return builder.string_ids()->Lookup(index);
  }
  static const StringId* Lookup(const DexFile::StringId& string_id, Builder& builder) {
    uint32_t index = builder.dex_file().GetIndexForStringId(string_id);
    const StringId* item = builder.string_ids()->Lookup(index);
    if (item == nullptr) {
      item = new StringId(builder.dex_file().GetStringData(string_id));
      builder.string_ids()->Set(index, item);
    }
    return item;
  }
  ~StringId() OVERRIDE { }

  const char* data() const { return data_; }

 private:
  DISALLOW_COPY_AND_ASSIGN(StringId);
  StringId(const char* data) : data_(strdup(data)) { }
  const char* data_;
};

class TypeId : public Item {
 public:
  TypeId(const DexFile::TypeId& disk_type, Builder& builder) :
    string_(builder.string_ids()->Lookup(disk_type.descriptor_idx_)) { }
  ~TypeId() OVERRIDE { }

  const StringId* string() const { return string_; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TypeId);
  const StringId* string_;
};

class ProtoId : public Item {
 public:
  ProtoId(const DexFile::ProtoId& disk_proto, Builder& builder) {
    shorty_ = builder.string_ids()->Lookup(disk_proto.shorty_idx_);
    return_type_ = builder.types()->Lookup(disk_proto.return_type_idx_);
    DexFileParameterIterator dfpi(builder.dex_file(), disk_proto);
    while (dfpi.HasNext()) {
      parameters_.push_back(builder.types()->Lookup(dfpi.GetTypeIdx()));
    }
  }
  ~ProtoId() OVERRIDE { }

  const StringId* shorty() const { return shorty_; }
  const TypeId* return_type() const { return return_type_; }
  std::vector<const TypeId*>* parameters() { return &parameters_; }

 private:
  DISALLOW_COPY_AND_ASSIGN(ProtoId);
  const StringId* shorty_;
  const TypeId* return_type_;
  std::vector<const TypeId*> parameters_;
};

class FieldId : public Item {
 public:
  FieldId(const DexFile::FieldId& disk_field, Builder& builder) {
    class_def_ = nullptr;
    type_ = builder.types()->Lookup(disk_field.type_idx_);
    name_ = builder.string_ids()->Lookup(disk_field.name_idx_);
  }
  ~FieldId() OVERRIDE { }

  // Breaks the cyclic type dependence between fields and classes.
  void setClass(const ClassDef* class_def) { class_def_ = class_def; }

  const ClassDef* class_def() const { return class_def_; }
  const TypeId* type() const { return type_; }
  const StringId* name() const { return name_; }

 private:
  DISALLOW_COPY_AND_ASSIGN(FieldId);
  const ClassDef* class_def_;
  const TypeId* type_;
  const StringId* name_;
};

class MethodId : public Item {
 public:
  MethodId(const DexFile::MethodId& disk_method, Builder& builder) {
    class_def_ = nullptr;
    proto_ = builder.protos()->Lookup(disk_method.proto_idx_);
    name_ = builder.string_ids()->Lookup(disk_method.name_idx_);
  }
  ~MethodId() OVERRIDE { }

  // Breaks the cyclic type dependence between fields and classes.
  void setClass(const ClassDef* class_def) { class_def_ = class_def; }

  const ClassDef* class_def() const { return class_def_; }
  const ProtoId* proto() const { return proto_; }
  const StringId* name() const { return name_; }

 private:
  DISALLOW_COPY_AND_ASSIGN(MethodId);
  const ClassDef* class_def_;
  const ProtoId* proto_;
  const StringId* name_;
};

class FieldItem : public Item {
 public:
  FieldItem(uint32_t access_flags, const FieldId* field) :
    access_flags_(access_flags), field_(field) { }
  ~FieldItem() OVERRIDE { }

  uint32_t access_flags() const { return access_flags_; }
  const FieldId* field() const { return field_; }

 private:
  DISALLOW_COPY_AND_ASSIGN(FieldItem);
  uint32_t access_flags_;
  const FieldId* field_;
};

class MethodItem : public Item {
 public:
  MethodItem(uint32_t access_flags, const MethodId* method, const CodeItem* code) :
    access_flags_(access_flags), method_(method), code_(code) { }
  ~MethodItem() OVERRIDE { }

  uint32_t access_flags() const { return access_flags_; }
  const MethodId* method() const { return method_; }
  const CodeItem* code() const { return code_; }

 private:
  DISALLOW_COPY_AND_ASSIGN(MethodItem);
  uint32_t access_flags_;
  const MethodId* method_;
  const CodeItem* code_;
};

class ArrayItem : public Item {
 public:
  class NameValuePair {
   public:
    NameValuePair(const StringId* name, const ArrayItem* value) :
      name_(name), value_(value) { }

    const StringId* name() const { return name_; }
    const ArrayItem* value() const { return value_; }

   private:
    DISALLOW_COPY_AND_ASSIGN(NameValuePair);
    const StringId* name_;
    const ArrayItem* value_;
  };

  ArrayItem(Builder& builder, const uint8_t** data, uint8_t type, uint8_t length);
  ArrayItem(Builder& builder, const uint8_t** data);
  ~ArrayItem() OVERRIDE { }

  bool bool_val() const { return item_.bool_val_; }
  int8_t byte_val() const { return item_.byte_val_; }
  int16_t short_val() const { return item_.short_val_; }
  uint16_t char_val() const { return item_.char_val_; }
  int32_t int_val() const { return item_.int_val_; }
  int64_t long_val() const { return item_.long_val_; }
  float float_val() const { return item_.float_val_; }
  double double_val() const { return item_.double_val_; }
  const StringId* string_val() const { return item_.string_val_; }
  const FieldId* field_val() const { return item_.field_val_; }
  const MethodId* method_val() const { return item_.method_val_; }
  std::vector<const ArrayItem*>* annotation_array_val() const {
    return item_.annotation_array_val_;
  }
  const StringId* annotation_annotation_string() const {
    return item_.annotation_annotation_val_.string_;
  }
  const std::vector<const NameValuePair*>* annotation_annotation_nvp_array() const {
    return item_.annotation_annotation_val_.array_;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ArrayItem);
  void Read(Builder& builder, const uint8_t** data, uint8_t type, uint8_t length);
  uint8_t type_;
  union {
    bool bool_val_;
    int8_t byte_val_;
    int16_t short_val_;
    uint16_t char_val_;
    int32_t int_val_;
    int64_t long_val_;
    float float_val_;
    double double_val_;
    const StringId* string_val_;
    const FieldId* field_val_;
    const MethodId* method_val_;
    std::vector<const ArrayItem*>* annotation_array_val_;
    struct {
      const StringId* string_;
      std::vector<const NameValuePair*>* array_;
    } annotation_annotation_val_;
  } item_;
};

class ClassDef : public Item {
 public:
  ClassDef(const DexFile::ClassDef& disk_class_def, Builder& builder);
  ~ClassDef() OVERRIDE { }

  const TypeId* class_type() const { return class_type_; }
  uint32_t access_flags() const { return access_flags_; }
  const TypeId* superclass() const { return superclass_; }
  const std::vector<const TypeId*>* interfaces() const { return &interfaces_; }
  const StringId* source_file() const { return source_file_; }
  const AnnotationsDirectoryItem* annotations() const { return annotations_; }
  const std::vector<const ArrayItem*>* static_values() const { return static_values_; }
  const std::vector<const FieldItem*>* static_fields() const {
    return &class_data_.static_fields_;
  }
  const std::vector<const FieldItem*>* instance_fields() const {
    return &class_data_.instance_fields_;
  }
  const std::vector<const MethodItem*>* direct_methods() const {
    return &class_data_.direct_methods_;
  }
  const std::vector<const MethodItem*>* virtual_methods() const {
    return &class_data_.virtual_methods_;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ClassDef);
  const TypeId* class_type_;
  uint32_t access_flags_;
  const TypeId* superclass_;
  std::vector<const TypeId*> interfaces_;
  const StringId* source_file_;
  const AnnotationsDirectoryItem* annotations_;
  std::vector<const ArrayItem*>* static_values_;
  struct ClassDataItem : public Item {
    std::vector<const FieldItem*> static_fields_;
    std::vector<const FieldItem*> instance_fields_;
    std::vector<const MethodItem*> direct_methods_;
    std::vector<const MethodItem*> virtual_methods_;
  } class_data_;
};

class CodeItem : public Item {
 public:
  CodeItem(const DexFile::CodeItem& disk_code_item, Builder& builder);
  ~CodeItem() OVERRIDE { }

  uint16_t registers_size() const { return registers_size_; }
  uint16_t ins_size() const { return ins_size_; }
  uint16_t outs_size() const { return outs_size_; }
  uint16_t tries_size() const { return tries_size_; }
  std::vector<const DebugInfoItem*>* debug_info() const { return debug_info_; }
  uint32_t insns_size() const { return insns_size_; }
  uint16_t* insns() const { return insns_; }
  std::vector<const TryItem*>* tries() const { return tries_; }

 private:
  DISALLOW_COPY_AND_ASSIGN(CodeItem);
  uint16_t registers_size_;
  uint16_t ins_size_;
  uint16_t outs_size_;
  uint16_t tries_size_;
  std::vector<const DebugInfoItem*>* debug_info_;
  uint32_t insns_size_;
  uint16_t* insns_;
  std::vector<const TryItem*>* tries_;
};

class TryItem : public Item {
 public:
  class CatchHandler {
   public:
    CatchHandler(const TypeId* type_id, uint32_t address) : type_id_(type_id), address_(address) { }

    const TypeId* type_id() const { return type_id_; }
    uint32_t address() const { return address_; }

   private:
    DISALLOW_COPY_AND_ASSIGN(CatchHandler);
    const TypeId* type_id_;
    uint32_t address_;
  };

  TryItem(const DexFile::TryItem& disk_try_item, const DexFile::CodeItem& disk_code_item,
          Builder& builder) {
    start_addr_ = disk_try_item.start_addr_;
    insn_count_ = disk_try_item.insn_count_;
    for (CatchHandlerIterator it(disk_code_item, disk_try_item); it.HasNext(); it.Next()) {
      const uint16_t type_index = it.GetHandlerTypeIndex();
      handlers_.push_back(new CatchHandler(builder.types()->Lookup(type_index),
                                           it.GetHandlerAddress()));
    }
  }
  ~TryItem() OVERRIDE { }

  uint32_t start_addr() const { return start_addr_; }
  uint16_t insn_count() const { return insn_count_; }
  std::vector<const CatchHandler*>* handlers() { return &handlers_; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TryItem);
  uint32_t start_addr_;
  uint16_t insn_count_;
  std::vector<const CatchHandler*> handlers_;
};

// TODO(sehr): implement debug information.
class DebugInfoItem : public Item {
 public:
 private:
  DISALLOW_COPY_AND_ASSIGN(DebugInfoItem);
};

class AnnotationSetItem : public Item {
 public:
  class AnnotationItem {
   public:
    AnnotationItem(uint8_t visibility, const ArrayItem* item) :
      visibility_(visibility), item_(item) { }

    uint8_t visibility() const { return visibility_; }
    const ArrayItem* item() const { return item_; }

   private:
    DISALLOW_COPY_AND_ASSIGN(AnnotationItem);
    uint8_t visibility_;
    const ArrayItem* item_;
  };

  AnnotationSetItem(const DexFile::AnnotationSetItem& disk_annotations_item, Builder& builder);
  ~AnnotationSetItem() OVERRIDE { }

  const std::vector<const AnnotationItem*>* items() const { return &items_; }

 private:
  DISALLOW_COPY_AND_ASSIGN(AnnotationSetItem);
  std::vector<const AnnotationItem*> items_;
};

class AnnotationsDirectoryItem : public Item {
 public:
  class FieldAnnotation {
   public:
    FieldAnnotation(const FieldId* field_id, const AnnotationSetItem* annotation_set_item) :
      field_id_(field_id), annotation_set_item_(annotation_set_item) { }

    const FieldId* field_id() const { return field_id_; }
    const AnnotationSetItem* annotation_set_item() const { return annotation_set_item_; }

   private:
    DISALLOW_COPY_AND_ASSIGN(FieldAnnotation);
    const FieldId* field_id_;
    const AnnotationSetItem* annotation_set_item_;
  };

  class MethodAnnotation {
   public:
    MethodAnnotation(const MethodId* method_id, const AnnotationSetItem* annotation_set_item) :
      method_id_(method_id), annotation_set_item_(annotation_set_item) { }

    const MethodId* method_id() const { return method_id_; }
    const AnnotationSetItem* annotation_set_item() const { return annotation_set_item_; }

   private:
    DISALLOW_COPY_AND_ASSIGN(MethodAnnotation);
    const MethodId* method_id_;
    const AnnotationSetItem* annotation_set_item_;
  };

  class ParameterAnnotation {
   public:
    ParameterAnnotation(const MethodId* method_id,
                        const DexFile::AnnotationSetRefList* annotation_set_ref_list,
                        Builder& builder) :
      method_id_(method_id) {
      for (uint32_t i = 0; i < annotation_set_ref_list->size_; ++i) {
        const DexFile::AnnotationSetItem* annotation_set_item =
            builder.dex_file().GetSetRefItemItem(&annotation_set_ref_list->list_[i]);
        annotations_.push_back(new AnnotationSetItem(*annotation_set_item, builder));
      }
    }

    const MethodId* method_id() const { return method_id_; }
    const std::vector<const AnnotationSetItem*>* annotations() const { return &annotations_; }

   private:
    DISALLOW_COPY_AND_ASSIGN(ParameterAnnotation);
    const MethodId* method_id_;
    std::vector<const AnnotationSetItem*> annotations_;
  };

  AnnotationsDirectoryItem(const DexFile::AnnotationsDirectoryItem* disk_annotations_item,
                           Builder& builder);

  const AnnotationSetItem* class_annotation() const { return class_annotation_; }
  const std::vector<const FieldAnnotation*>* field_annotations() const {
    return &field_annotations_;
  }
  const std::vector<const MethodAnnotation*>* method_annotations() const {
    return &method_annotations_;
  }
  const std::vector<const ParameterAnnotation*>* parameter_annotations() const {
    return &parameter_annotations_;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AnnotationsDirectoryItem);
  const AnnotationSetItem* class_annotation_;
  std::vector<const FieldAnnotation*> field_annotations_;
  std::vector<const MethodAnnotation*> method_annotations_;
  std::vector<const ParameterAnnotation*> parameter_annotations_;
};

// TODO(sehr): implement MapList.
class MapList : public Item {
 public:
 private:
  DISALLOW_COPY_AND_ASSIGN(MapList);
};

class MapItem : public Item {
 public:
 private:
  DISALLOW_COPY_AND_ASSIGN(MapItem);
};

}  // namespace dex_ir
}  // namespace art

#endif  // DEX_IR_INCLUDED
