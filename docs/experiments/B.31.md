### B.31 Pincer Decompression — Option B: Full Alternating-Direction DFS (Proposed)

#### B.31.1 Motivation

B.30 (Option A) runs a propagation-only sweep from the bottom after a top-down stall. However,
the `PropagationEngine` is already a fixed-point computation — it forces every cell that can be
deduced from the current partial assignment after every branching decision. A pure re-propagation
sweep on the same ConstraintStore state will find no new forced cells and is therefore expected
to be a no-op or near-no-op.

The mechanism that makes the Pincer hypothesis genuinely powerful is **SHA-1 row hash
verification**, not propagation. When the bottom-up DFS assigns all 511 cells in a bottom row
and verifies its SHA-1 hash, it eliminates an entire exponential subtree of infeasible bottom
assignments. This constraint is non-linear and global — it cannot be derived from any
cardinality propagation over the line constraints alone. Propagation has no access to this
information; only branching decisions that complete bottom rows can trigger it.

**Option B implements the full Pincer:** the solver actually branches in both directions,
completing rows from row 510 upward after the top-down plateau. The SHA-1 verifications on
completed bottom rows tighten the residual constraint network available to the resumed top-down
pass, providing the constraint-density improvement described in B.30.3.

#### B.31.2 DI Consistency

A potential concern is that alternating-direction branching changes the enumeration order, making
the DI invalid. This concern does not apply: **the compressor runs the same decompressor code
to find the DI**. Whatever enumeration order the decompressor uses, the compressor uses
identically. The DI is the zero-based position of the correct solution in the decompressor's
enumeration — its definition is stable regardless of which direction cells are assigned in,
as long as the enumeration is deterministic and reproducible.

The alternating-direction DFS is fully deterministic: given the same constraint system, it
always produces the same direction switches and the same cell selections. DI is therefore
well-defined and consistent between compression and decompression.

#### B.31.3 The Alternating Structure

B.31 adds a `direction` state to the DFS and maintains two independent frontiers:

| State | Description |
|-------|-------------|
| `direction` | `TOP_DOWN` or `BOTTOM_UP` — current traversal direction |
| `topFrontier` | Lowest row with any unassigned cell (advances downward) |
| `bottomFrontier` | Highest row with any unassigned cell (advances upward) |
| `stall_count` | Branching decisions since the last forced cell in current direction |

**Plateau trigger:** when `stall_count` reaches threshold $N$ (candidate: 511 — one full row
of unconstrained branching), declare a plateau and switch direction. Reset `stall_count`.

**Convergence:** if both directions have plateaued without any new forced cell since the last
direction switch, the meeting band (rows `topFrontier`..`bottomFrontier`) is irreducibly hard
with the current assignments. The solver falls back to standard DFS within the meeting band in
row-major order, or terminates the alternation and continues with the existing single-direction
strategy.

**Termination:** when `topFrontier > bottomFrontier` (the two frontiers cross), all rows are
assigned. SHA-256 block hash verification; if valid, yield the solution and advance the DI
counter.

#### B.31.4 Cell Selection in Each Direction

The existing DFS pre-computes a `cellOrder` sorted by probability confidence. For B.31:

- **TOP_DOWN ordering** (existing): cells in row-major order, reordered by confidence.
  Unchanged from current implementation.

- **BOTTOM_UP ordering** (new): cells in *reverse* row-major order (rows 510→0, within
  each row by decreasing confidence). Pre-computed once at the start of `enumerateSolutionsLex`
  alongside the existing top-down ordering.

The DFS stack is extended with a `direction` field per frame. When a direction switch occurs,
the solver pushes a new frame using the opposite ordering starting from the appropriate
frontier. The existing row-completion heap (rows with $u \leq 64$) applies in both directions —
in BOTTOM_UP mode it preferentially selects near-complete rows from the bottom.

#### B.31.5 Unified Undo Stack

The undo stack (`brancher_->undoToSavePoint / recordAssignment`) is **direction-agnostic**:
it records `(r, c)` pairs regardless of which direction made the assignment. When a backtrack
crosses a direction-switch point (unwinding bottom-up assignments while in the top-down phase),
the ConstraintStore correctly unassigns those cells and restores line residuals. No special
handling is required.

The `UndoToken` mechanism (stack-size save points) already provides this: a token saved before
a direction switch, when restored, undoes all assignments in both directions since the save
point.

#### B.31.6 SHA-1 Verification

Row $r$ verifies its SHA-1 when `cs.getStatDirect(r).unknown == 0`, regardless of direction.
In BOTTOM_UP mode, the solver verifies rows as they complete in the sequence 510, 509, 508, …
No changes to the hash verification infrastructure are needed.

