# HeapForge Architecture Document

## 1. High-Level Vision
HeapForge is a custom Linux userspace dynamic memory allocator. Unlike general-purpose standard library allocators that carry decades of legacy baggage and tuning quirks, HeapForge is engineered to balance real-world robustness with pedagogical clarity for Operating Systems course evaluation and viva defense.

---

## 2. Layers of the Allocator

```
+----------------------------------------------------------------------+
| Layer 4: Public API & Preload Interface                              |
| - malloc(), free(), calloc(), realloc()                              |
| - LD_PRELOAD wrapper interposition (libheapforge.so)                 |
+----------------------------------------------------------------------+
                                  |
+----------------------------------------------------------------------+
| Layer 3: Allocation Management & Sizing Policy                       |
| - Threshold routing: Small/Medium vs. Large allocations              |
| - Size classes (segregated free lists, powers-of-two classes)        |
| - Block splitting & Immediate coalescing                             |
+----------------------------------------------------------------------+
                                  |
+----------------------------------------------------------------------+
| Layer 2: Block & Metadata Abstraction                                |
| - 16-byte alignment enforcement                                      |
| - Boundary tags (Header + Footer size & allocation flags)            |
| - Intrusive doubly linked list pointers (in free blocks)             |
+----------------------------------------------------------------------+
                                  |
+----------------------------------------------------------------------+
| Layer 1: OS Kernel Interface                                         |
| - sbrk() for heap extension (contiguous break-managed pool)          |
| - mmap(MAP_ANONYMOUS | MAP_PRIVATE) for large chunks                |
| - munmap() for returning large chunks directly to page tables        |
+----------------------------------------------------------------------+
```

---

## 3. Subsystem Breakdown

### 3.1 Kernel Memory Interface (`src/allocator.c`, `src/mmap_alloc.c`)
- **Heap Growth (`sbrk`)**: Manages the continuous program break. HeapForge aligns the initial break address to 16 bytes upon first invocation to ensure all subsequent increments remain strictly aligned.
- **Large Allocations (`mmap`/`munmap`)**: Allocations meeting or exceeding `MMAP_THRESHOLD` (e.g. 128 KiB) bypass the sbrk heap and receive dedicated page-aligned mappings directly from the kernel virtual memory subsystem, avoiding long-term fragmentation of the main heap.

### 3.2 Block & Boundary Tag Engine (`src/block.c`, `src/coalesce.c`)
- Every block is prefixed by a **Header** storing block size and allocation status flags.
- Free blocks contain a matching **Footer** at the block tail, forming Knuth's classic **boundary tags**. This guarantees that any block can inspect its physical preceding block in $O(1)$ time, making immediate coalescing $O(1)$.
- Intrusive `prev_free` and `next_free` pointers occupy the user payload section *only while the block is free*, imposing zero additional metadata overhead during block utilization.

### 3.3 Free List Subsystem (`src/freelist.c`)
- Implements segregated doubly-linked free lists partitioned into size classes (e.g., 16–32, 33–64, ..., 4096+).
- Insertion and removal are $O(1)$.
- Best-fit or first-fit within the exact size class eliminates the catastrophic $O(N)$ linear scans of single implicit free lists.

### 3.4 Verification & Diagnostics (`src/heap_check.c`, `src/stats.c`)
- An invariant checker validates block alignments, header/footer symmetry, free-list membership, acyclicity, and prevents double-free and heap corruption before silent catastrophic failures occur.
- A statistics tracking module computes active bytes, high-water mark, OS overhead, and fragmentation ratio.

### 3.5 Concurrency Model (`src/arena.c`)
- **Stage 1**: Coarse-grained global `pthread_mutex_t` protecting all allocator state.
- **Stage 2**: Multi-arena design assigning threads to dedicated heaps with thread-local caching, mitigating lock contention.
