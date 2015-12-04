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

#ifndef ART_RUNTIME_BASE_ARRAY_SLICE_H_
#define ART_RUNTIME_BASE_ARRAY_SLICE_H_

#include "length_prefixed_array.h"
#include "stride_iterator.h"
#include "base/bit_utils.h"
#include "base/casts.h"
#include "base/iteration_range.h"

namespace art {

template<typename T>
class ArraySlice {
 public:
  ArraySlice() : array_(nullptr), size_(0), element_size_(0) {}

  ArraySlice(T* array,
             size_t length,
             size_t element_size = sizeof(T))
      : array_(array),
        size_(dchecked_integral_cast<uint32_t>(length)),
        element_size_(element_size) {
    DCHECK(array_ != nullptr || length == 0);
  }

  ArraySlice(T* array,
             uint32_t start_offset,
             uint32_t end_offset,
             size_t element_size = sizeof(T))
      : array_(nullptr),
        size_(dchecked_integral_cast<uint32_t>(end_offset - start_offset)),
        element_size_(element_size) {
    DCHECK(array_ != nullptr || size_ == 0);
    DCHECK_LE(start_offset, end_offset);
    if (size_ != 0) {
      uintptr_t offset = start_offset * element_size_;
      array_ = *reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(array) + offset);
    }
  }

  ArraySlice(LengthPrefixedArray<T>* array,
             uint32_t start_offset,
             uint32_t end_offset,
             size_t element_size = sizeof(T),
             size_t alignment = alignof(T))
      : array_(nullptr),
        size_(dchecked_integral_cast<uint32_t>(end_offset - start_offset)),
        element_size_(element_size) {
    DCHECK(array != nullptr || size_ == 0);
    if (size_ != 0) {
      DCHECK_LE(start_offset, end_offset);
      DCHECK_LE(start_offset, array->size());
      DCHECK_LE(end_offset, array->size());
      array_ = &array->At(start_offset, element_size_, alignment);
    }
  }

  T& At(size_t index) {
    DCHECK_LT(index, size_);
    return AtUnchecked(index);
  }

  const T& At(size_t index) const {
    DCHECK_LT(index, size_);
    return AtUnchecked(index);
  }

  StrideIterator<T> begin() {
    return StrideIterator<T>(&AtUnchecked(0), element_size_);
  }

  StrideIterator<const T> begin() const {
    return StrideIterator<const T>(&AtUnchecked(0), element_size_);
  }

  StrideIterator<T> end() {
    return StrideIterator<T>(&AtUnchecked(size_), element_size_);
  }

  StrideIterator<const T> end() const {
    return StrideIterator<const T>(&AtUnchecked(size_), element_size_);
  }

  size_t size() const {
    return size_;
  }

  size_t ElementSize() const {
    return element_size_;
  }

 private:
  T& AtUnchecked(size_t index) {
    return *reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(array_) + index * element_size_);
  }

  const T& AtUnchecked(size_t index) const {
    return *reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(array_) + index * element_size_);
  }

  T* array_;
  uint32_t size_;
  size_t element_size_;
};

// Returns empty iteration range if the array is null.
template<typename T>
IterationRange<StrideIterator<T>> MakeIterationRangeFromArraySlice(ArraySlice<T> slice) {
  return slice.size() != 0 ? MakeIterationRange(slice.begin(), slice.end())
                           : MakeEmptyIterationRange(StrideIterator<T>(nullptr, 0));
}

}  // namespace art

#endif  // ART_RUNTIME_BASE_ARRAY_SLICE_H_
