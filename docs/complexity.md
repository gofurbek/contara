# Complexity contract

The library chooses algorithms for standard containers through their public
APIs. There is no universally fastest algorithm for every size, allocator,
comparison cost, or data distribution. These bounds describe the implementation;
they do not claim optimality among all possible data structures.

Let `n = a.size()`, `m = b.size()`, `s = min(n, m)`, `l = max(n, m)`, and `k` be
the number of result elements. Bounds assume constant-cost element operations
and compatible comparator/key-equality policies. `O(1)` fixed overhead is
implicit, including for empty inputs. Ordered insertion/erasure bounds below
are amortized where the standard specifies amortized bounds.

## Binary set operations on lvalues

| Container / operation | Time | Additional storage, including result |
| --- | --- | --- |
| Ordered union `a \| b` | `O(n + m)` | `O(k)` |
| Ordered intersection `a & b` | `O(min(n + m, s log(l + 1)) + k)` | `O(k)` |
| Ordered difference `a - b` | `O(min(n + m, n log(m + 1)) + k)` | `O(k)` |
| Ordered symmetric difference `a ^ b` | `O(n + m)` | `O(k)` |
| Unordered union / difference / symmetric difference | Expected `O(n + m)` | `O(k)` |
| Unordered intersection | Expected `O(s)` | `O(k)` |

Ordered intersection chooses between a linear merge and searching the larger
tree for each smaller-input key. Difference similarly avoids scanning a huge
right input when the left is tiny. These paths insert sorted results with end
hints. Unordered intersection iterates the smaller set and retrieves matching
values from the left set, retaining left representatives of equivalent keys.
Unordered union reserves an upper bound once to avoid a second growth rehash;
for overlapping sets this can reserve up to about twice the needed buckets.

Exact self-difference and self-symmetric-difference return an empty result in
constant time. Ordered intersections with disjoint endpoint ranges can also
return in constant time. Other early exits preserve the bounds above.

## In-place set operations

These are the compound operators and their `*_into(a, b)` named equivalents.
Surviving left nodes are retained. `r` below is the number of inserted nodes.

| Container / operation | Time | Extra storage, excluding new left nodes |
| --- | --- | --- |
| Ordered `\|=` / `^=` | `O(min(n + m, m log(n + m + 1)))` | `O(1)` |
| Ordered `&=` | `O(min(n + m, n log(m + 1)))` | `O(1)` |
| Ordered `-=` | `O(min(n + m, s log(l + 1)))` | `O(1)` |
| Unordered `\|=` / `^=` | Expected `O(m)` plus rehash costs | `O(n + r)` buckets if rehashing |
| Unordered `&=` | Expected `O(n)` | `O(1)` |
| Unordered `-=` | Expected `O(s)` | `O(1)` |

Unordered insertions use the standard container's growth policy. A rehash may
visit the existing container, so one small insertion can cost `O(n + r)` on
average. Identity operations `a |= a` and `a &= a` do no work; `a -= a` and
`a ^= a` clear and destroy `n` elements.

Moving the left operand into a binary set operation selects the in-place path
and then moves out its container. It may change the moved-from operand.
These mutating paths provide the basic exception guarantee, with no rollback
of completed insertions/erasures. Ordinary lvalue binary operations preserve
their inputs on failure.

## Hashing and custom policies

Expected hash-table bounds require an appropriate hash distribution. Under
collisions, lookup/erase can scan a bucket of size `O(l)`, and constructing a
result of size `k` can take `O(k^2)`. Thus unordered binary operations can take
`O((n + m)^2)` in the worst case, including rehashing. This coarse quadratic
worst-case bound also applies to unordered compound operations. Intersection has the tighter
coarse bound `O(s*l + k^2)`. The standard does not promise worst-case constant
hash lookup.

Stateful comparators or key-equality policies without a usable equivalence
check require rebuilding the right operand under the left policy. Add
`O(m log(m + 1))` for ordered sets or expected `O(m)` (worst `O(m^2)`) for hashed
sets, and `O(m)` temporary storage. Different hash functions do not require
rebuilding when key equality is compatible. Early exits skip unnecessary
normalization. Stateful policy authors can define equality to establish
equivalent behavior and enable the fast path.

## Sequences

