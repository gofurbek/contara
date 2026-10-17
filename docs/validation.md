# Local validation

Date: 2026-10-17. Platform: Windows x64.

Toolchain: MSVC 19.50.35718.0, C++20; CMake 4.4.4 with NMake.

- Direct Debug build: 45,706 checks passed.
- Direct Release build: 45,706 checks passed.
- Both direct test builds used `/W4 /WX /permissive- /EHsc`.
- Quickstart: built and produced the documented container results.
- CMake Release: tests, example, and benchmark built; CTest passed.
- Installation used a custom include directory, `contara-include`.
- A separate consumer found the installed package, built, and passed its test.

One optimized local benchmark run reported these milliseconds per operation
(best of three batches, five operations per batch):

| Operation | Time (ms) |
| --- | ---: |
| Direct STL ordered union | 5.763 |
| Contara ordered union | 5.036 |
| Sequential CPU-heavy transform | 54.486 |
| Contara parallel map, four workers | 17.961 |

These measurements include allocation, destruction, and thread startup. They
are illustrative observations, not performance guarantees. The benchmark
verified equal outputs before timing.

GCC, Clang, and sanitizer jobs are configured in CI but were not run during this
local validation. Cross-platform CI results remain unverified until those jobs
execute.

## Performance revision, 2026-10-17

Using the same Windows/MSVC toolchain:

- Debug and Release each passed 60,969 checks with `/W4 /WX`.
- Regression checks cover geometric append allocation counts, retained set-node
  addresses, smaller-input hash/comparison counts, sorted streams with duplicate
  keys, zero-copy matching-container moves, allocator-sensitive list splicing,
  colliding hashes, and cleanup after a generic parallel callback fails.
- CMake Release build, CTest, installation to a custom include directory, and
  the separately built installed-package consumer passed again.

A paired benchmark compiled the same benchmark source against the original
`b664e1a` headers and the optimized headers, with `/O2 /DNDEBUG /MD`. These are
milliseconds per operation, best of three batches of five operations, except
the uneven intersections which used 100 operations per batch:

| Contara operation | Before | After |
| --- | ---: | ---: |
| Ordered union | 5.939 | 4.811 |
| Ordered intersection, 20,000 versus 1 | 0.157 | <0.001 |
| Unordered intersection, 20,000 versus 1 | 0.106 | <0.001 |
| Sorted vector to ordered set | 3.610 | 2.166 |
| 4,096 repeated single-element vector appends | 1.148 | 0.046 |
| 32 set removals/restorations, including initial copy | 77.593 | 1.615 |
| CPU-heavy map, four workers | 19.721 | 17.700 |

Output equality was checked before each timing. Timing variation across runs
was observed; sub-microsecond results are rounded here, and these measurements
are not universal speedup guarantees. The deterministic operation/allocation
checks protect the algorithmic improvements independently of timing noise.

## Zip extension, 2026-10-17

- MSVC Debug and Release each passed 61,003 checks in C++20 mode with `/W4 /WX`.
- Linux GCC 11.5 passed the same 61,003 checks with
  `-std=c++20 -Wall -Wextra -Wpedantic -Werror -O2 -pthread`.
- Coverage includes shortest-input termination, empty and infinite inputs,
  constant-time random access, const references, forward/bidirectional/single-pass
  ranges, proxy references, move-only values, owning temporaries, nested pairs,
  value materialization, standard algorithms, constexpr evaluation, and
  allocation-free iteration over borrowed containers.
- A small ownership fallback supports C++20 libraries predating `owning_view`.
- CMake Release build, installation, and the independent consumer using zip passed.

One local benchmark over 100,000 pairwise sums took 0.497560 ms for an indexed
loop and 0.512980 ms for lazy zip plus materialization. Both produced identical
outputs and allocated the output vector. These timings are illustrative;
the zip traversal remains linear with constant auxiliary storage.

An address/undefined-behavior sanitizer build was attempted but could not link:
the installed GCC toolchain references a missing `/usr/lib64/libasan.so.6.0.0`.
No sanitizer success is claimed for this revision. Clang and the configured
GitHub CI jobs remain unverified locally.
