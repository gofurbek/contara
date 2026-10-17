# Contara

**Container algebra, simpler C++.**

A small C++20 header-only library for readable operations on standard containers,
easy conversions, lazy range pairing, and explicit multithreading. No third-party dependencies.

```cpp
#include <contara/contara.hpp>

int main() {
    using namespace contara::ops;

    std::set<int> a{1, 2, 3}, b{3, 4, 5};
    auto common = a & b; // {3}
    auto all    = a | b; // {1, 2, 3, 4, 5}
    auto only_a = a - b; // {1, 2}
    auto unique = a ^ b; // {1, 2, 4, 5}: in exactly one input

    std::vector<int> x{3, 1, 3}, y{2, 4};
    auto joined = x + y;                            // {3, 1, 3, 2, 4}
    auto sorted = contara::to_set(joined);           // {1, 2, 3, 4}
    auto hashed = contara::to_unordered_set(joined); // unique; unspecified order
    auto back   = contara::to_vector(sorted);       // {1, 2, 3, 4}
}
```

The results are ordinary standard containers. Binary operations preserve lvalue
inputs. Explicitly moving a left operand allows its storage to be reused; moving
a right sequence allows its elements or list nodes to be transferred.
Operators live in `contara::ops`; import them in the scope where you want them.

## Operations

| Containers | Syntax | Named equivalent | Meaning |
| --- | --- | --- | --- |
| `std::set`, `std::unordered_set` | `a \| b` | `contara::unite(a, b)` | Union |
| Same | `a & b` | `contara::intersect(a, b)` | Intersection |
| Same | `a - b` | `contara::difference(a, b)` | Left difference |
| Same | `a ^ b` | `contara::symmetric_difference(a, b)` | In exactly one input |
| `std::vector`, `std::deque`, `std::list` | `a + b` | `contara::concat(a, b)` | Concatenation; keeps duplicates |
| Same | `a += b` | `contara::append(a, b)` | Append to `a` |

Sets also support `|=`, `&=`, `-=`, and `^=`. These update the left container in
place and retain surviving nodes. Named versions are `unite_into`,
`intersect_into`, `difference_into`, and `symmetric_difference_into`. Sequence `+` means
concatenation. Element-wise numerical addition is outside this API.

Binary operands must have the same complete container type. Convert explicitly
when mixing containers:

```cpp
auto common = contara::to_set(x) & contara::to_set(y);
```

Use parentheses when mixing operators: `a - (b & c)`. Normal C++ operator
precedence applies. C++ cannot introduce a new operator such as `@`.

## Conversions and lazy input

`to_vector`, `to_set`, `to_unordered_set`, `to_deque`, and `to_list` accept C++20
input ranges, including arrays, lists, containers, and range pipelines.
The generic spelling is `contara::to<std::vector>(range)`.

```cpp
auto even = joined | std::views::filter([](int n) { return n % 2 == 0; });
auto output = contara::to_vector(even); // materialize only when needed
```

Views follow standard C++ lifetime and iterator-invalidation rules. Keep their
source alive. Conversion creates a new container with its default allocator and
default comparison/hash policies. A conversion to the same complete container
type copies an lvalue in linear time or moves a mutable rvalue in constant time.
Other conversions copy ordinary range elements; use move iterators to move them
explicitly. Sorted ranges build ordered sets in linear time, including single-pass streams.
Converting to a set removes duplicates; converting from an unordered
set preserves its current iteration order, which is unspecified.

## Zip ranges

`contara::zip(a, b)` pairs corresponding elements and stops at the shorter input.
It is a lazy C++20 view: lvalue inputs are borrowed without visiting or copying elements.

```cpp
std::vector<int> ids{1, 2, 3};
std::vector<std::string> names{"one", "two"};

for (auto&& [id, name] : contara::zip(ids, names)) {
    // (1, "one"), (2, "two"); both bindings refer to source elements
    name += "!";
}

auto rows = contara::to_vector(contara::zip(ids, names)); // copy values into owned rows
auto indexed = contara::zip(std::views::iota(0), names); // generate indices lazily
auto created = contara::zip(std::views::iota(0, 3), std::views::iota(10, 13));
```

The example uses `<string>` in addition to the library header. Pairs expose
`.first` and `.second` and support structured bindings. Reference pairs and
materialized value pairs use `contara::zip_pair`, which supports C++20 range
concepts even when the underlying elements are move-only.

Compose with standard views:

```cpp
auto sums = contara::to_vector(contara::zip(x, y) |
    std::views::transform([](auto pair) { return pair.first + pair.second; }));
```

Lvalue sources are referenced and must stay alive. Viewable rvalue containers
are moved into owning views, with the input container's normal move costs.
Const sources yield const references; a const view
does not itself make mutable referenced containers const. Proxy references
such as `vector<bool>` remain proxies. `to_vector` copies element values, while
explicit move iterators can transfer move-only values.

Traversal preserves the weaker input's iterator capability, including constant-
time indexing when both inputs support random access. `size()` is available when
both inputs are sized and returns their minimum size. Single-pass inputs work;
their iterator initialization/increment can read an element without a yielded
partner when detecting the shorter input's end. Use nested `zip` views to combine
more than two ranges. Ordinary source lifetime and iterator invalidation rules apply.

For two inputs, zip adds `O(1)` work per pair and `O(1)` auxiliary storage.
Complete traversal yields `k = min(n, m)` pairs in `O(k)` time, excluding work
done by underlying views. Zip does not allocate a result buffer.

## Multithreading

```cpp
auto output = contara::parallel_map(input, [](int n) {
    return n * n;
}, {.workers = 4, .min_per_worker = 1024});
```

