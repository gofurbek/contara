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

## Zip shorthand and Cartesian product, 2026-10-18

- `a % b` is an opt-in alias for `zip(a, b)`; `a * b` and
  `cartesian_product(a, b)` produce a lazy Cartesian product.
- MSVC Debug and Release each passed 61,648 checks with `/W4 /WX` in C++20 mode.
- Linux GCC 11.5 passed the same 61,648 checks with
  `-std=c++20 -Wall -Wextra -Wpedantic -Werror -O2 -pthread`.
- Checks cover nested-loop ordering, small-grid membership, empty/infinite
  inputs, input/forward/bidirectional/random-access ranges, negative offsets,
  const/proxy references, owned temporaries, move-only elements, raw-array
  combinations, standard pipelines, and opt-in operator lookup.
- Borrowed-container traversal allocates no result buffer. Constant-time
  indexing was exercised on a view containing 10^18 pairs without traversal.
  Size and signed-distance overflow, including the minimum signed distance,
  are covered independently of element allocation.
- CMake Release built the tests, example, and benchmark; CTest passed.
  Installation with a custom include directory and the independent consumer
  using both operators passed. A standalone Cartesian header check passed
  on MSVC and GCC.

One optimized local benchmark materialized 120,000 pairwise sums in nested-loop
order: 0.151300 ms for explicit loops versus 0.154140 ms for the lazy Cartesian
product. Output equality was checked before timing. Both allocate the final
vector; these observations are illustrative, not a universal speed guarantee.
Sanitizer and Clang execution remain unverified for this revision.

## Correctness and performance review, 2026-10-18

Reviewed all public headers, set-policy/allocator behavior, sequence aliasing,
range lifetime and arithmetic, parallel exception cleanup, CMake packaging,
and CI configuration. The input was the local tree at `85b120d` with the later
zip shorthand and Cartesian product additions. Existing source edits were
preserved; the isolated review used byte-for-byte copies of that tree.

Confirmed with before/after reproducers and fixed:

- Value-producing ranges failed to compile with rvalue-reference callbacks in
  `parallel_map`; sequential invocation now preserves the reference category.
- Move-only set keys could not be intersected/removed in place because an unused
  normalization path instantiated copies. Compatible policies now work; an
  incompatible policy throws before mutation when normalization cannot copy.
- Named `concat` lacked moved-left overloads, causing unnecessary copies and
  rejecting move-only elements. It now shares the operator's storage reuse.
- Very large random-access zip ends silently narrowed their length into a
  signed offset, producing an end before begin. End creation now checks the
  offset; subtraction also avoids negating the minimum signed offset.
- Reserving conversion and parallel-map paths could narrow an oversized range
  size before checking destination limits. They now reject it before visiting
  elements or starting workers. GCC's extended-integer range case verifies this.

Self/empty-right hashed copy paths now compact excess reservations while
preserving the left policies/load factor. A GCC two-element self-union reduced
the result from 107,897 buckets to 2. Moved concatenation retains the original
vector buffer or list nodes; allocation checks account for the standard
container move's own debug metadata overhead.

- MSVC C++20 Debug and Release each passed 61,667 checks with `/W4 /WX`.
- GCC 11.5 passed 61,669 checks with strict warnings, both optimized and with
  `_GLIBCXX_DEBUG` / `_GLIBCXX_ASSERTIONS`. Its two extra checks cover 128-bit sizes.
- Standalone headers, quickstart, CMake Release/CTest, installation to a custom
  include directory, and the independently built installed-package consumer passed.
- A throwing callable-copy regression verifies cleanup after launching a worker;
  actual operating-system thread-resource exhaustion was not injected.

Benchmarks now consume a checksum of every output element. This adds common
linear checksum work to both sides; earlier measurements consumed only the
size/first element and should be treated as illustrative historical observations.

The existing [GitHub CI run](https://github.com/gofurbek/contara/actions/runs/39066052781)
passed GCC, Clang, MSVC, and sanitizers for the published `85b120d` baseline.
It does not cover the later local changes. The current revision has no local
ASan/UBSan or Clang result: sanitizer runtime files were missing, and no Clang
run was performed. No new dependencies were installed.
