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

#include "string.h"

#include "array.h"
#include "class-inl.h"
#include "gc/accounting/card_table-inl.h"
#include "intern_table.h"
#include "object-inl.h"
#include "runtime.h"
#include "sirt_ref.h"
#include "thread.h"
#include "utf-inl.h"
#include "well_known_classes.h"

namespace art {
namespace mirror {

void String::ComputeHashCode() {
  SetHashCode(ComputeUtf16Hash(GetValue(), GetLength()));
}

int32_t String::GetUtfLength() {
  return CountUtf8Bytes(GetValue(), GetLength());
}

int32_t String::FastIndexOf(int32_t ch, int32_t start) {
  int32_t count = GetLength();
  if (start < 0) {
    start = 0;
  } else if (start > count) {
    start = count;
  }
  const uint16_t* chars = GetValue();
  const uint16_t* p = chars + start;
  const uint16_t* end = chars + count;
  while (p < end) {
    if (*p++ == ch) {
      return (p - 1) - chars;
    }
  }
  return -1;
}

// TODO: get global references for these
Class* String::java_lang_String_ = NULL;

void String::SetClass(Class* java_lang_String) {
  CHECK(java_lang_String_ == NULL);
  CHECK(java_lang_String != NULL);
  java_lang_String_ = java_lang_String;
}

void String::ResetClass() {
  CHECK(java_lang_String_ != NULL);
  java_lang_String_ = NULL;
}

String* String::Intern() {
  return Runtime::Current()->GetInternTable()->InternWeak(this);
}

int32_t String::GetHashCode() {
  int32_t result = GetField32(OFFSET_OF_OBJECT_MEMBER(String, hash_code_), false);
  if (result == 0) {
    ComputeHashCode();
  }
  result = GetField32(OFFSET_OF_OBJECT_MEMBER(String, hash_code_), false);
  DCHECK(result != 0 || ComputeUtf16Hash(GetValue(), GetLength()) == 0)
          << ToModifiedUtf8() << " " << result;
  return result;
}

int32_t String::GetLength() {
  int32_t result = GetField32(OFFSET_OF_OBJECT_MEMBER(String, count_), false);
  DCHECK_GE(result, 0);
  return result;
}

uint16_t String::CharAt(int32_t index) {
  // TODO: do we need this? Equals is the only caller, and could
  // bounds check itself.
  DCHECK_GE(count_, 0);  // ensures the unsigned comparison is safe.
  if (UNLIKELY(static_cast<uint32_t>(index) >= static_cast<uint32_t>(count_))) {
    Thread* self = Thread::Current();
    ThrowLocation throw_location = self->GetCurrentLocationForThrow();
    self->ThrowNewExceptionF(throw_location, "Ljava/lang/StringIndexOutOfBoundsException;",
                             "length=%i; index=%i", count_, index);
    return 0;
  }
  return GetValue()[index];
}

String* String::AllocFromUtf16(Thread* self,
                               int32_t utf16_length,
                               const uint16_t* utf16_data_in,
                               int32_t hash_code) {
  CHECK(utf16_data_in != nullptr || utf16_length == 0);
  String* string = Alloc(self, utf16_length);
  if (UNLIKELY(string == nullptr)) {
    return nullptr;
  }
  uint16_t* array = const_cast<uint16_t*>(string->GetValue());
  if (UNLIKELY(array == nullptr)) {
    return nullptr;
  }
  memcpy(array, utf16_data_in, utf16_length * sizeof(uint16_t));
  if (hash_code != 0) {
    DCHECK_EQ(hash_code, ComputeUtf16Hash(utf16_data_in, utf16_length));
    string->SetHashCode(hash_code);
  } else {
    string->ComputeHashCode();
  }
  return string;
}

String* String::AllocFromBytes(Thread* self,
                              int32_t byte_length,
                              const uint8_t* byte_data_in,
                              int32_t high_byte,
                              int32_t hash_code) {
  CHECK(byte_data_in != nullptr || byte_length == 0);
  String* string = Alloc(self, byte_length);
  if (UNLIKELY(string == nullptr)) {
    return nullptr;
  }
  uint16_t* array = const_cast<uint16_t*>(string->GetValue());
  if (UNLIKELY(array == nullptr)) {
    return nullptr;
  }
  high_byte <<= 8;
  for (int i = 0; i < byte_length; i++) {
    array[i] = high_byte + (byte_data_in[i] & 0xFF);
  }
  if (hash_code != 0) {
    string->SetHashCode(hash_code);
  } else {
    string->ComputeHashCode();
  }
  return string;
}

String* String::AllocFromModifiedUtf8(Thread* self, const char* utf) {
  if (UNLIKELY(utf == nullptr)) {
    return nullptr;
  }
  size_t char_count = CountModifiedUtf8Chars(utf);
  return AllocFromModifiedUtf8(self, char_count, utf);
}

String* String::AllocFromModifiedUtf8(Thread* self, int32_t utf16_length,
                                      const char* utf8_data_in) {
  String* string = Alloc(self, utf16_length);
  if (UNLIKELY(string == nullptr)) {
    return nullptr;
  }
  uint16_t* utf16_data_out = const_cast<uint16_t*>(string->GetValue());
  ConvertModifiedUtf8ToUtf16(utf16_data_out, utf8_data_in);
  string->ComputeHashCode();
  return string;
}

String* String::Alloc(Thread* self, int32_t utf16_length) {
  size_t header_size = sizeof(String);
  size_t data_size = sizeof(uint16_t) * utf16_length;
  size_t size = header_size + data_size;
  Class* java_lang_String = GetJavaLangString();

  // Check for overflow and throw OutOfMemoryError if this was an unreasonable request.
  if (UNLIKELY(size < data_size)) {
    self->ThrowOutOfMemoryError(StringPrintf("%s of length %d would overflow",
                                             PrettyDescriptor(java_lang_String).c_str(),
                                             utf16_length).c_str());
    return NULL;
  }

  gc::Heap* heap = Runtime::Current()->GetHeap();
  String* string = down_cast<String*>(heap->AllocObject<true>(self, java_lang_String, size));
  if (string != NULL) {
    string->SetCount(utf16_length);
  }
  return string;
}

bool String::Equals(String* that) {
  if (this == that) {
    // Quick reference equality test
    return true;
  } else if (that == NULL) {
    // Null isn't an instanceof anything
    return false;
  } else if (this->GetLength() != that->GetLength()) {
    // Quick length inequality test
    return false;
  } else {
    // Note: don't short circuit on hash code as we're presumably here as the
    // hash code was already equal
    for (int32_t i = 0; i < that->GetLength(); ++i) {
      if (this->CharAt(i) != that->CharAt(i)) {
        return false;
      }
    }
    return true;
  }
}

bool String::Equals(const uint16_t* that_chars, int32_t that_offset, int32_t that_length) {
  if (this->GetLength() != that_length) {
    return false;
  } else {
    for (int32_t i = 0; i < that_length; ++i) {
      if (this->CharAt(i) != that_chars[that_offset + i]) {
        return false;
      }
    }
    return true;
  }
}

bool String::Equals(const char* modified_utf8) {
  for (int32_t i = 0; i < GetLength(); ++i) {
    uint16_t ch = GetUtf16FromUtf8(&modified_utf8);
    if (ch == '\0' || ch != CharAt(i)) {
      return false;
    }
  }
  return *modified_utf8 == '\0';
}

bool String::Equals(const StringPiece& modified_utf8) {
  const char* p = modified_utf8.data();
  for (int32_t i = 0; i < GetLength(); ++i) {
    uint16_t ch = GetUtf16FromUtf8(&p);
    if (ch != CharAt(i)) {
      return false;
    }
  }
  return true;
}

// Create a modified UTF-8 encoded std::string from a java/lang/String object.
std::string String::ToModifiedUtf8() {
  const uint16_t* chars = GetValue();
  size_t byte_count = GetUtfLength();
  std::string result(byte_count, static_cast<char>(0));
  ConvertUtf16ToModifiedUtf8(&result[0], chars, GetLength());
  return result;
}

#ifdef HAVE__MEMCMP16
// "count" is in 16-bit units.
extern "C" uint32_t __memcmp16(const uint16_t* s0, const uint16_t* s1, size_t count);
#define MemCmp16 __memcmp16
#else
static uint32_t MemCmp16(const uint16_t* s0, const uint16_t* s1, size_t count) {
  for (size_t i = 0; i < count; i++) {
    if (s0[i] != s1[i]) {
      return static_cast<int32_t>(s0[i]) - static_cast<int32_t>(s1[i]);
    }
  }
  return 0;
}
#endif

int32_t String::CompareTo(String* rhs) {
  // Quick test for comparison of a string with itself.
  String* lhs = this;
  if (lhs == rhs) {
    return 0;
  }
  // TODO: is this still true?
  // The annoying part here is that 0x00e9 - 0xffff != 0x00ea,
  // because the interpreter converts the characters to 32-bit integers
  // *without* sign extension before it subtracts them (which makes some
  // sense since "char" is unsigned).  So what we get is the result of
  // 0x000000e9 - 0x0000ffff, which is 0xffff00ea.
  int lhsCount = lhs->GetLength();
  int rhsCount = rhs->GetLength();
  int countDiff = lhsCount - rhsCount;
  int minCount = (countDiff < 0) ? lhsCount : rhsCount;
  const uint16_t* lhsChars = lhs->GetValue();
  const uint16_t* rhsChars = rhs->GetValue();
  int otherRes = MemCmp16(lhsChars, rhsChars, minCount);
  if (otherRes != 0) {
    return otherRes;
  }
  return countDiff;
}

void String::VisitRoots(RootCallback* callback, void* arg) {
  if (java_lang_String_ != nullptr) {
    java_lang_String_ = down_cast<Class*>(callback(java_lang_String_, arg, 0, kRootStickyClass));
  }
}

CharArray* String::ToCharArray(Thread* self) {
  CharArray* result = CharArray::Alloc(self, GetLength());
  memcpy(result->GetData(), GetValue(), GetLength() * sizeof(uint16_t));
  return result;
}

uint32_t String::GetStringFactoryMethodIndex(std::string signature) {
  if (signature == "()V") {
    return kEmptyString;
  }
  if (signature == "([B)V") {
    return kStringFromBytes_B;
  }
  if (signature == "([BI)V") {
    return kStringFromBytes_BI;
  }
  if (signature == "([BII)V") {
    return kStringFromBytes_BII;
  }
  if (signature == "([BIII)V") {
    return kStringFromBytes_BIII;
  }
  if (signature == "([BIILjava/lang/String;)V") {
    return kStringFromBytes_BIIString;
  }
  if (signature == "([BLjava/lang/String;)V") {
    return kStringFromBytes_BString;
  }
  if (signature == "([BIILjava/nio/charset/Charset;)V") {
    return kStringFromBytes_BIICharset;
  }
  if (signature == "([BLjava/nio/charset/Charset;)V") {
    return kStringFromBytes_BCharset;
  }
  if (signature == "([C)V") {
    return kStringFromChars_C;
  }
  if (signature == "([CII)V") {
    return kStringFromChars_CII;
  }
  if (signature == "(II[C)V") {
    return kStringFromCharsNoCheck;
  }
  if (signature == "(Ljava/lang/String;)V") {
    return kStringFromString;
  }
  if (signature == "(Ljava/lang/StringBuffer;)V") {
    return kStringFromStringBuffer;
  }
  if (signature == "([III)V") {
    return kStringFromCodePoints;
  }
  if (signature == "(Ljava/lang/StringBuilder;)V") {
    return kStringFromStringBuilder;
  }
  LOG(FATAL) << "Invalid string init signature for string factory: " << signature;
  return 0;
}

std::string String::GetStringFactoryMethodSignature(uint32_t index) {
  switch (index) {
    case kEmptyString:
      return "()V";
    case kStringFromBytes_B:
      return "([B)V";
    case kStringFromBytes_BI:
      return "([BI)V";
    case kStringFromBytes_BII:
      return "([BII)V";
    case kStringFromBytes_BIII:
      return "([BIII)V";
    case kStringFromBytes_BIIString:
      return "([BIILjava/lang/String;)V";
    case kStringFromBytes_BString:
      return "([BLjava/lang/String;)V";
    case kStringFromBytes_BIICharset:
      return "([BIILjava/nio/charset/Charset;)V";
    case kStringFromBytes_BCharset:
      return "([BLjava/nio/charset/Charset;)V";
    case kStringFromChars_C:
      return "([C)V";
    case kStringFromChars_CII:
      return "([CII)V";
    case kStringFromCharsNoCheck:
      return "(II[C)V";
    case kStringFromString:
      return "(Ljava/lang/String;)V";
    case kStringFromStringBuffer:
      return "(Ljava/lang/StringBuffer;)V";
    case kStringFromCodePoints:
      return "([III)V";
    case kStringFromStringBuilder:
      return "(Ljava/lang/StringBuilder;)V";
  }
  LOG(FATAL) << "Invalid index for string factory: " << index;
  return NULL;
}

const char* String::GetStringFactoryMethodName(std::string signature) {
  if (signature == "()V") {
    return "newEmptyString";
  }
  if (signature == "([B)V" || signature == "([BI)V" || signature == "([BII)V" ||
      signature == "([BIII)V" || signature == "([BIILjava/lang/String;)V" ||
      signature == "([BLjava/lang/String;)V" || signature == "([BIILjava/nio/charset/Charset;)V" ||
      signature == "([BLjava/nio/charset/Charset;)V") {
    return "newStringFromBytes";
  }
  if (signature == "([C)V" || signature == "([CII)V") {
    return "newStringFromChars";
  }
  if (signature == "(II[C)V") {
    return "newStringFromCharsNoCheck";
  }
  if (signature == "(Ljava/lang/String;)V") {
    return "newStringFromString";
  }
  if (signature == "(Ljava/lang/StringBuffer;)V") {
    return "newStringFromStringBuffer";
  }
  if (signature == "([III)V") {
    return "newStringFromCodePoints";
  }
  if (signature == "(Ljava/lang/StringBuilder;)V") {
    return "newStringFromStringBuilder";
  }
  return NULL;
}

ArtMethod* String::GetStringFactoryMethodForStringInit(std::string signature) {
  mirror::Class* sf_class = WellKnownClasses::ToClass(WellKnownClasses::java_lang_StringFactory);
  StringPiece method_name(GetStringFactoryMethodName(signature));
  signature = signature.substr(0, signature.length() - 1).append("Ljava/lang/String;");
  return sf_class->FindDirectMethod(method_name, signature);
}

}  // namespace mirror
}  // namespace art