#### B.31.7 Proposed Implementation

**New state (local to `enumerateSolutionsLex`):**
```cpp
enum class SolverDirection : std::uint8_t { TopDown, BottomUp };
SolverDirection direction    = SolverDirection::TopDown;
std::uint16_t   topFrontier  = 0;     // next unassigned row from top
std::uint16_t   bottomFrontier = kS - 1; // next unassigned row from bottom
std::uint32_t   dirStallCount  = 0;   // branching decisions since last forced cell
std::uint32_t   kDirStallMax   = kS;  // plateau threshold (511 = one row)
std::uint64_t   dirSwitches    = 0;   // total direction switches
```

**Precompute bottom-up ordering** (alongside existing `cellOrder`):
```cpp
auto bottomOrder = estimator.computeGlobalCellScoresReverse();
// or: reverse of cellOrder sorted by row descending
```

**In the DFS loop — per-frame direction:**
```cpp
struct ProbDfsFrame {
    // ... existing fields ...
    SolverDirection direction{SolverDirection::TopDown}; // NEW
};
```

**Stall detection for direction switch:**
```cpp
// After assignment + propagation succeeds:
if (/* propagation forced no cells && no heap selection */) {
    ++dirStallCount;
    if (dirStallCount >= kDirStallMax) {
        dirStallCount = 0;
        ++dirSwitches;
        direction = (direction == SolverDirection::TopDown)
            ? SolverDirection::BottomUp
            : SolverDirection::TopDown;
    }
}
```

**Cell selection by direction:**
```cpp
const auto &activeOrder = (direction == SolverDirection::TopDown)
    ? cellOrder : bottomOrder;
// Heap still applies in both directions (near-complete rows from appropriate end)
```

#### B.31.8 Expected Outcomes

**Optimistic.** Bottom-up DFS completes rows 510 → ~332 via SHA-1-guided backtracking. Each
completed bottom row provides a non-linear constraint that cardinality propagation cannot
express. When top-down resumes at `topFrontier`, many meeting-band cells are forced outright
by the combined top-down + bottom-up constraint tightening. The alternation converges in 2–3
switches. Effective branching space falls from ~170K underdetermined cells to ~10–30K. Wall
time decreases by 1–2 orders of magnitude.

**Likely.** Bottom-up achieves a shallow but nonzero cascade (10–50 rows from the bottom)
before SHA-1 failures become frequent. Each direction switch provides incremental constraint
tightening. The meeting band shrinks modestly. Net speedup is 2–5× in wall time, not an order
of magnitude. Multiple direction switches (5–10) are required before convergence.

**Pessimistic.** The residual constraint budgets after the forward pass are too tight for
bottom-up branching: SHA-1 failures begin almost immediately from row 510 because the top-down
pass has exhausted many line budgets, leaving no feasible bottom-row assignments near the
"expected" region. Bottom-up barely advances; the direction switches add overhead without
benefit. Performance degrades to below the B.30.4 / current baseline.

#### B.31.9 Relationship to B.30

B.30 and B.31 are the same Pincer hypothesis at different implementation depths:

| | B.30 (Option A) | B.31 (Option B) |
|---|---|---|
| Bottom-up phase | Propagation sweep (no branching) | Full DFS with branching |
| SHA-1 in bottom-up | Not triggered (no row completions) | Triggered for each completed row |
| Expected benefit | Likely zero (propagation at fixed-point) | Real: SHA-1 eliminates subtrees |
| Implementation cost | Minimal (10–20 lines) | Substantial (150–250 lines) |
| DI change | None (top-down order unchanged) | Deterministic alternating order |

B.30 is implemented first as a cheap probe. If it produces zero forced cells (confirming the
no-op hypothesis), this validates the need for B.31 and eliminates any ambiguity about whether
propagation alone is sufficient.

#### B.31.10 Open Questions

(a) **What is the bottom-up plateau depth?** Empirical measurement from B.31 first run will
answer B.30.10(a) definitively.

(b) **What is `kDirStallMax`?** The threshold for direction switching. Too small → switches too
often (overhead dominates). Too large → misses the beneficial switch point. Candidate starting
values: 511 (one row), 1024, 4096.

(c) **Is the row-completion heap useful in BOTTOM_UP mode?** The existing heap selects rows
with $u \leq 64$ and applies in both directions. In BOTTOM_UP, near-complete rows are those
near row 510. The heap naturally captures this without modification.

(d) **Should the DI still use lex order?** The alternating DFS enumerates in a deterministic
but non-lex-row-major order. The compressor, running the same code, finds the DI in the same
order. All that matters for correctness is consistency. "Lexicographic" in the DI specification
is therefore generalized to "alternating-DFS order" in B.31.

---

