# CRSCE v1 CSM Refactor Specification

This document specifies the refactor of the Cross‑Sum Matrix (CSM) container used during decompression/solving. The
packed bit‑vector is replaced by a contiguous 2D grid with embedded per‑cell metadata and concurrency controls, a
rotated (d,x) pointer view is introduced, cross‑sum counts are maintained live, and addressing helpers are consolidated
into the `Csm` class. No feature flag; this fully replaces the existing packed implementation.

We are capturing this here for posterity and to facilitate discussion and review when I write my report. ...and also
so I can remember what I did later on and why.

## 1. Goals

- Replace the packed bit layer with a contiguous 511×511 grid of 1‑byte cells that pack: data bit (bit0), per‑cell
  mutex (bit6), and solved‑lock (bit7) to improve performance.
- Provide rc and dx APIs with full parity: `get_rc/put_rc/lock_rc/is_locked_rc` and
  `get_dx/put_dx/lock_dx/is_locked_dx`. The underlying locks are the same but they are addressed through the two
  different coordinate systems (r,c) and (d,x).
- Move `calc_d(r,c)` and `calc_x(r,c)` into `Csm` as public, constexpr helpers; keep existing wrappers delegating to the
  class.
- Maintain live cross‑sum counters for LSM/VSM/DSM/XSM in fixed `uint16_t` C‑arrays; reads are O(1).
- Define clear concurrency semantics with:
    - Per‑cell mutex bit (mu, bit6) for fine‑grained exclusion.
    - 511‑element series locks for rows, columns, diagonals, and antidiagonals to serialize multi‑family updates.
- Support optional coroutine‑based async count updates while honoring locks. Synchronous path is the default baseline.
- Preserve a rotated (d,x) “view” via fixed pointer tables into the base (r,c) grid; there is one source of truth
  regardless of versioning.
- Ensure strong back‑compat for existing call sites; provide rc aliases for legacy names.
- Make as much as we can into `constexpr` to avoid runtime overhead.

## 2. Constants and Notation

- Dimension: `S = 511`.
- Families: row (r), column (c), diagonal (d), antidiagonal (x).
- Index relations (kept indefinitely):
    - `d = (c - r) mod S` → refer to `Csm::calc_d(r,c)`
    - `x = (r + c) mod S` → refer to `Csm::calc_x(r,c)`
    - This is important: I keep using modular math I've memorized rather than efficient operations I've learned.
      - In the future I might consider adding linter rules to enforce this.
- All indices are 0‑based with bounds `[0, S)`.

## 3. Data Structures

### 3.1 Bits (per‑cell)

One byte per CSM cell with embedded flags:

- bit0: data (0 or 1)
- bit6: mu (mutex) — 1 when held
- bit7: solved‑lock — 1 when the cell is immutable to data writes

Masks: `kData=0x01`, `kMu=0x40`, `kLock=0x80`. All other bits are 0. `static_assert(sizeof(Bits) == 1)`.

Required API (atomic operations via `std::atomic_ref<uint8_t>` on the underlying byte):

- Constructors: default=0; from `(bool data,bool lock,bool mu=false)`; from raw (masked to data|mu|lock).
- Accessors: `bool data() const; bool locked() const; bool mu_locked() const; uint8_t raw() const`.
- Mutators (RMW on the single byte):
    - `void set_data(bool v)`; `void flip_data()`
    - `void set_locked(bool on=true)`
    - `void set_mu_locked(bool on=true)`
    - `void assign(bool data,bool lock,bool mu=false)`
    - `void set_raw(uint8_t v)`; `void clear()`
- Mutex helpers:
    - `bool try_lock_mu()` — atomic test‑and‑set bit6; returns true on acquisition (Acquire).
    - `void unlock_mu()` — atomic clear bit6 (Release).
    - Optional RAII `ScopedMu` that spins with bounded backoff to acquire, releases in dtor.
- Operators: `explicit operator bool()` → data(); `Bits& operator=(bool v)` → set_data(v); equality by `raw()`.

Policy: Bits exposes mechanisms; `Csm` enforces policy (e.g., throws on writes to solved‑locked cells).

### 3.2 Csm (container)

Primary fields:

- `Bits cells_[S][S]` — row‑major base matrix; single source of truth for bits and locks.
- `double data_[S][S]` — per‑cell belief/score in [0,1]; advisory only.
- `std::atomic<uint64_t> row_versions_[S]` — increments when any data bit in row r changes (writes via rc or dx).
- Cross‑sum counters (live, O(1) reads/updates):
    - `uint16_t lsm_c[S]` — row sums (ones per row)
    - `uint16_t vsm_c[S]` — column sums
    - `uint16_t dsm_c[S]` — diagonal sums
    - `uint16_t xsm_c[S]` — antidiagonal sums
