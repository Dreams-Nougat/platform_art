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

#ifndef ART_RUNTIME_DEX_FILE_INL_H_
#define ART_RUNTIME_DEX_FILE_INL_H_

#include "base/bit_utils.h"
#include "base/logging.h"
#include "base/stringpiece.h"
#include "dex_file.h"
#include "leb128.h"

namespace art {

inline int32_t DexFile::GetStringLength(const StringId& string_id) const {
  const uint8_t* ptr = begin_ + string_id.string_data_off_;
  return DecodeUnsignedLeb128(&ptr);
}

inline const char* DexFile::GetStringDataAndUtf16Length(const StringId& string_id,
                                                        uint32_t* utf16_length) const {
  DCHECK(utf16_length != nullptr) << GetLocation();
  const uint8_t* ptr = begin_ + string_id.string_data_off_;
  *utf16_length = DecodeUnsignedLeb128(&ptr);
  return reinterpret_cast<const char*>(ptr);
}

inline const char* DexFile::GetStringData(const StringId& string_id) const {
  uint32_t ignored;
  return GetStringDataAndUtf16Length(string_id, &ignored);
}

inline const char* DexFile::StringDataAndUtf16LengthByIdx(uint32_t idx,
                                                          uint32_t* utf16_length) const {
  if (idx == kDexNoIndex) {
    *utf16_length = 0;
    return nullptr;
  }
  const StringId& string_id = GetStringId(idx);
  return GetStringDataAndUtf16Length(string_id, utf16_length);
}

inline const char* DexFile::StringDataByIdx(uint32_t idx) const {
  uint32_t unicode_length;
  return StringDataAndUtf16LengthByIdx(idx, &unicode_length);
}

inline const char* DexFile::StringByTypeIdx(dex::TypeIndex idx, uint32_t* unicode_length) const {
  const TypeId& type_id = GetTypeId(idx);
  return StringDataAndUtf16LengthByIdx(type_id.descriptor_idx_, unicode_length);
}

inline const char* DexFile::StringByTypeIdx(dex::TypeIndex idx) const {
  const TypeId& type_id = GetTypeId(idx);
  return StringDataByIdx(type_id.descriptor_idx_);
}

inline const char* DexFile::GetTypeDescriptor(const TypeId& type_id) const {
  return StringDataByIdx(type_id.descriptor_idx_);
}

inline const char* DexFile::GetFieldTypeDescriptor(const FieldId& field_id) const {
  const DexFile::TypeId& type_id = GetTypeId(field_id.type_idx_);
  return GetTypeDescriptor(type_id);
}

inline const char* DexFile::GetFieldName(const FieldId& field_id) const {
  return StringDataByIdx(field_id.name_idx_);
}

inline const char* DexFile::GetMethodDeclaringClassDescriptor(const MethodId& method_id) const {
  const DexFile::TypeId& type_id = GetTypeId(method_id.class_idx_);
  return GetTypeDescriptor(type_id);
}

inline const Signature DexFile::GetMethodSignature(const MethodId& method_id) const {
  return Signature(this, GetProtoId(method_id.proto_idx_));
}

inline const Signature DexFile::GetProtoSignature(const ProtoId& proto_id) const {
  return Signature(this, proto_id);
}

inline const char* DexFile::GetMethodName(const MethodId& method_id) const {
  return StringDataByIdx(method_id.name_idx_);
}

inline const char* DexFile::GetMethodShorty(uint32_t idx) const {
  return StringDataByIdx(GetProtoId(GetMethodId(idx).proto_idx_).shorty_idx_);
}

inline const char* DexFile::GetMethodShorty(const MethodId& method_id) const {
  return StringDataByIdx(GetProtoId(method_id.proto_idx_).shorty_idx_);
}

inline const char* DexFile::GetMethodShorty(const MethodId& method_id, uint32_t* length) const {
  // Using the UTF16 length is safe here as shorties are guaranteed to be ASCII characters.
  return StringDataAndUtf16LengthByIdx(GetProtoId(method_id.proto_idx_).shorty_idx_, length);
}

inline const char* DexFile::GetClassDescriptor(const ClassDef& class_def) const {
  return StringByTypeIdx(class_def.class_idx_);
}

inline const char* DexFile::GetReturnTypeDescriptor(const ProtoId& proto_id) const {
  return StringByTypeIdx(proto_id.return_type_idx_);
}

inline const char* DexFile::GetShorty(uint32_t proto_idx) const {
  const ProtoId& proto_id = GetProtoId(proto_idx);
  return StringDataByIdx(proto_id.shorty_idx_);
}

inline const DexFile::TryItem* DexFile::GetTryItems(const CodeItem& code_item, uint32_t offset) {
  const uint16_t* insns_end_ = &code_item.insns_[code_item.insns_size_in_code_units_];
  return reinterpret_cast<const TryItem*>
      (RoundUp(reinterpret_cast<uintptr_t>(insns_end_), 4)) + offset;
}

static inline bool DexFileStringEquals(const DexFile* df1, uint32_t sidx1,
                                       const DexFile* df2, uint32_t sidx2) {
  uint32_t s1_len;  // Note: utf16 length != mutf8 length.
  const char* s1_data = df1->StringDataAndUtf16LengthByIdx(sidx1, &s1_len);
  uint32_t s2_len;
  const char* s2_data = df2->StringDataAndUtf16LengthByIdx(sidx2, &s2_len);
  return (s1_len == s2_len) && (strcmp(s1_data, s2_data) == 0);
}

inline bool Signature::operator==(const Signature& rhs) const {
  if (dex_file_ == nullptr) {
    return rhs.dex_file_ == nullptr;
  }
  if (rhs.dex_file_ == nullptr) {
    return false;
  }
  if (dex_file_ == rhs.dex_file_) {
    return proto_id_ == rhs.proto_id_;
  }
  uint32_t lhs_shorty_len;  // For a shorty utf16 length == mutf8 length.
  const char* lhs_shorty_data = dex_file_->StringDataAndUtf16LengthByIdx(proto_id_->shorty_idx_,
                                                                         &lhs_shorty_len);
  StringPiece lhs_shorty(lhs_shorty_data, lhs_shorty_len);
  {
    uint32_t rhs_shorty_len;
    const char* rhs_shorty_data =
        rhs.dex_file_->StringDataAndUtf16LengthByIdx(rhs.proto_id_->shorty_idx_,
                                                     &rhs_shorty_len);
    StringPiece rhs_shorty(rhs_shorty_data, rhs_shorty_len);
    if (lhs_shorty != rhs_shorty) {
      return false;  // Shorty mismatch.
    }
  }
  if (lhs_shorty[0] == 'L') {
    const DexFile::TypeId& return_type_id = dex_file_->GetTypeId(proto_id_->return_type_idx_);
    const DexFile::TypeId& rhs_return_type_id =
        rhs.dex_file_->GetTypeId(rhs.proto_id_->return_type_idx_);
    if (!DexFileStringEquals(dex_file_, return_type_id.descriptor_idx_,
                             rhs.dex_file_, rhs_return_type_id.descriptor_idx_)) {
      return false;  // Return type mismatch.
    }
  }
  if (lhs_shorty.find('L', 1) != StringPiece::npos) {
    const DexFile::TypeList* params = dex_file_->GetProtoParameters(*proto_id_);
    const DexFile::TypeList* rhs_params = rhs.dex_file_->GetProtoParameters(*rhs.proto_id_);
    // Both lists are empty or have contents, or else shorty is broken.
    DCHECK_EQ(params == nullptr, rhs_params == nullptr);
    if (params != nullptr) {
      uint32_t params_size = params->Size();
      DCHECK_EQ(params_size, rhs_params->Size());  // Parameter list size must match.
      for (uint32_t i = 0; i < params_size; ++i) {
        const DexFile::TypeId& param_id = dex_file_->GetTypeId(params->GetTypeItem(i).type_idx_);
        const DexFile::TypeId& rhs_param_id =
            rhs.dex_file_->GetTypeId(rhs_params->GetTypeItem(i).type_idx_);
        if (!DexFileStringEquals(dex_file_, param_id.descriptor_idx_,
                                 rhs.dex_file_, rhs_param_id.descriptor_idx_)) {
          return false;  // Parameter type mismatch.
        }
      }
    }
  }
  return true;
}


}  // namespace art

#endif  // ART_RUNTIME_DEX_FILE_INL_H_
