### B.29 Single-Byte Prefix with `CRSCLTP` Suffix (Skipped)

**Decision: not worth running.** B.29 was evaluated after B.28 was skipped and the same
reasoning applies with equal force:

1. **Same architecture, different byte position.** B.29 varies the MSB (bits 63..56) while
   keeping `CRSCLTP` as the trailing 7 bytes — the mirror image of B.26d (vary LSB, keep
   `CRSCLTP` as the leading 7 bytes). Both give 256 candidates per axis within the same
   single-permutation-per-seed paradigm. The search space per axis is identical in size and
   the structural ceiling problem is unchanged.

2. **LCG trajectory analysis confirms symmetry.** In a multiplicative LCG
   (`state = a·state + c mod 2^64`), varying the MSB shifts the starting state by `Δ << 56`.
   After *n* steps the two trajectories differ by `a^n·(Δ << 56) mod 2^64` — structurally
   identical to varying the LSB by a different linear offset in the same cycle. There is no
   mechanism that makes MSB variation qualitatively different from the already-exhausted LSB
   variation in a Fisher-Yates/LCG context.

3. **Seeds 3+4 invariance is the diagnostic, not byte position.** The flatness observed in
   B.26c for seeds 3 and 4 was not caused by the byte that was varied. It reflects that
   additional uniform-511 sub-tables contribute zero marginal constraint discriminability once
   seeds 1+2 are optimised. Varying the MSB of further seeds faces the same barrier.

4. **B.26c exhausted the LSB family exhaustively.** Its global maximum (91,090) is the true
   ceiling for that family. B.29's MSB family is disjoint and could in principle have a higher
   ceiling — but there is no theoretical mechanism making MSB variation qualitatively superior,
   so this remains low-probability speculation.

5. **The hill-climber is the higher-leverage next step.** Directly optimising LTP cell
   assignments gives O(s⁴) ≈ 6.8 × 10¹⁰ degrees of freedom and can target row-concentration
   geometry explicitly. B.29's expected return does not justify the cost.

The original proposal is preserved below for reference.

#### B.29.1 Motivation (Original Proposal)

B.26d and B.27 search seeds of the form `CRSCLTP` + one variable byte: the high 7 bytes are
fixed and byte 8 (the LSB, big-endian) varies. Both families share the property that the
*leading* bytes of the 64-bit seed are fixed and the *trailing* bytes vary.

B.29 inverts this structure. It fixes `CRSCLTP` as the trailing 7 bytes (the suffix) and
sweeps a single variable byte in the leading position (the MSB):

$$\text{B.26d/B.27:}\quad \underbrace{C R S C L T P}_{7\text{ fixed}} \underbrace{X}_{1\text{ var}}
\qquad
\text{B.29:}\quad \underbrace{X}_{1\text{ var}} \underbrace{C R S C L T P}_{7\text{ fixed}}$$

The seed values (big-endian uint64):

$$\text{B.26d seed}(X) = \texttt{0x435253434C5450}\lVert X
\qquad
\text{B.29 seed}(X) = X \lVert \texttt{0x435253434C5450}$$

where $\lVert$ denotes byte concatenation and $X \in [0\text{x}00, 0\text{x}FF]$.

These two families are **disjoint**: no seed value is reachable by both. The B.26d family fixes
bits 63..8 at `0x435253434C5450` and varies bits 7..0. The B.29 family fixes bits 55..0 at
`0x435253434C5450` and varies bits 63..56. The only potential overlap would require simultaneously
fixing and varying the same bits, which is impossible.

**Hypothesis.** The Fisher-Yates LCG shuffle is sensitive to all 64 bits of its seed; there is
no reason to expect the high byte to be irrelevant. Seeds where the low 7 bytes spell `CRSCLTP`
but the high byte differs from `C` (0x43) may produce partition layouts with tighter line
co-residency at the plateau boundary, yielding higher depth.

#### B.29.2 Seed Structure

| Role | Bytes | Bit positions | Value |
|------|-------|---------------|-------|
| Prefix (variable) | byte 0 | 63..56 | `0x00`–`0xFF` |
| Suffix (fixed) | bytes 1–7 | 55..0 | `0x435253434C5450` (`CRSCLTP`) |

The 256 B.29 seeds per axis are:

$$s(X) = (X \ll 56) \mid \texttt{0x435253434C5450}, \quad X \in \{0, 1, \ldots, 255\}$$

When $X = \texttt{0x43}$ (`C`), the seed is `0x43435253434C5450` — note that this is **not** a
valid B.26d seed; B.26d's `C`-prefixed seed with suffix `P` is `0x435253434C545050` (CRSCLTPP),
which has a different bit layout.

#### B.29.3 Coordinate-Descent Staging