`parallel_map` accepts a const-readable random-access, sized range, such as a
vector, array, span, or deque. It returns a vector in input order. Result values
may be move-only and need not be default-constructible. Boolean results work too.

- `workers = 0` uses `std::thread::hardware_concurrency()`, falling back to one.
- The worker count is capped at `max(1, size / min_per_worker)`. Small inputs
  run sequentially. `min_per_worker = 0` throws `std::invalid_argument`.
- Each worker gets a copy of the callable. Callbacks must be independent and
  safe to run concurrently. Synchronize any shared state they capture.
- The input and referenced data must stay valid and must not be modified
  concurrently. This library does not make standard containers synchronized.
- Workers join before return. A callback exception cancels remaining work on a
  best-effort basis, then is rethrown to the caller. Already-completed callback
  side effects are not rolled back; the selected exception is unspecified if
  multiple callbacks fail. Thread-creation failures also join started workers.

Threads are created for each parallel call. This is a small utility, with no
persistent thread pool. Set algebra and conversions execute sequentially.

## Performance and policy guarantees

Shorter syntax has the same kinds of allocation and element-copy costs as the
equivalent STL operations. Header-only code and move semantics do not guarantee
zero cost or universal speedups.

| Operation | Time and allocation (compatible policies) |
| --- | --- |
| Ordered union / symmetric difference | `O(n + m)` for lvalue inputs; allocate result nodes |
| Ordered intersection | `O(min(n + m, s log(l + 1)) + k)`; chooses merge or lookup |
| Ordered difference | `O(min(n + m, n log(m + 1)) + k)` |
| Unordered intersection | Expected `O(min(n, m))`; keeps left representatives |
| Other unordered binary operations | Expected `O(n + m)`; collision-heavy hashing can be quadratic |
| Vector append | `O(m)` without growth, `O(n + m)` with growth; geometric capacity preserves amortized linear total work |
| Sequence `a + b` | `O(n + m)` for lvalues; vectors reserve the combined size once |
| List `std::move(a) + std::move(b)` | `O(1)` node splice with equal allocators; otherwise `O(m)` |
| Conversions | Linear for sequences and sorted ranges to sets; general set construction `O(n log n)`; hashed sets expected `O(n)` |
| Parallel map | Trivial non-bool results write directly to output; other results use per-worker buffers |

Here `n` and `m` are input sizes, `s = min(n, m)`, `l = max(n, m)`, and `k` is
the result size. Hash bounds are expected, not worst-case guarantees. Element
construction, comparison, hashing, and callbacks have their own costs.
See [the complete complexity contract](docs/complexity.md) for in-place,
move, worst-case, and auxiliary-space bounds.

Ordered set results retain the left allocator and comparator. Hashed set results
retain the left allocator, hash, equality, and maximum load factor. Values in
both sets are interpreted using the **left operand's equivalence rule**. With
different rules this can make a union's representation depend on operand order.

Comparators and key-equality policies of the same empty type are treated as
compatible. Stateful policies use equality when available; otherwise the right
set is rebuilt under the left policy. Different hash seeds alone do not require
normalization. Normalization adds `O(m log m)` work for ordered sets or expected
`O(m)` for hashed sets, plus temporary storage. A policy's equality must imply
equivalent behavior. Custom policies must
satisfy the standard container requirements.

Binary set operations on lvalues copy selected elements. Compound operations
and operations on a moved left set reuse surviving nodes. They provide the basic
exception guarantee: a failure can leave a partially updated container. List
concatenation can splice a moved right operand when allocators match. Self-append
copies original elements directly without a temporary container; move-only
self-append is rejected. Iterators and references follow standard invalidation rules.

Use the benchmark on your workload before choosing parallel execution. Cheap
callbacks and small inputs can be slower with threads.

## Build and use

Requires a C++20 compiler and standard library with ranges and `std::jthread`,
plus CMake 3.21 or newer if using the CMake build.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Run `build/contara_quickstart` on a single-configuration build, or
`build/Release/contara_quickstart.exe` with Visual Studio.

For your CMake project:

```cmake
add_subdirectory(path/to/contara)
target_link_libraries(my_app PRIVATE contara::contara)
```

Or install it, then use `find_package(contara CONFIG REQUIRED)`:

```sh
cmake --install build --config Release --prefix path/to/install
```

You can also copy the `include/contara` directory into your include path. Compile
with C++20 enabled and enable threading as needed by your toolchain (`-pthread`
on many Unix toolchains, `/EHsc` with MSVC). Individual headers can be included
when you only need one feature.

### Benchmarks

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCONTARA_BUILD_BENCHMARKS=ON
cmake --build build --config Release --parallel
```

Run `contara_benchmarks` from the build output directory. Each case first checks
result equality. Cases cover balanced and uneven set operations, sorted
conversion, repeated vector appends, in-place set updates, and a CPU-heavy
sequential transform versus a four-worker map. Timings include allocation,
destruction, and thread startup; they are illustrative and depend on hardware.

The test suite covers set membership properties, differing policies, allocator
retention, sequence aliasing, range conversions, real worker execution, ordered
parallel results, and exception propagation. CI is configured for GCC, Clang,
MSVC, and Linux address/undefined-behavior sanitizers.

## Scope

This initial version focuses on unique-key sets and sequences. Maps need an
explicit policy for conflicting values; multisets need multiplicity semantics.
Those APIs are left for a future version rather than assigning surprising
operator meanings.

## References

- [C++ working draft: operator overloading](https://eel.is/c++draft/over.oper.general)
- [C++ working draft: set operations](https://eel.is/c++draft/alg.set.operations)
- [C++ working draft: joining threads](https://eel.is/c++draft/thread.jthread)

## License

MIT. See [LICENSE](LICENSE).
