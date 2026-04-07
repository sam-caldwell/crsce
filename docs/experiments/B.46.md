## B.46 Variable-Length rLTP5/rLTP6 (Proposed)

### B.46.1 Motivation

B.27 established that LTP5 and LTP6 (uniform 511-cell partitions) are structurally inert: adding them to the 4+4 system produces zero depth improvement. B.45 demonstrated that variable-length LTP partitions ("rLTP") with line lengths following the triangular pattern (1, 2, ..., 511, ..., 2, 1) provide effective short-line forcing that matches or exceeds geometric diagonals.

B.46 combines these findings: replace the inert uniform LTP5/LTP6 with variable-length rLTP5/rLTP6. This modifies two existing but non-contributing constraint families to use the rLTP architecture, adding 2 $\times$ 1,021 = 2,042 lines (including 40 short lines of length $\leq$ 10) while removing 2 $\times$ 511 = 1,022 uniform lines that contribute nothing at the plateau.

**Hypothesis.** The short lines in rLTP5/rLTP6 will force additional cells in the early propagation cascade (as B.45a's correct-path simulation confirmed for rLTP partitions), and the resulting deeper propagation zone will push the solver past the 96,672 depth ceiling.

### B.46.2 Design

**Partition construction.** Each rLTP sub-table partitions all 261,121 cells into 1,021 lines with lengths matching the diagonal pattern:

$$\text{len}(k) = \min(k + 1, \; 511, \; 1021 - k) \quad \text{for } k \in \{0, \ldots, 1020\}$$

Cell-to-line assignment uses the same Fisher-Yates pool-chained shuffle as uniform LTP, but with variable bucket sizes instead of uniform 511.

**Constraint store impact.** The stats array currently allocates 511 entries each for LTP5 and LTP6. rLTP5/rLTP6 need 1,021 entries each. The total line count changes from $3{,}064 + 6 \times 511 = 6{,}130$ to $3{,}064 + 4 \times 511 + 2 \times 1{,}021 = 7{,}150$.

**Payload impact.** Uniform LTP5/LTP6 use 511 $\times$ 9 = 4,599 bits each. Variable-length rLTP5/rLTP6 use the same variable-width encoding as DSM/XSM: $\sum_{k=0}^{1020} \lceil\log_2(\text{len}(k) + 1)\rceil = 8{,}185$ bits each. The payload increases by $2 \times (8{,}185 - 4{,}599) = 7{,}172$ bits (897 bytes). New payload: 133,160 bits (16,645 bytes). Compression ratio: $133{,}160 / 261{,}121 = 51.0\%$ (from 48.2%).

### B.46.3 Implementation

Modifications to three files:

1. **`LtpTable.h` / `LtpTable.cpp`:** Modify `buildAllPartitions()` to construct rLTP5/rLTP6 with variable bucket sizes. Add `ltpLineLen5(k)` and `ltpLineLen6(k)` functions returning `min(k+1, 511, 1021-k)`. Update `LtpMembership` to map cells to 1021-line indices for sub-tables 5 and 6.

2. **`ConstraintStore.h` / `ConstraintStore_ctor.cpp`:** Expand `kTotalLines` to accommodate 2 $\times$ 1,021 entries for rLTP5/rLTP6 (replacing 2 $\times$ 511). Initialize stats with variable line lengths.

3. **`CompressedPayload` serialization:** Encode rLTP5/rLTP6 sums using variable-width bit packing (same as DSM/XSM) instead of uniform 9-bit packing. Update `kBlockPayloadBytes`.

### B.46.4 Implementation Status

The B.46 implementation encountered a **pool-chaining synchronization barrier**. The C++ LTP table construction uses a pool-chained Fisher-Yates shuffle (the pool state from sub-table 0's shuffle carries into sub-table 1's shuffle, etc.). Modifying sub-tables 5/6 to use variable-length buckets requires either:

(a) Modifying the pool-chained construction to handle mixed uniform/variable-length sub-tables, including the LTPB file loading path which validates uniform line lengths; or

(b) Using a standalone Python-generated sidecar, which produces different partition assignments due to the lack of pool chaining.

Option (a) requires extensive refactoring of `LtpTable.cpp`, `buildFromAssignment()`, and the LTPB file format. Option (b) produces inconsistent partition assignments between the compressor and decompressor.

### B.46.5 Conclusion (Blocked; Superseded by B.45)

B.46 is blocked by the pool-chaining infrastructure. However, the B.45 augmented experiment (4+4 base + 8 uniform + 3 variable-length LTP sidecar) already tested the rLTP hypothesis with the correct partition assignments. That experiment found **100,019 rLTP forcings but depth unchanged** (96,673 vs 96,689 baseline). Since B.45's augmented configuration includes strictly MORE rLTP constraint information than B.46 (11 additional families vs 2 replacement families), B.45's null depth result implies B.46 would also produce no depth improvement.

**B.46 status: BLOCKED (pool-chaining barrier). Superseded by B.45 augmented result (depth unchanged despite 100K rLTP forcings).**

---

