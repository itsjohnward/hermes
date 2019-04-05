/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the LICENSE
 * file in the root directory of this source tree.
 */
#ifndef HERMES_SUPPORT_OSCOMPAT_H
#define HERMES_SUPPORT_OSCOMPAT_H

#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Compiler.h"

#ifdef _WINDOWS
#include <io.h>
#else
#include <unistd.h>
#endif

#include <chrono>
#include <cmath>
#include <cstddef>
#include <sstream>
#include <string>

// This file defines cross-os APIs for functionality provided by our target
// operating systems.

#if __has_feature(address_sanitizer) || defined(__SANITIZE_ADDRESS__)
#define ASAN_ENABLED
#endif

#ifdef ASAN_ENABLED
#include <sanitizer/asan_interface.h>
#endif

namespace hermes {
namespace oscompat {

#ifndef NDEBUG
void set_test_page_size(size_t pageSz);
void reset_test_page_size();
#endif // !NDEBUG

// Returns the current page size.
size_t page_size();

#ifndef NDEBUG
/// For testing purposes, we can limit the maximum net change in allocated
/// virtual address space from this point forward.  That is, if we track the sum
/// of future allocations, minus future frees, an allocation that would make
/// that sum exceed \p totSz fails.
void set_test_vm_allocate_limit(size_t totSz);

/// Return the test VM allocation lmit to "unlimited."
void unset_test_vm_allocate_limit();
#endif // !NDEBUG

// Allocates a virtual memory region of the given size (required to be
// a multiple of page_size()), and returns a pointer to the start.
// Returns nullptr if the allocation is unsuccessful.  The pages
// will be zero-filled on demand.
void *vm_allocate(size_t sz);

// Allocates a virtual memory region of the given size and alignment (both
// must be multiples of page_size()), and returns a pointer to the start.
// Returns nullptr if the allocation is unsuccessful.  The pages
// will be zero-filled on demand.
void *vm_allocate_aligned(size_t sz, size_t alignment);

/// Free a virtual memory region allocated by \p vm_allocate.
/// \p p must point to the base address that was returned by \p vm_allocate.
/// Memory region returned by \p vm_allocate_aligned must be freed by
/// invoking \p vm_free_aligned, instead of this function.
/// \p size must match the value passed to the respective allocate functions.
/// In other words, partial free is not allowed.
void vm_free(void *p, size_t sz);

/// Similar to \p vm_free, but for memory regions returned by
/// \p vm_allocate_aligned.
void vm_free_aligned(void *p, size_t sz);

/// Mark the \p sz byte region of memory starting at \p p as not currently in
/// use, so that the OS may free it. \p p must be page-aligned.
void vm_unused(void *p, size_t sz);

/// Mark the \p sz byte region of memory starting at \p p as soon being needed,
/// so that the OS may prefetch it. \p p must be page-aligned.
void vm_prefetch(void *p, size_t sz);

/// Assign a \p name to the \p sz byte region of virtual memory starting at
/// pointer \p p.  The name is assigned only on supported platforms (currently
/// only Android).  This name appears when the OS is queried about the mapping
/// for a process (e.g. by /proc/<pid>/maps).
void vm_name(void *p, size_t sz, const char *name);

enum class ProtectMode { ReadWrite };

/// Set the \p sz byte region of memory starting at \p p to the specified
/// \p mode. \p p must be page-aligned. \return true if successful,
/// false on error.
bool vm_protect(void *p, size_t sz, ProtectMode mode);

/// Issue an madvise() call.
/// \return true on success, false on error.
enum class MAdvice { Random, Sequential };
bool vm_madvise(void *p, size_t sz, MAdvice advice);

/// Return the number of pages in the given region that are currently in RAM.
/// If \p runs is provided, then populate it with the lengths of runs of
/// consecutive pages with the same resident/non-resident status, alternating
/// between the two statuses, and with the first element always denoting a
/// number of resident pages (0 if the first page is not resident).
///
/// Return -1 on failure (including not supported).
int pages_in_ram(
    const void *p,
    size_t sz,
    llvm::SmallVectorImpl<int> *runs = nullptr);

/// Resident set size (RSS), in bytes: the amount of RAM used by the process.
/// It excludes virtual memory that has been paged out or was never loaded.
uint64_t peak_rss();

/// Get the number of \p voluntary and \p involuntary context switches the
/// process has made so far, or return false if unsupported.
bool num_context_switches(long &voluntary, long &involuntary);

/// \return OS thread id of current thread.
uint64_t thread_id();

/// \return the duration in microseconds the CPU has spent executing this thread
/// upon success, or `std::chrono::microseconds::max()` on failure.
std::chrono::microseconds thread_cpu_time();

/// Get by reference the minor and major page fault counts for the current
/// thread. \return true if successful, false on error.
bool thread_page_fault_count(int64_t *outMinorFaults, int64_t *outMajorFaults);

/// \return name of current thread.
std::string thread_name();

/// Poisons/unpoisons the memory region when run with ASAN on. This is a no-op
/// when ASAN is not enabled.
///
/// A poisoned region cannot be read from or written to, else it'll generate
/// an abort with the stack trace of the illegal read/write.
/// This should not be used as a replacement for ASAN's normal operations with
/// malloc/free, and should only be used to poison memory ranges that are not
/// managed by normal C memory management (for example, in a GC).
inline void asan_poison_if_enabled(void *start, void *end);
inline void asan_unpoison_if_enabled(void *start, void *end);

/// Converts a value to its string representation.  Only works for
/// numeric values, e.g. 0 becomes "0", not '\0'.
///
/// NOTE: This is here because Android does not have to_string defined in its
/// standard library.
template <typename T>
inline ::std::string to_string(T value);

/// The following functions are defined in Android's standard library, but not
/// in the \c std namespace.
inline double log2(double n);
inline double trunc(double n);
inline double copysign(double x, double y);
inline double nextafter(double x, double y);

inline void asan_poison_if_enabled(void *start, void *end) {
#ifdef ASAN_ENABLED
  ASAN_POISON_MEMORY_REGION(
      start,
      reinterpret_cast<uintptr_t>(end) - reinterpret_cast<uintptr_t>(start));
#endif
}

inline void asan_unpoison_if_enabled(void *start, void *end) {
#ifdef ASAN_ENABLED
  ASAN_UNPOISON_MEMORY_REGION(
      start,
      reinterpret_cast<uintptr_t>(end) - reinterpret_cast<uintptr_t>(start));
#endif
}

template <typename T>
inline ::std::string to_string(T value) {
  ::std::ostringstream os;
  os << +value;
  return os.str();
}

inline double log2(double n) {
  return ::log(n) / ::log(2.0);
}

inline double trunc(double n) {
  return ::trunc(n);
}

inline double copysign(double x, double y) {
  return ::copysign(x, y);
}

inline double nextafter(double x, double y) {
  return ::nextafter(x, y);
}

#ifdef _WINDOWS

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

/// \return whether fd refers to a terminal / character device
inline int isatty(int fd) {
  return ::_isatty(fd);
}

#else

/// \return whether fd refers to a terminal / character device
inline int isatty(int fd) {
  return ::isatty(fd);
}

#endif

/// Set the env var \p name to \p value.
/// \p value must not be an empty string.
/// Setting an env var to empty is not supported because doing it
/// cross-platform is hard.
/// \return true if successful, false on error.
bool set_env(const char *name, const char *value);

/// Unset the env var \p name.
/// \return true if successful, false on error.
bool unset_env(const char *name);

/// LLVM sets up an alternate signal stack.  By default, the stack is
/// never deleted, and is reported as a leak.  The destructor of this
/// class deletes the alt signal stack, if one was installed.
class SigAltStackDeleter {
 public:
  SigAltStackDeleter();
  ~SigAltStackDeleter();
#if !defined(__APPLE__) && !defined(_WINDOWS)
 private:
  void *origStack_{nullptr};
#endif
};

} // namespace oscompat
} // namespace hermes

#endif // HERMES_VM_OSCOMPAT_H
