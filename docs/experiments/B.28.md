### B.28 Interior-Byte Variant: `CRSC[X]LTP` (Skipped)

**Decision: not worth running.** After completing B.27c, the case for B.28 was re-evaluated
and the experiment was skipped for the following reasons:

1. **B.26c was exhaustive, not coordinate descent.** It evaluated all 256×256 pairs within the
   `CRSCLTP[X]` suffix-byte family and found the true global maximum (91,090). B.28 varying a
   different byte position still gives only *one degree of freedom per sub-table* — it probes a
   different slice of the same single-permutation-per-seed paradigm, not a richer search space.

2. **One byte, different position, same ceiling problem.** The Fisher-Yates shuffle maps a
   64-bit seed to a single permutation of 261,121 cells. Whether bit 7 or bit 31 varies, the
   search space per axis is identically 256 candidates. There is no reason to expect the
   interior-byte family to escape a ceiling that the exhaustive suffix-byte search could not.

3. **Seeds 3+4 invariance is the diagnostic.** In B.26c, seeds 3 and 4 in the `CRSCLTP[X]`
   family were completely flat across all 36 tested values. This flatness is not about which byte
   position is varied — it reflects that additional uniform-511 sub-tables contribute zero
   marginal constraint discriminability once seeds 1+2 are optimised. B.28 would test a different
   bit position in the same LCG-seeded Fisher-Yates architecture and is expected to exhibit the
   same saturation behaviour.

4. **The hill-climber is the higher-leverage next step.** Directly optimising LTP cell
   assignments (rather than seeds) gives O(s⁴) ≈ 6.8 × 10¹⁰ degrees of freedom and can target
   row-concentration geometry explicitly (B.9.2). B.28's expected return does not justify
   diverting effort from that more principled approach.

The original proposal is preserved below for reference.

#### B.28.1 Motivation (Original Proposal)

B.26d and B.27 vary the trailing byte (byte 8, LSB, bits 7..0) of the 8-byte seed, holding the
first 7 bytes fixed as `CRSCLTP`. B.29 varies the leading byte (byte 1, MSB, bits 63..56).
Both probe the *extremes* of the 64-bit seed integer.

B.28 tests the **interior**. It fixes the first 4 bytes (`CRSC`) and the last 3 bytes (`LTP`),
and sweeps a single variable byte in position 5 — the byte that has always been fixed to `L`
in all prior seeds. The word `CRSCLTP` is split in the middle:

$$\underbrace{C R S C}_{4\text{ fixed}} \underbrace{[X]}_{1\text{ var}} \underbrace{L T P}_{3\text{ fixed}}$$

This tests whether the constraint that anchors the seed family is the four-letter prefix `CRSC`
(bytes 0–3), or the full seven-letter string `CRSCLTP`. It also probes bit positions 31..24 —
the center of the 64-bit integer — which neither B.26d/B.27 nor B.29 covers. Together with
B.27 (LSB) and B.29 (MSB), these three 1-byte sweeps bracket the 64-bit seed space at three
independent positions and are each tractable in under one hour per axis with 4 workers.

**Hypothesis.** The Fisher-Yates LCG shuffle is sensitive to all 64 bits of its seed. The
convention of fixing byte 5 to `L` (0x4C) may be silently suboptimal. Seeds of the form
`CRSC[X]LTP` where $X \neq \texttt{0x4C}$ may produce partition layouts with higher depth.

#### B.28.2 Seed Structure

| Role | Bytes | Bit positions | Fixed value |
|------|-------|---------------|-------------|
| High prefix (fixed) | bytes 0–3 | 63..32 | `CRSC` = `0x43525343` |
| Middle (variable) | byte 4 | 31..24 | `0x00`–`0xFF` |
| Low suffix (fixed) | bytes 5–7 | 23..0 | `LTP` = `0x4C5450` |

The 256 B.28 seeds per axis are:

$$s(X) = (\texttt{0x43525343} \ll 32) \mid (X \ll 24) \mid \texttt{0x004C5450}, \quad X \in \{0, 1, \ldots, 255\}$$

