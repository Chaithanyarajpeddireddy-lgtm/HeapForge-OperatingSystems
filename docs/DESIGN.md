# HeapForge Design Decisions Document

This document records architectural and implementation design decisions made throughout the evolution of HeapForge. Every decision details the problem context, the chosen solution, the rationale, alternatives considered, and trade-offs.

---

## Decision 1: 16-Byte Alignment Guarantee

- **Problem:**
  On modern 64-bit architectures (x86_64, AArch64), different C data types require different memory alignment constraints (e.g. 8 bytes for pointers/integers, 16 bytes for `long double` or SSE vector types `__m128`). Returning unaligned pointers from `malloc()` causes undefined behavior, severe CPU penalties, or hardware bus error faults (`SIGBUS`).
- **Decision:**
  Guarantee strict **16-byte alignment** on every pointer returned by `heapforge_malloc()`. All block sizes are rounded up to the nearest multiple of 16.
- **Why this decision:**
  `alignof(max_align_t)` on x86_64 Linux is 16 bytes. By enforcing 16-byte alignment:
  1. Any primitive C type can be safely stored at any returned address.
  2. The lowest 4 bits (`log2(16) = 4`) of any block address/size are guaranteed to be zero (`0b0000`). This allows future milestones to pack status flags (Allocated bit, mmap bit) directly into size fields without wasting additional metadata bytes.
- **Alternatives considered:**
  - *8-byte alignment:* Saves internal padding on small odd-sized allocations, but fails `max_align_t` requirements for 128-bit types.
  - *32-byte or 64-byte alignment:* Optimal for AVX-256 / cache-line alignment, but creates excessive internal fragmentation for small allocations (e.g. allocating 4 bytes would consume 64 bytes).
- **Trade-offs:**
  Small allocations (e.g., 1–8 bytes) experience internal padding up to 16 bytes, but complete safety and hardware compliance are guaranteed.

---

## Decision 2: Zero-Size Allocation Policy (`malloc(0)`)

- **Problem:**
  The C standard (C11 §7.22.3) leaves the behavior of `malloc(0)` implementation-defined: the allocator may return `NULL`, or return a non-null pointer that cannot be dereferenced but can be passed to `free()`.
- **Decision:**
  HeapForge explicitly chooses: `heapforge_malloc(0)` returns `NULL`.
- **Why this decision:**
  1. Prevents wasting heap metadata and fragmenting free lists for requests with zero storage utility.
  2. Provides clear, deterministic semantics easily validated during automated tests and vivas.
  3. Consistent with the principle: zero bytes requested = zero resources reserved.
- **Alternatives considered:**
  - *Return unique non-null pointer:* Requires creating a minimum dummy block or shared sentinel, adding complexity and potential memory leaks if callers forget to free.
- **Trade-offs:**
  Callers cannot use pointer equality of non-null addresses to distinguish two distinct 0-byte allocations (which standard POSIX allows).

---

## Decision 3: Integer Overflow and Upper Bound Guarding

- **Problem:**
  Computing `size + alignment_padding` on untrusted input size can trigger 64-bit integer wraparound (e.g., `SIZE_MAX`). If an integer overflows, a small size could be requested from `sbrk()`, returning a tiny buffer that leads to catastrophic buffer overflows when the caller writes into it.
- **Decision:**
  Before alignment calculation:
  ```c
  if (size > HEAPFORGE_MAX_ALLOC_SIZE) {
      errno = ENOMEM;
      return NULL;
  }
  ```
  where `HEAPFORGE_MAX_ALLOC_SIZE = (SIZE_MAX - HEAPFORGE_ALIGNMENT)`. Furthermore, ensure the aligned size fits inside signed `intptr_t` accepted by `sbrk()`.
- **Why this decision:**
  Strict defense-in-depth against integer overflow vulnerabilities (CWE-190).
- **Alternatives considered:**
  - *Let sbrk() fail naturally:* Fails if the wrapped size becomes a small positive number (e.g. `SIZE_MAX + 1 == 0` or small), resulting in severe security holes.
- **Trade-offs:**
  Tiny branch overhead on allocation entry.

---

## Decision 4: Milestone 1 Bump Allocator Baseline

- **Problem:**
  Building all features (free lists, coalescing, splitting, mmap, threads) in one shot leads to debugging nightmares and violated engineering rules.
- **Decision:**
  Milestone 1 implements an unadulterated monotonic bump allocator directly backed by `sbrk()`.
- **Why this decision:**
  Establishes the minimal viable interaction between userspace memory requests and the Linux kernel program break. It provides a clean, passing foundation against which alignment and basic allocation can be rigorously benchmarked before introducing headers in Milestone 2.
- **Alternatives considered:**
  - *Skip directly to linked free list:* Violates rule 1: "DO NOT generate a giant implementation immediately. Build the system incrementally and verify each milestone before moving to the next one."
- **Trade-offs:**
  Freed memory is not yet reused in Milestone 1 (addressed in Milestone 2).
