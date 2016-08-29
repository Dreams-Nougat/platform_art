/*
 * Copyright (C) 2010 The Android Open Source Project
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

#ifndef ART_RUNTIME_BASE_MACROS_H_
#define ART_RUNTIME_BASE_MACROS_H_

#include <stddef.h>  // for size_t
#include <unistd.h>  // for TEMP_FAILURE_RETRY

#include "android-base/macros.h"

#define OVERRIDE override
#define FINAL final

// Declare a friend relationship in a class with a test. Used rather that FRIEND_TEST to avoid
// globally importing gtest/gtest.h into the main ART header files.
#define ART_FRIEND_TEST(test_set_name, individual_test)\
friend class test_set_name##_##individual_test##_Test

// Declare a friend relationship in a class with a typed test.
#define ART_FRIEND_TYPED_TEST(test_set_name, individual_test)\
template<typename T> ART_FRIEND_TEST(test_set_name, individual_test)

// A macro to disallow new and delete operators for a class. It goes in the private: declarations.
// NOTE: Providing placement new (and matching delete) for constructing container elements.
#define DISALLOW_ALLOCATION() \
  public: \
    NO_RETURN ALWAYS_INLINE void operator delete(void*, size_t) { UNREACHABLE(); } \
    ALWAYS_INLINE void* operator new(size_t, void* ptr) noexcept { return ptr; } \
    ALWAYS_INLINE void operator delete(void*, void*) noexcept { } \
  private: \
    void* operator new(size_t) = delete  // NOLINT

#define SIZEOF_MEMBER(t, f) sizeof((reinterpret_cast<t*>(4096))->f)  // NOLINT

#define OFFSETOF_MEMBER(t, f) \
  (reinterpret_cast<uintptr_t>(&reinterpret_cast<t*>(16)->f) - static_cast<uintptr_t>(16u))  // NOLINT

#define OFFSETOF_MEMBERPTR(t, f) \
  (reinterpret_cast<uintptr_t>(&(reinterpret_cast<t*>(16)->*f)) - static_cast<uintptr_t>(16))  // NOLINT

#define PACKED(x) __attribute__ ((__aligned__(x), __packed__))

// Stringify the argument.
#define QUOTE(x) #x
#define STRINGIFY(x) QUOTE(x)

#ifndef NDEBUG
#define ALWAYS_INLINE
#else
#define ALWAYS_INLINE  __attribute__ ((always_inline))
#endif

// clang doesn't like attributes on lambda functions. It would be nice to say:
//   #define ALWAYS_INLINE_LAMBDA ALWAYS_INLINE
#define ALWAYS_INLINE_LAMBDA

#define NO_INLINE __attribute__ ((noinline))

#if defined (__APPLE__)
#define HOT_ATTR
#define COLD_ATTR
#else
#define HOT_ATTR __attribute__ ((hot))
#define COLD_ATTR __attribute__ ((cold))
#endif

#define PURE __attribute__ ((__pure__))

// Define that a position within code is unreachable, for example:
//   int foo () { LOG(FATAL) << "Don't call me"; UNREACHABLE(); }
// without the UNREACHABLE a return statement would be necessary.
#define UNREACHABLE  __builtin_unreachable

// Add the C++11 noreturn attribute.
#define NO_RETURN [[ noreturn ]]  // NOLINT[whitespace/braces] [5]

// Annotalysis thread-safety analysis support.

#define ACQUIRED_AFTER(...) __attribute__((acquired_after(__VA_ARGS__)))
#define ACQUIRED_BEFORE(...) __attribute__((acquired_before(__VA_ARGS__)))
#define GUARDED_BY(x) __attribute__((guarded_by(x)))
#define GUARDED_VAR __attribute__((guarded))
#define LOCK_RETURNED(x) __attribute__((lock_returned(x)))
#define NO_THREAD_SAFETY_ANALYSIS __attribute__((no_thread_safety_analysis))
#define PT_GUARDED_BY(x)
// THREAD_ANNOTATION_ATTRIBUTE__(point_to_guarded_by(x))
#define PT_GUARDED_VAR __attribute__((point_to_guarded))
#define SCOPED_LOCKABLE __attribute__((scoped_lockable))

#define EXCLUSIVE_LOCK_FUNCTION(...) __attribute__((exclusive_lock_function(__VA_ARGS__)))
#define EXCLUSIVE_TRYLOCK_FUNCTION(...) __attribute__((exclusive_trylock_function(__VA_ARGS__)))
#define SHARED_LOCK_FUNCTION(...) __attribute__((shared_lock_function(__VA_ARGS__)))
#define SHARED_TRYLOCK_FUNCTION(...) __attribute__((shared_trylock_function(__VA_ARGS__)))
#define UNLOCK_FUNCTION(...) __attribute__((unlock_function(__VA_ARGS__)))
#define REQUIRES(...) __attribute__((requires_capability(__VA_ARGS__)))
#define SHARED_REQUIRES(...) __attribute__((requires_shared_capability(__VA_ARGS__)))
#define CAPABILITY(...) __attribute__((capability(__VA_ARGS__)))
#define SHARED_CAPABILITY(...) __attribute__((shared_capability(__VA_ARGS__)))
#define ASSERT_CAPABILITY(...) __attribute__((assert_capability(__VA_ARGS__)))
#define ASSERT_SHARED_CAPABILITY(...) __attribute__((assert_shared_capability(__VA_ARGS__)))
#define RETURN_CAPABILITY(...) __attribute__((lock_returned(__VA_ARGS__)))
#define TRY_ACQUIRE(...) __attribute__((try_acquire_capability(__VA_ARGS__)))
#define TRY_ACQUIRE_SHARED(...) __attribute__((try_acquire_shared_capability(__VA_ARGS__)))
#define ACQUIRE(...) __attribute__((acquire_capability(__VA_ARGS__)))
#define ACQUIRE_SHARED(...) __attribute__((acquire_shared_capability(__VA_ARGS__)))
#define RELEASE(...) __attribute__((release_capability(__VA_ARGS__)))
#define RELEASE_SHARED(...) __attribute__((release_shared_capability(__VA_ARGS__)))
#define SCOPED_CAPABILITY __attribute__((scoped_lockable))

#define LOCKABLE CAPABILITY("mutex")
#define SHARED_LOCKABLE SHARED_CAPABILITY("mutex")

#endif  // ART_RUNTIME_BASE_MACROS_H_
