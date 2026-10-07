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
  Callers cannot use pointer equality of non-null addresses to distinguish two distinct 0-byte allocations.

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

## Decision 4: Symmetrical 16-Byte Boundary Tags with 64-Bit Canaries

- **Problem:**
  Coalescing physically adjacent free blocks requires inspecting the preceding block in $O(1)$ time. In addition, corruptions like buffer overflows and double frees must be detected immediately before corrupting allocator pointers.
- **Decision:**
  Both `block_header_t` and `block_footer_t` are sized at exactly 16 bytes:
  - Header: `size_and_flags` (8 bytes) + `magic` (0xDEADBEEFCAFEFEED) (8 bytes).
  - Footer: `size_and_flags` (8 bytes) + `magic` (0xFEEDCAFEBEEFDEAD) (8 bytes).
- **Why this decision:**
  1. Because `sizeof(block_header_t) == 16`, any payload starting immediately after the header begins at an address that is guaranteed 16-byte aligned.
  2. The matching footer allows $O(1)$ traversal to the preceding block header: `prev_hdr = (char *)curr_hdr - prev_ftr->size`.
  3. Canaries immediately expose buffer overruns during `free()` and `heap_check()`.
- **Alternatives considered:**
  - *8-byte header without canaries:* Payload start would be misaligned by 8 bytes unless padded, increasing code complexity.
  - *Footer only on free blocks:* Common in glibc, but requires checking whether previous block is allocated via a flag in the current header. Symmetrical footers provide simpler verification for course viva explanation.
- **Trade-offs:**
  32 bytes of total metadata overhead per block.

---

## Decision 5: Segregated Free Lists (13 Power-of-Two Classes)

- **Problem:**
  A single global free list requires an $O(N)$ linear search for each allocation, resulting in catastrophic slowdowns as heap usage scales.
- **Decision:**
  Implement 13 segregated free list classes partitioned approximately by powers of two:
  - Class 0: 48–64
  - Class 1: 65–128
  - Class 2: 129–256
  - ...
  - Class 11: 65537–131072 (128 KiB mmap threshold)
  - Class 12: > 131072
- **Why this decision:**
  Finding a suitable block is $O(1)$ on average. Allocations search their exact size class first, and fall back directly to larger classes where any block is guaranteed to satisfy the request.
- **Alternatives considered:**
  - *Implicit free list:* Linear search through allocated and free blocks. Extremely slow.
  - *Binary Buddy allocator:* Fast, but suffers severe internal fragmentation (up to 50%).
- **Trade-offs:**
  Array of 13 list head pointers and bookkeeping counters.

---

## Decision 6: Block Splitting and `MIN_BLOCK_SIZE = 48`

- **Problem:**
  When a request is smaller than the available free block, assigning the entire block wastes memory (internal fragmentation). However, creating an arbitrarily small remainder would fail to hold block metadata.
- **Decision:**
  Define `MIN_BLOCK_SIZE = 48` bytes (16-byte header + 16-byte free node pointers + 16-byte footer). Split only if `remainder >= 48`.
- **Why this decision:**
  Guarantees that every free block can safely house the intrusive `prev_free` and `next_free` pointers.
- **Trade-offs:**
  Requests leaving $< 48$ bytes cannot be split and suffer minor internal fragmentation.

---

## Decision 7: `realloc()` In-Place Optimization

- **Problem:**
  Naively implementing `realloc` as `malloc() + memcpy() + free()` causes heavy memory churn and copying overhead.
- **Decision:**
  Implement 5 distinct cases:
  - **Case A (`ptr == NULL`):** Equivalent to `malloc(size)`.
  - **Case B (`size == 0`):** Equivalent to `free(ptr)`.
  - **Case C (Shrinking):** If excess $\ge 48$ bytes, split the block in place and return the remainder to the free list.
  - **Case D (Growing in place):** If the immediately following physical block is free and `current_size + next_size >= needed_size`, merge into the adjacent free block in place without copying!
  - **Case E (Movement):** Fall back to `malloc() + memcpy() + free()` only when in-place growth is impossible.
- **Why this decision:**
  Drastically accelerates common growing patterns (e.g. dynamic array / vector resizing).
- **Trade-offs:**
  Slightly more conditional logic in `realloc()`.

---

## Decision 8: External Fragmentation Formula and Tracking Architecture

- **Problem:**
  Evaluating an allocator requires objectively quantifying how effectively it prevents memory fragmentation. Without a mathematical definition, claims about fragmentation are untestable.
- **Decision:**
  HeapForge defines external fragmentation using the classical formula:
  $$\text{External Fragmentation} = 1.0 - \left( \frac{\text{Largest Available Free Block}}{\text{Total Free Bytes}} \right)$$
  when $\text{Total Free Bytes} > 0$ (otherwise $0.0$).
- **Why this decision:**
  1. If all free memory is coalesced into one single large block, $\text{Largest} = \text{Total}$, yielding $0.0$ ($0\%$ fragmentation).
  2. If free memory is shattered across many small disjoint fragments, $\text{Largest} \ll \text{Total}$, yielding a value approaching $1.0$ ($100\%$ fragmentation).
  3. Bounded strictly in $[0.0, 1.0]$, intuitive to interpret and visualize.
- **Alternatives considered:**
  - *Ratio of free bytes to total heap:* Measures unallocated capacity, not fragmentation. A heap can be 90% free and 0% fragmented if that free memory is a single large contiguous block.
- **Trade-offs:**
  Requires a fast pass over free list heads to extract the maximum block size during statistics queries.