- Series locks (spinlocks) to coordinate multi‑family updates:
    - `atomic_flag row_mu[S], col_mu[S], diag_mu[S], xdg_mu[S]` (or equivalent custom spinlock);
      initialized clear; Acquire on lock, Release on unlock.
- Rotated (d,x) view (fixed aliasing into base grid):
    - `Bits* dx_cells_[S][S]` — `dx_cells_[d][x] == &cells_[r][c]` for the rc that maps to (d,x)
    - `uint16_t dx_row_[S][S]` — corresponding row index r for version bumps on dx writes

Invariants:

- `dx_cells_` and `dx_row_` are built once on construction/reset/copy/move and never re‑pointed; there is only one
  truth: `cells_`.
- `Csm::calc_d/x` are consistent with the `dx_cells_` mapping.

## 4. Public Interface

### 4.1 Construction and Reset

- `Csm()` — zero‑initialize all storage, counters, versions, and locks; build `dx_cells_` and `dx_row_` tables.
- `void reset() noexcept` — as above.
- Copy/move ctor/assign — copy storage and counters; rebuild `dx_cells_` and `dx_row_` to point into the destination
  object.

### 4.2 Addressing Helpers

- `static constexpr size_t calc_d(size_t r,size_t c) noexcept;`
- `static constexpr size_t calc_x(size_t r,size_t c) noexcept;`

These retain the current branchless, modulus‑free implementations. Keep thin wrappers in
`include/decompress/Utils/detail/calc_{d,x}.h` that delegate to `Csm`.

### 4.3 rc APIs

- `bool get_rc(size_t r,size_t c) const;`
- `void put_rc(size_t r,size_t c,bool v,bool lock=true, bool async=false);`
- `void lock_rc(size_t r,size_t c,bool lock=true);`
- `bool is_locked_rc(size_t r,size_t c) const;`
- Data layer: `double get_data(size_t r,size_t c) const;`
- Data layer write: `void set_data(size_t r,size_t c,double value,bool lock=true, bool async=false);`

Legacy aliases for continuity: `get/put/lock/is_locked` are rc aliases.

### 4.4 dx APIs

- `bool get_dx(size_t d,size_t x) const;`
- `void put_dx(size_t d,size_t x,bool v,bool lock=true, bool async=false);`
- `void lock_dx(size_t d,size_t x,bool lock=true);`
- `bool is_locked_dx(size_t d,size_t x) const;`

### 4.5 Counts (live)

- Arrays: `uint16_t lsm_c[S], vsm_c[S], dsm_c[S], xsm_c[S]` maintained by writes.
- Queries (return `uint16_t`):
    - `uint16_t count_lsm(size_t r) const noexcept;`
    - `uint16_t count_vsm(size_t c) const noexcept;`
    - `uint16_t count_dsm(size_t d) const noexcept;`
    - `uint16_t count_xsm(size_t x) const noexcept;`

## 5. Semantics and Errors

- Bounds: any rc/dx access with index ≥ S throws `CsmIndexOutOfBounds(r_or_d, c_or_x, S)`.
- Solved‑locks: any `put_*` into a solved‑locked cell throws `WriteFailureOnLockedCsmElement`.
- Versioning: increment `row_versions_[r]` only when the data bit changes value; lock changes and data writes that are
  no‑ops do not bump.
- Data layer: `set_data` honors locking parameters (mu), but solved‑lock does not block data layer updates unless
  explicitly configured otherwise. Callers skip locked cells for data updates; this matches current behavior.

## 6. Concurrency Model

### 6.1 Locking order and rules

To avoid deadlocks and keep updates atomic across all families, `put_rc/put_dx` take locks in this fixed global order:

1) series lock row(r) → 2) series lock col(c) → 3) series lock diag(d) → 4) series lock xdg(x) → 5) cell mu (bit6)

Release order is the exact reverse: mu → xdg → diag → col → row. Ties (same family type) are resolved by index
ascending. This order applies identically to rc and dx writes (with r,c derived for dx via tables).

Notes:

- `lock=true` (default) acquires the per‑cell mu bit; `lock=false` skips cell‑level mu only (series locks for counts are
  still acquired when counts must be updated). Setting `lock=false` is for tightly‑controlled hot loops that already
  hold exclusivity by construction.
- Series locks guard the four counters consistently with the bit flip. They can also be used by higher‑level algorithms
  to serialize row/column/diag/xdiag‑wide operations when needed.
- Mu is a short critical‑section spinlock; not reentrant and not fair. Intended for very brief per‑cell exclusivity.

### 6.2 Memory ordering

