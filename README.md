# HeapForge — Linux Userspace Memory Allocator

HeapForge is a production-inspired, educational 64-bit Linux userspace memory allocator written in C11. Built from the ground up without relying on the system C runtime allocator (`glibc malloc`), HeapForge demonstrates fundamental Operating Systems concepts: virtual memory management, system calls (`sbrk`, `mmap`, `munmap`), segregated free lists, boundary tag coalescing, memory alignment, heap integrity verification, and multithreading safety.

---

## Architecture Overview

```
                      +-----------------------------+
                      |   Application / User Code   |
                      +-----------------------------+
                                     |
               +---------------------+---------------------+
               |                     |                     |
        malloc() / free()     calloc() / realloc()    heap_check()
               |                     |                     |
               +---------------------+---------------------+
                                     |
                       +---------------------------+
                       |    HeapForge Front-End    |
                       | Alignment, Overflow Guard |
                       +---------------------------+
                                     |
                +--------------------+--------------------+
                | (< MMAP_THRESHOLD) | (>= MMAP_THRESHOLD)|
                v                                         v
   +-------------------------+               +-------------------------+
   |   Segregated Free List  |               |  mmap Anonymous Pages   |
   |   (Powers of 2 classes) |               |  (Dedicated Allocation) |
   +-------------------------+               +-------------------------+
                |                                         |
   +-------------------------+               +-------------------------+
   |  sbrk() Program Break   |               |         munmap()        |
   |  Heap Expansion Engine  |               +-------------------------+
   +-------------------------+
```

---

## Milestone Roadmap

- [x] **Milestone 1 (Week 1):** Linux toolchain validation, repository skeleton, 16-byte alignment primitives, minimal `sbrk()` bump allocator, overflow protection, test suite.
- [ ] **Milestone 2 (Week 2):** Block headers and footers, boundary tags, minimum block size enforcement, free-list foundation.
- [ ] **Milestone 3 (Week 3):** Segregated free lists, size classes (`size_to_class`), and `heap_check()` consistency engine.
- [ ] **Milestone 4 (Week 4):** Block splitting (`MIN_BLOCK_SIZE` remainder rule) and stress testing.
- [ ] **Milestone 5 (Week 5):** `calloc()` zeroing and `realloc()` in-place expansion/shrinking logic.
- [ ] **Milestone 6 (Week 6):** Allocator statistics engine, external fragmentation metrics, heap dump.
- [ ] **Milestone 7 (Week 7):** Immediate O(1) coalescing of adjacent blocks.
- [ ] **Milestone 8 (Week 8):** Large allocation handling via `mmap()` and `munmap()`.
- [ ] **Milestone 9 (Week 9):** Allocation trace and replay system (`HEAPFORGE_TRACE`), Python visualizer.
- [ ] **Milestone 10 (Week 10):** Thread safety (Global mutex lock + multithreading stress test).
- [ ] **Milestone 11 (Week 11):** Per-thread arenas, thread-local storage, cross-arena free handling.
- [ ] **Milestone 12 (Week 12):** `LD_PRELOAD` shared library (`libheapforge.so`), benchmark suite vs system `malloc`, viva documentation.

---

## Building and Testing

### Prerequisites
- Linux OS (Ubuntu 22.04+ or WSL2)
- GCC with C11 support
- GNU Make 4.0+

### Build Targets
```bash
# Build static library (lib/libheapforge.a)
make

# Build and run the test suite
make test

# Build with debug symbols
make debug

# Run test suite under AddressSanitizer and UndefinedBehaviorSanitizer
make asan

# Clean build artifacts
make clean
```

---

## License
MIT License. See [LICENSE](LICENSE) for details.