- **Seeds per axis:** 256 (same as B.26d/B.27 and B.28)
- **Cost per axis sweep:** $256 \times 23\,\text{s} \div 4\,\text{workers} \approx 26$ minutes
- **Convergence round (two axes):** $\approx 52$ minutes; 2–3 rounds typical ($\approx 2$ hours)

**Procedure.** Fix $s_6$ at its default B.29 seed ($X = \texttt{0x43}$, i.e., prefix byte =
`C`, yielding `0x43435253434C5450`); sweep all 256 prefix-byte values for $s_5$; freeze the
best $s_5$; sweep all 256 prefix-byte values for $s_6$; freeze the best $s_6$; iterate until
no axis improves. This is identical in structure to the B.27 and B.28 staging procedures.

**Cross-family restart** *(original proposal, superseded — B.28 and B.29 both skipped).*
After B.29 converged, the plan was to re-run B.27 (LSB) sweeps with seeds fixed at the B.29
winner to verify no further improvement. This step is moot.

#### B.29.4 Relationship to the Current Seed Families

The seed families considered across B.26c–B.29:

| Experiment | Seed form | Variable bits | Space/axis | Status |
|-----------|-----------|---------------|-----------|--------|
| B.26c | `CRSCLTP` + alphanum suffix | bits 7..0, restricted | 36 | Completed — global max 91,090 |
| B.26d / B.27 | `CRSCLTP` + any suffix byte | bits 7..0 | 256 | Completed — confirmed 91,090 ceiling |
| B.28 | `CRSC[X]LTP` interior byte | bits 31..24 | 256 | Skipped — same objection as B.29 |
| B.29 | prefix `[X]` + `CRSCLTP` | bits 63..56 | 256 | Skipped — see rationale above |

B.26c and B.26d/B.27 together exhaustively covered the LSB family. B.28 and B.29 were
proposed to probe different bit zones but were skipped because varying a different byte position
within the same single-permutation-per-seed LCG architecture offers no qualitative increase in
degrees of freedom and is not expected to break the 91,090 ceiling.

#### B.29.5 Tooling

A new script `tools/b29_seed_search.py`, structured identically to `tools/b27_seed_search.py`
with one change in seed construction:

```python
_SUFFIX = 0x435253434C5450  # "CRSCLTP" as a 56-bit integer (big-endian bytes 1..7)

def make_seed(prefix_byte: int) -> int:
    return (prefix_byte << 56) | _SUFFIX
```

The worker partitioning, run_candidate logic, progressive JSON output, and resume support are
unchanged. A companion `tools/b29_merge_results.py` mirrors `tools/b27_merge_results.py`.

#### B.29.6 Expected Outcomes

**Optimistic.** A prefix byte other than `0x43` (`C`) yields a substantially higher depth for
one or more seeds. This would prove that the convention of starting seeds with `C` (from
`CRSCE`) has been silently constraining the search and that better partition layouts exist.

**Likely.** Depth is approximately flat across all 256 prefix values for each seed, similar to
the B.22 observation that seeds 3 and 4 were invariant within the alphanumeric set. The plateau
at 91,090 cells is insensitive to the high byte of the seed, and the LCG high-bit sensitivity
does not translate to partition-quality sensitivity.

**Pessimistic.** Prefix `0x43` (`C`) is the best for every seed, confirming that the `CRSCLTP?`
convention is not merely arbitrary — the high bits of the seed do matter, and the current
family happens to be optimal (or near-optimal) within the tractable search space.

#### B.29.7 Retrospective: Conclusion of the Seed-Search Programme

B.26d/B.27 (LSB, completed) together with the decision to skip B.28 (interior byte) and B.29
(MSB) complete the seed-search programme. The combined evidence supports a clear conclusion:

- **B.26c** exhaustively covered the LSB family (256×256 pairs) and found the global maximum
  at 91,090. The landscape is rugged (range 84,265–91,090, mean 86,941) with only 2.2% of
  pairs beating the B.22 baseline — but the ceiling is definite.
- **B.27 series** (B.27, B.27a–c) established that additional uniform-511 sub-tables (LTP5/6)
  provide zero marginal benefit regardless of seed values or operating point.
- **B.28 and B.29 were skipped** because varying a different byte position in the same
  LCG-seeded Fisher-Yates architecture does not increase degrees of freedom and there is no
  theoretical mechanism by which MSB or interior-byte variation escapes the same structural
  ceiling.

The strong inference is that 91,090 is not a seed-search artefact but a structural property
of the uniform-511 partition geometry under the current solver. Seed search within any
single-byte-variable `CRSCLTP`-family is exhausted. The natural next direction is the
**hill-climber**: directly optimising cell assignments with O(s⁴) degrees of
freedom, targeting the row-concentration geometry identified in B.9.2.

---