- Mu acquire uses Acquire semantics; mu release uses Release.
- Series locks use Acquire on lock and Release on unlock (e.g., clearing an `atomic_flag`).
- `row_versions_[r].fetch_add(1, std::memory_order_relaxed)` is sufficient because visibility is bounded by the above
  locks; readers that care about order can also consult series locks externally.

### 6.3 Reads

- `get_*` and `count_*` do not acquire locks. They read the current value; races with concurrent writers are benign
  under the above ordering. If stronger read consistency is required, callers can coordinate via series locks.

### 6.4 Guard typedefs (RAII)

To standardize locking and ensure exception‑safe release, provide small RAII helpers and typedefs:

- `using SeriesLock = /* atomic_flag or custom spinlock type */;`
- `struct SeriesGuard { /* acquires one series lock in ctor; releases in dtor */ };`
- `struct SeriesScopeGuard { /* acquires a fixed ordered tuple of series locks (row→col→diag→xdg); releases in reverse */ };`
- `struct CellMuGuard { /* spins on Bits::try_lock_mu() with bounded backoff; releases in dtor */ };`

These helpers encode the canonical acquisition order and guarantee unlock on all control paths.

### 6.5 Snapshot helper (diagnostics)

Provide a helper to take a consistent diagnostic snapshot of all four cross‑sum arrays:

- `struct CounterSnapshot { uint16_t lsm[S]; uint16_t vsm[S]; uint16_t dsm[S]; uint16_t xsm[S]; };`
- `CounterSnapshot take_counter_snapshot() const;`

Semantics:

- Internally acquires all series locks in canonical order (row[0..S‑1] → col[0..S‑1] → diag[0..S‑1] → xdg[0..S‑1]) via a
  `SeriesScopeGuard`, copies counters, then releases in reverse order.
- This is heavyweight and intended only for occasional diagnostics/telemetry. For targeted checks, prefer acquiring a
  single family’s series lock and reading its counter array directly.

## 7. Write Operations (rc/dx)

Shared algorithm for `put_rc(r,c,v, lock=true, async=false)` and `put_dx(d,x,v, lock=true, async=false)`:

1) Compute all four family indices: r,c,d,x (dx resolves r via `dx_row_` and c via `dx_cells_`).
2) Acquire series locks in the fixed order (row→col→diag→xdg).
3) If `lock==true`: acquire cell mu (spin, bounded backoff) via `try_lock_mu()`.
4) If solved‑lock is set, release held locks in reverse order and throw `WriteFailureOnLockedCsmElement`.
5) Read old data bit. If `v == old`, skip counter changes and version bump; still release locks.
6) Otherwise, write new data bit to the cell.
7) Update cross‑sum counters (see §8) for the four families: +1 for 0→1, −1 for 1→0.
8) Bump `row_versions_[r]`.
9) Release locks in reverse order.

Error handling: Exceptions unwind and release any locks already acquired. Implement via small helper that tracks
acquired stages.

## 8. Cross‑Sum Counters (live maintenance)

- Maintained arrays: `lsm_c`, `vsm_c`, `dsm_c`, `xsm_c` of length `S`, `uint16_t` each.
- Updates happen under the series locks, in the same critical section as the bit change. Overflows are undefined
  behavior (inputs guarantee sums ≤ S, which fits in 9 bits; `uint16_t` is sufficient).
- Queries: `count_*` simply return the corresponding array element; no locking.

## 9. Rotated (d,x) View

- On construction/reset/copy/move, build the tables by iterating the base grid once:
    - For each `(r,c)`, compute `d = calc_d(r,c)`, `x = calc_x(r,c)`; set `dx_cells_[d][x] = &cells_[r][c]` and
      `dx_row_[d][x] = static_cast<uint16_t>(r)`.
- Invariants:
    - `dx_cells_` always alias `cells_`; they are never re‑pointed during the object’s lifetime.
    - Any write to a cell (via rc or dx) mutates the same underlying `Bits` instance.

## 10. Data Layer

- `get_data(r,c)` returns the per‑cell score in [0,1].
- `set_data(r,c,value, lock=true, async=false)` honors the same mu policy as bit writes, but does not interact with
  solved‑lock unless the caller chooses to enforce it externally (mirrors current behavior where data updates skip
  solved cells at call sites).
- Data writes do not affect cross‑sum counters or row versions.

Optional symmetry for dx‑oriented code paths (can be added when needed):

- `double get_data_dx(size_t d,size_t x) const;`
- `void set_data_dx(size_t d,size_t x,double value,bool lock=true, bool async=false);`

These delegate to the same underlying `data_[r][c]` storage using the rotated mapping and honor the same locking
semantics.

## 11. Asynchronous Operations (Coroutines)