| Operation | Time | Storage |
| --- | --- | --- |
| Lvalue concatenation `a + b` | `O(n + m)` | `O(n + m)` result |
| Vector append, enough capacity | `O(m)` | No new buffer |
| Vector append, growth needed | `O(n + m)` | Geometric buffer growth |
| Deque/list append by copying | `O(m)` | `O(m)` new elements |
| Append moved vector/deque | `O(m)` plus vector growth | Moves right elements |
| Append moved list, equal allocators | `O(1)` | Splices existing nodes |
| Append moved list, unequal allocators | `O(m)` | Allocates/moves right elements |
| Self-append | `O(n)` | No temporary element container; capacity/nodes may grow |

Vector appends double capacity when necessary, with an overflow/max-size guard.
Repeated single-element appends therefore take `O(N)` total relocation work,
rather than the `O(N^2)` possible with reserving the exact new size every time.
Lvalue concatenation reserves the known final vector size once.

Binary operations on moved sequences reuse left storage. A copied left list
followed by a moved right list takes `O(n)` with equal allocators. Two moved
lists concatenate in `O(1)` with equal allocators. Right element moves leave
the source valid but with unspecified element values; list splicing empties it.
Self-append requires copyable elements, including when the argument is moved.

## Conversions

| Conversion | Time | Storage |
| --- | --- | --- |
| Same complete container type, lvalue | `O(n)` copy | `O(n)` result |
| Same complete container type, mutable rvalue | `O(1)` move | Transfers existing storage |
| To vector/deque/list | `O(n)` | `O(n)` result |
| To ordered set, sorted range | `O(n)` | `O(k)` result |
| To ordered set, general input | `O(n log(n + 1))` | `O(k)` result |
| To unordered set | Expected `O(n)`, worst `O(n^2)` | `O(n)` buckets/elements |

Sized vector/hash conversions reserve once. General single-pass input ranges
are consumed once. Ordered sets use the range constructor for common ranges;
other ranges use end hints and skip adjacent equivalent keys, preserving linear
time on sorted streams. Ordinary element conversions copy unless the input yields
rvalue references. A matching mutable container rvalue transfers its storage.

## Zip ranges

For `contara::zip(a, b)`, let `k = min(n, m)`.

| Operation | Zip overhead | Extra storage |
| --- | --- | --- |
| Create a view over lvalues | `O(1)` | `O(1)` |
| Dereference, increment, compare to end | `O(1)` | `O(1)` |
| Complete traversal | `O(k)` | `O(1)` |
| Size, when both inputs are sized | `O(1)` | `O(1)` |
| Index/offset, when both inputs are random access | `O(1)` | `O(1)` |
| Materialize to vector | `O(k)` plus element copies/moves | `O(k)` result |

There is no intermediate pair container. Dereferencing containers yields a
small pair of references/proxies; dereferencing value-producing views follows
their own costs. `to_vector` materializes `zip_pair` values, not reference rows.
Owning rvalue inputs retain their original storage through the underlying view's
move semantics. Underlying range operations and user callbacks retain their own
time/allocation costs. Single-pass iterators may read an unpaired element while
discovering the end; no pairs beyond the shorter input are yielded.

Random-access sized inputs have a common end iterator positioned at the shorter
length, so obtaining the end and reversing the view require no linear search.
Other inputs use a sentinel that stops when either constituent reaches its end.
The view preserves forward/bidirectional capability where both inputs provide it.

## Parallel map

For `p` workers and uniform callback cost `C`, total work is `O(n*C + n + p)`.
The ideal concurrent callback component is `O(ceil(n/p)*C)`. Output initialization,
generic-result collection, allocation, thread startup/join, and scheduling
remain additional costs; this is not a blanket `O(n/p)` wall-clock guarantee.

Trivially default-constructible, trivially copyable, assignable non-bool values
write directly to the final vector: output storage `O(n)`, auxiliary storage
`O(p)`. Other result types use per-worker vectors and a final ordered move pass:
auxiliary storage `O(n + p)`, plus the `O(n)` output. Boolean workers use separate
packed buffers to prevent concurrent writes to shared bits. Cancellation is
checked every 256 elements; callbacks already in progress may still complete.
Small inputs run sequentially, using only output storage.

## Standard-library basis

- [Associative containers: lookup, hints, insertion and erasure](https://eel.is/c++draft/associative.reqmts)
- [Vector modifiers and reallocation](https://eel.is/c++draft/vector.modifiers)
- [Ordered set range construction](https://eel.is/c++draft/set.cons)
- [List splicing and allocator requirements](https://eel.is/c++draft/list.ops)
- [Container element data-race requirements, including vector bool](https://eel.is/c++draft/container.requirements.dataraces)
- [Standard zip-view semantics](https://eel.is/c++draft/range.zip)
