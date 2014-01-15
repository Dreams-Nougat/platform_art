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

#ifndef ART_RUNTIME_MIRROR_STRING_INL_H_
#define ART_RUNTIME_MIRROR_STRING_INL_H_

#include "array.h"
#include "intern_table.h"
#include "runtime.h"
#include "string.h"
#include "thread.h"

#include "class.h"
#include "gc/heap-inl.h"

namespace art {
namespace mirror {

inline String* String::Intern() {
  return Runtime::Current()->GetInternTable()->InternWeak(this);
}

inline uint16_t String::CharAt(int32_t index) {
  if (UNLIKELY((index < 0) || (index >= count_))) {
    Thread* self = Thread::Current();
    ThrowLocation throw_location = self->GetCurrentLocationForThrow();
    self->ThrowNewExceptionF(throw_location, "Ljava/lang/StringIndexOutOfBoundsException;",
                             "length=%i; index=%i", count_, index);
    return 0;
  }
  return GetValue()[index];
}

// Used for setting string count in the allocation code path to ensure it is guarded by a CAS.
class SetStringCountVisitor {
 public:
  explicit SetStringCountVisitor(int32_t count) : count_(count) {
  }

  void operator()(Object* obj, size_t usable_size) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    UNUSED(usable_size);
    // Avoid AsString as object is not yet in live bitmap or allocation stack.
    String* string = down_cast<String*>(obj);
    string->SetCount(count_);
  }

 private:
  const int32_t count_;
};

template<VerifyObjectFlags kVerifyFlags, bool kDoReadBarrier>
inline size_t String::SizeOf() {
  return sizeof(String) + (sizeof(uint16_t) * GetCount<kVerifyFlags>());
}

template <bool kIsInstrumented>
inline String* String::Alloc(Thread* self, int32_t utf16_length, gc::AllocatorType allocator_type) {
  size_t header_size = sizeof(String);
  size_t data_size = sizeof(uint16_t) * utf16_length;
  size_t size = header_size + data_size;
  Class* string_class = GetJavaLangString();

  // Check for overflow and throw OutOfMemoryError if this was an unreasonable request.
  if (UNLIKELY(size < data_size)) {
    self->ThrowOutOfMemoryError(StringPrintf("%s of length %d would overflow",
                                             PrettyDescriptor(string_class).c_str(),
                                             utf16_length).c_str());
    return nullptr;
  }
  gc::Heap* heap = Runtime::Current()->GetHeap();
  SetStringCountVisitor visitor(utf16_length);
  return down_cast<String*>(
      heap->AllocObjectWithAllocator<kIsInstrumented, false>(self, string_class, size,
                                                             allocator_type, visitor));
}

template <bool kIsInstrumented>
inline String* String::AllocFromByteArray(Thread* self, int32_t byte_length,
                                          Handle<ByteArray> array, int32_t offset,
                                          int32_t high_byte, gc::AllocatorType allocator_type) {
  String* string = Alloc<kIsInstrumented>(self, byte_length, allocator_type);
  if (UNLIKELY(string == nullptr)) {
    return nullptr;
  }
  uint8_t* data = reinterpret_cast<uint8_t*>(array->GetData()) + offset;
  uint16_t* new_value = string->GetValue();
  high_byte <<= 8;
  for (int i = 0; i < byte_length; i++) {
    new_value[i] = high_byte + (data[i] & 0xFF);
  }
  return string;
}

template <bool kIsInstrumented>
inline String* String::AllocFromCharArray(Thread* self, int32_t array_length,
                                          Handle<CharArray> array, int32_t offset,
                                          gc::AllocatorType allocator_type) {
  String* new_string = Alloc<kIsInstrumented>(self, array_length, allocator_type);
  if (UNLIKELY(new_string == nullptr)) {
    return nullptr;
  }
  uint16_t* data = array->GetData() + offset;
  uint16_t* new_value = new_string->GetValue();
  memcpy(new_value, data, array_length * sizeof(uint16_t));
  return new_string;
}

template <bool kIsInstrumented>
inline String* String::AllocFromString(Thread* self, int32_t string_length, Handle<String> string,
                                       int32_t offset, gc::AllocatorType allocator_type) {
  String* new_string = Alloc<kIsInstrumented>(self, string_length, allocator_type);
  if (UNLIKELY(new_string == nullptr)) {
    return nullptr;
  }
  uint16_t* data = string->GetValue() + offset;
  uint16_t* new_value = new_string->GetValue();
  memcpy(new_value, data, string_length * sizeof(uint16_t));
  return new_string;
}

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_STRING_INL_H_
