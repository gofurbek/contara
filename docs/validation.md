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