When $X = \texttt{0x4C}$ (`L`), the seed is `0x435253434C4C5450` — not a valid B.26d/B.27
seed (`CRSCL`**`L`**`TP`). The B.26d/B.27 `CRSCLTP?` seeds have byte 4 = `0x4C` and byte 5 =
`0x54` (`T`); in B.28 notation byte 5 is fixed to `T` already, so this is consistent.

The four seed families now systematically probe three bit zones of the 64-bit integer:

| Experiment | Seed form | Variable bit zone | Space/axis |
|-----------|-----------|-------------------|-----------|
| B.26c | `CRSCLTP` + alphanum | bits 7..0, restricted | 36 |
| B.26d / B.27 | `CRSCLTP` + any byte | bits 7..0 | 256 |
| **B.28** | **`CRSC[X]LTP`** | **bits 31..24** | **256** |
| B.29 | `[X]CRSCLTP` | bits 63..56 | 256 |

#### B.28.3 Coordinate-Descent Staging

**Per-axis sweep.** Fix all other seeds at their current best values; sweep all 256 $X$-byte
values for the target axis. Cost: $256 \times 23\,\text{s} \div 4\,\text{workers} \approx 26$
minutes per axis — identical to a B.27 or B.29 sweep.

**Convergence round.** Sweep seed5 (hold seed6 at default), freeze best seed5; sweep seed6,
freeze best seed6; repeat until no improvement. Typical convergence: 2–3 rounds, $\approx 2$
hours total.

**Cross-family restart.** After B.28 converges on both axes, re-run B.27 (LSB) and B.29 (MSB)
sweeps with seeds fixed at the B.28 winner to verify no other bit zone can improve further.
Repeat across families until no axis yields improvement.

#### B.28.4 Tooling

A new script `tools/b28_seed_search.py`, structured identically to `tools/b27_seed_search.py`
with one change in seed construction:

```python
_HIGH = 0x43525343  # "CRSC" (bytes 0–3)
_LOW  = 0x004C5450  # "\x00LTP" (bytes 5–7, zero-padded for OR)

def make_seed(x_byte: int) -> int:
    return (_HIGH << 32) | (x_byte << 24) | _LOW
```

```bash
# Sweep seed5 B.28 axis (hold seeds 1-4 at B.26c winner, seed6 at default):
python3 tools/b28_seed_search.py --worker 0 --workers 4 --target-seed 5

# Merge and rank after completion:
python3 tools/b28_merge_results.py --out-dir tools/b28_results --workers 4
```

A companion `tools/b28_merge_results.py` mirrors `tools/b27_merge_results.py`.

#### B.28.5 Expected Outcomes

**Optimistic.** A byte value other than `0x4C` (`L`) yields meaningfully higher depth for one
or more seeds. This would confirm that the `CRSCLTP` prefix is not optimal in full and that
the interior byte space offers genuine gains, motivating further 2-byte interior sweeps.

**Likely.** Depth is approximately flat across most of the 256 values, with `0x4C` (`L`) being
near-optimal — similar to the B.22 observation that seeds 3 and 4 were invariant within the
alphanumeric set. The plateau at 91,090 is insensitive to the interior byte.

**Pessimistic.** Depth degrades sharply for $X \neq \texttt{0x4C}$ (`L`), confirming that the
specific byte sequence `CRSCLTP` matters structurally and cannot be perturbed at byte 5.

#### B.28.6 Relationship to Other Proposals

*B.26d / B.27.* B.28 tests bit positions 31..24 (byte 5), whereas B.26d/B.27 test bit
positions 7..0 (byte 8). Together these two experiments probe opposite halves of the 64-bit
integer. A flat response in both zones would strongly indicate that depth is insensitive to
individual seed byte values within the `CRSC??LTP`-style family.

*B.29.* B.29 tests bit positions 63..56 (byte 1). Together B.28 and B.29 cover the interior
and the MSB, while B.26d/B.27 cover the LSB. All three are 256-candidate, sub-hour-per-axis
sweeps — a consistent methodology that avoids the infeasibility of 2-byte joint searches.

*B.22.* B.22 used greedy coordinate descent within the 36-element alphanumeric set. B.28
extends that strategy to all 256 byte values at an interior seed byte position, testing
coordinate-descent in a new bit zone at the same per-evaluation cost.