Objective: reduce put latency by deferring non‑critical work (e.g., counter updates) when allowed.

API surface:

- `put_rc/put_dx(..., bool lock=true, bool async=false)` — when `async==true`, the method:
    - Takes the same locks (series + mu) to atomically flip the data bit.
    - Bumps `row_versions_[r]` immediately.
    - Enqueues a coroutine/task to apply the four counter updates while still honoring the series locks; the task
      acquires series locks just for the counter delta application and releases them.
    - Releases mu promptly after flipping the bit; series locks are held only within the coroutine’s short window.
- `set_data(..., bool lock=true, bool async=false)` — can also be deferred (no counters/versions involved); still honors
  mu policy.
- `void flush()` — waits for all pending tasks to complete (useful before validation reads that require fully consistent
  counters).

Executor:

- A low‑latency thread pool or cooperative event loop suitable for very small tasks. Exact implementation is left to the
  code phase; tasks MUST never block while holding series locks for long periods.

Defaults:

- The default behavior is synchronous (`async=false`) to preserve strict consistency. Async can be enabled per‑call or
  via a future `set_async_mode()` if desired.

Read consistency in async mode:

- When `async==true`, `count_*` queries are eventually consistent and may temporarily lag the underlying bit state
  until deferred counter updates complete. Use `flush()` before invariant checks or telemetry that require strict
  consistency between `get_*` bit reads and `count_*` family sums.

## 12. Exceptions

- `CsmIndexOutOfBounds` — thrown on any rc/dx API call with index ≥ S.
- `WriteFailureOnLockedCsmElement` — thrown on any data bit write to a solved‑locked cell.

## 13. Compatibility and Migrations

- `get/put/lock/is_locked` remain as rc aliases for continuity.
- Existing `calc_d.h` and `calc_x.h` headers remain and delegate inline to `Csm::calc_d/x` (no deprecation for now).
- All usage of the previous packed bit utilities (`bit_mask`, `byte_index`, etc.) is removed.
- Call sites that enumerated families with modulo can move to `dx_cells_` traversal to avoid repeated index math in hot
  loops (optional; not required by this refactor).

## 14. Performance Considerations

- Hot loops:
    - dx traversal uses `dx_cells_` to avoid per‑cell modulo or calc calls.
    - rc traversal is contiguous in memory for strong cache locality.
- Concurrency:
    - Per‑cell mu guards very small sections; series locks are acquired in a strict order to avoid deadlocks; avoid
      holding locks across slow operations.

## 15. Initialization, Copy/Move, and Reset

- `Csm()` initializes all arrays to zero, clears locks, sets row versions to 0, and builds `dx_cells_`/`dx_row_`.
- `reset()` does the same in place.
- Copy/move operations copy all state but rebuild `dx_*` so pointers never alias another object’s storage.

## 16. Validation Plan

- Bits unit tests:
    - Data/lock/mu setters/getters; raw masking; `try_lock_mu`/`unlock_mu` semantics; `ScopedMu` backoff and release.
- Indexing:
    - `Csm::calc_d/x` equivalence to current helpers across the full domain.
- Rotated view:
    - `dx_cells_[d][x]` always refers to the correct `(r,c)`; changes via rc reflect via dx and vice versa.
- Counters:
    - Initial zero; 0→1 increments and 1→0 decrements across all four families; no counter drift over random operations.
- Concurrency:
    - Stress tests with concurrent writers on distinct and same rows/cols; ensure no deadlock with the prescribed lock
      order; versions increment exactly on data changes.
- Copy/reset:
    - After copy/move/reset, mappings and counters are coherent.
- Async mode:
    - With `async=true`, counters eventually consistent; `flush()` yields consistent state matching synchronous
      semantics.

## 17. Usage Guidelines

- Prefer rc access for row‑major sweeps; prefer dx traversal for diagonal/antidiagonal sweeps via `dx_cells_`.
- For batch operations on a single family, optionally acquire the corresponding series lock once externally to reduce
  lock churn.
- Keep critical sections short; never perform heavy work while holding series locks or a cell mu.
- Use `lock=false` only when higher‑level logic already guarantees exclusivity.
- When using async writes (`async=true`), be aware that `count_*` may lag bit state until pending tasks finish. Call
  `flush()` before any invariant checks (e.g., row/diag/xdiag sum verifications, telemetry snapshots) that require
  strict consistency between cell bits and family counters.

## 18. Out‑of‑Scope

- Changing the data layer semantics (still advisory) or tying it to solved‑locks.
- Persisting the data layer or counters; they are runtime‑only.
- Introducing a feature flag; this refactor fully replaces the packed implementation.

---

This specification is complete and implementation‑ready. Deviations, if any, should be discussed and reflected here
before coding.
