### B.26 Joint Seed Search for LTP Sub-Tables (Implemented)

#### B.26.1 Motivation

B.22 established that LTP partition seeds control a 2.7% depth variance (~2,380 cells) and
proposed a greedy sequential (coordinate-descent) search over a 36-element candidate set per
sub-table. The B.22 search optimized each seed $s_i$ independently in four sequential phases,
fixing the other three to their current best or default values. This strategy reduced the search
cost from $36^4 = 1{,}679{,}616$ evaluations to $4 \times 36 = 144$ evaluations, but it assumed
that sub-table quality contributions are approximately separable.

Separability may fail when two LTP sub-tables interact through constraint propagation cascades.
A cell $(r, c)$ belongs to one line in $T_0$ and one line in $T_1$. When the solver assigns a
value to $(r, c)$, both lines' residuals are updated simultaneously. If $T_0$-line $\ell_0$ and
$T_1$-line $\ell_1$ share many cells in the plateau band (rows 100--300), a forcing event on
$\ell_0$ may immediately push $\ell_1$ toward its own forcing threshold, producing a two-step
cascade that neither sub-table would achieve with arbitrary partners.

In optimization theory, coordinate descent over a discrete, non-differentiable objective provides
no convergence guarantee to the global optimum (Papadimitriou & Steiglitz, 1998). The DFS depth
metric $D(\sigma_1, \sigma_2, \sigma_3, \sigma_4)$ is a highly non-smooth function of the seed
tuple with no known gradient or Lipschitz structure. B.26 replaces coordinate descent with
exhaustive joint search in the two most sensitive seed dimensions.

#### B.26.2 Objective

Determine whether the B.22 greedy sequential seed optimum is also the joint optimum for
sub-tables $T_0$ and $T_1$, and if not, identify the globally optimal $(s_1, s_2)$ pair within
the candidate set. The search proceeds in two campaigns of increasing scope:

- **B.26c:** Exhaustive joint search over $36 \times 36 = 1{,}296$ pairs drawn from the
  original B.22 alphanumeric candidate set (ASCII `0`--`9`, `A`--`Z`), holding $s_3$ and $s_4$
  fixed.

- **B.26d:** Exhaustive joint search over $256 \times 256 = 65{,}536$ pairs drawn from the
  full single-byte range (`0x00`--`0xFF`), parallelized across multiple CPU cores. B.26d
  subsumes B.26c and explores the complete suffix-byte seed space.

Secondary objective: characterize the two-dimensional landscape $D(s_1, s_2)$ to determine
whether the objective is smooth (amenable to local search) or rugged (requiring exhaustive
enumeration).

#### B.26.3 Fixed Parameters

Both campaigns fix sub-table seeds $s_3 = \texttt{CRSCLTP3}$ and $s_4 = \texttt{CRSCLTP4}$ at
their B.22 Phase 3/Phase 4 values. This decision is justified by an empirical observation from
the B.22 sequential search: all 36 candidates for both Phase 3 and Phase 4 achieved identical
peak depths of 89,331, indicating that the solver's depth ceiling is insensitive to $s_3$ and
$s_4$ at the current operating point. The search therefore computes:

$$
D^*(s_1, s_2) = \max_{(s_1, s_2) \in \mathcal{S} \times \mathcal{S}} D(s_1, s_2, \sigma_3^{\text{fixed}}, \sigma_4^{\text{fixed}})
$$

where $\mathcal{S}$ is the candidate set (36 elements for B.26c, 256 for B.26d) and $D(\cdot)$
is the peak depth achieved in a fixed-duration decompression run.

#### B.26.4 Methodology

**Re-compression requirement.** Each candidate seed pair requires re-compression of the source
file before decompression can be evaluated. The compressed `.crsce` file contains LTP cross-sum
vectors computed from the original matrix under a specific set of seeds. If the decompressor uses
different seeds (hence a different cell-to-line mapping), the stored sums are inconsistent with
the partition the decompressor constructs. Without re-compression, depth measurements are invalid.

The implementation proceeds as follows for each candidate pair $(s_1, s_2)$:

```python
# Step 1: Compress with candidate seeds (DI discovery disabled for speed)
compress_env["CRSCE_LTP_SEED_1"] = str(s1)
compress_env["CRSCE_LTP_SEED_2"] = str(s2)
compress_env["CRSCE_LTP_SEED_3"] = str(SEED3_FIXED)
compress_env["CRSCE_LTP_SEED_4"] = str(SEED4_FIXED)
compress_env["DISABLE_COMPRESS_DI"] = "1"
subprocess.run([compress_bin, "-in", source_video, "-out", crsce_path], env=compress_env)

# Step 2: Decompress with same seeds for `secs` seconds, then SIGINT
proc = subprocess.Popen([decompress_bin, "-in", crsce_path, "-out", out_path], env=env)
time.sleep(secs)
proc.send_signal(signal.SIGINT)   # graceful flush of telemetry
proc.wait(timeout=5)              # escalate to SIGKILL on timeout
```

**Fitness metric.** Peak DFS depth recorded in the decompressor's JSON-L telemetry log. The
decompressor writes structured events to `CRSCE_EVENTS_PATH`; each event includes `tags.depth`.
The script parses the log and extracts `max(depth)` across all events.

**Progressive output.** Results are written to a JSON file after each pair evaluation.
The `--resume` flag reloads prior results and skips completed pairs, making the search idempotent
with respect to interruption.

#### B.26.5 B.26c: Alphanumeric Joint Search

**Search space.** The 36-element candidate set from B.22: ASCII characters `0`--`9` and `A`--`Z`
appended to the 7-byte prefix `CRSCLTP`, yielding 8-byte (64-bit) seeds
$\sigma = \texttt{0x43\_52\_53\_43\_4C\_54\_50\_}xx$ where $xx \in \{\texttt{0x30}\text{--}\texttt{0x39}, \texttt{0x41}\text{--}\texttt{0x5A}\}$.
Total: $36^2 = 1{,}296$ pairs.

**Runtime.** Sequential execution on Apple Silicon, 20s decompression + ~3--5s re-compression
per pair, totaling approximately 9 hours.

**Implementation.** `tools/b26c_joint_seed_search.py`. Extends B.22's single-axis scan to a
two-dimensional exhaustive sweep. The `run_candidate()` function implements the re-compress +
timed-decompress + telemetry-parse pipeline described in B.26.4.

#### B.26.6 B.26c Experimental Results

The joint search completed all 1,296 candidate pairs.

**Winner:** $(\sigma_1, \sigma_2) = (\texttt{CRSCLTPV}, \texttt{CRSCLTPP})$, peak DFS depth
**91,090**. This surpasses the B.22 greedy sequential baseline of 89,331 by **1,759 cells**
(+1.97%).

**Depth landscape:**

| Metric                    | Value            |
|:--------------------------|:-----------------|
| Total pairs evaluated     | 1,296            |
| Mean depth                | 86,941           |
| Minimum depth             | 84,265           |
| Maximum depth (winner)    | 91,090           |
| Pairs $\geq$ 89,331 (B.22 baseline) | 29 (2.2%) |
| Pairs $\geq$ 90,000      | 7 (0.54%)        |
| Pairs $\geq$ 91,000      | 1 (0.08%)        |

**Top-10 pairs:**

| Rank | Seed 1   | Seed 2   | Depth  |
|:----:|:---------|:---------|:------:|
| 1    | CRSCLTPV | CRSCLTPP | 91,090 |
| 2    | CRSCLTPH | CRSCLTPR | 90,826 |
| 3    | CRSCLTPG | CRSCLTPK | 90,244 |
| 4    | CRSCLTPX | CRSCLTPB | 90,139 |
| 5    | CRSCLTPZ | CRSCLTPZ | 90,031 |
| 6    | CRSCLTPP | CRSCLTPQ | 90,027 |
| 7    | CRSCLTPJ | CRSCLTP9 | 90,013 |
| 8    | CRSCLTPB | CRSCLTPA | 89,875 |
| 9    | CRSCLTPV | CRSCLTPY | 89,874 |
| 10   | CRSCLTPZ | CRSCLTPC | 89,853 |

The B.22 greedy sequential pair (CRSCLTP0, CRSCLTPG) scored 89,331 and ranks 29th of 1,296.

**Confirmed interaction effects.** The results definitively refute the separability assumption:

1. The greedy sequential pair is *not* the joint optimum. It ranks 29th overall.

2. The winning seed1 value (`CRSCLTPV`, suffix `0x56`) was not the greedy Phase-1 winner
   (`CRSCLTP0`). `CRSCLTPV` is synergistic with `CRSCLTPP`—a combination invisible to the
   greedy search, which conditioned Phase 2 on the Phase-1 winner.

3. The landscape is rugged: the coefficient of variation is ~1.4%, but the range spans 6,825
   cells (84,265--91,090), a 7.6% spread relative to the mean. Small seed changes produce large
   depth swings, precluding gradient-based or local search strategies.

**Deployment.** Seeds deployed in `src/Decompress/Solvers/LtpTable.cpp`:

```cpp
constexpr std::uint64_t kSeed1 = 0x4352'5343'4C54'5056ULL;  // CRSCLTPV
constexpr std::uint64_t kSeed2 = 0x4352'5343'4C54'5050ULL;  // CRSCLTPP
```

End-to-end validation via the 30-minute `uselessMachine` test confirmed depth 91,090.

#### B.26.7 Lessons from Hill-Climbing (Abandoned Side-Experiment)

An attempt to push depth beyond 91,090 by direct hill-climbing of LTP tables (swapping
cell-to-line assignments within a sub-table to maximize a local $\Phi$ metric) was conducted
and immediately abandoned. The procedure plateaued at depth $< 500$—a catastrophic regression.

**Root cause.** The Fisher-Yates shuffle produces partitions with global statistical properties
(near-uniform pairwise cell co-occurrence across lines, low maximum cross-line overlap,
well-distributed spatial coverage) that are critical for effective constraint propagation. Local
swaps destroy these global invariants: a swap that improves $\Phi$ on one line degrades spatial
coverage elsewhere, and the cascade of corrections required to restore balance exceeds the
benefit. The Fisher-Yates construction, parameterized only by the LCG seed, is a strong global
prior that cannot be replicated by greedy local perturbation.

This result confirms that seed search (varying the input to a structurally sound construction
algorithm) is the correct optimization strategy, not direct manipulation of the output partition.

#### B.26.8 B.26d: Full-Byte Joint Search (In Progress)

The B.26c result demonstrated that the 36-element alphanumeric candidate set contains a pair
significantly better than the greedy optimum. The natural follow-on question is whether
expanding the candidate set to the full single-byte range (`0x00`--`0xFF`, 256 candidates)
reveals even stronger pairs in the unexplored 220/256 = 85.9% of the byte space.

**Search space.** $256 \times 256 = 65{,}536$ pairs—a $50.6\times$ expansion over B.26c.

**Parallelization.** At 23 seconds per pair, sequential execution would require approximately
17.5 days. The B.26d script partitions the search across $N$ workers by dividing the seed1
axis into $N$ contiguous slices. Each worker independently sweeps all 256 seed2 values for its
assigned seed1 range. For $N = 4$ workers:

| Worker | seed1 range       | Pairs   | Est. hours |
|:------:|:------------------|:-------:|:----------:|
| 0      | `0x00`--`0x3F`    | 16,384  | ~105       |
| 1      | `0x40`--`0x7F`    | 16,384  | ~105       |
| 2      | `0x80`--`0xBF`    | 16,384  | ~105       |
| 3      | `0xC0`--`0xFF`    | 16,384  | ~105       |

Wall time: approximately **4.3 days** with 4 cores.

**Worker isolation.** Each worker writes to a separate results file
(`b26d_results_w{N}.json`) and uses a separate temporary directory for compressed/decompressed
files. Workers share no mutable state and can be launched, stopped, and resumed independently.
The `--resume` behavior from B.26c is preserved per-worker.

**Merge.** After all workers complete, `tools/b26d_merge_results.py` reads all per-worker JSON
files, deduplicates by seed pair, computes descriptive statistics (mean, median, standard
deviation, percentiles, baseline-beating count), ranks all pairs by depth, and writes a unified
`b26d_merged.json`.

**Implementation.** `tools/b26d_full_byte_joint_search.py`. Extends B.26c with:

- `--worker W --workers N` for single-worker execution on a partition of the seed1 axis
- `--launch-all` to spawn all $N$ workers as background processes with per-worker log files
- `partition_seed1_range(worker, workers)` for balanced slice assignment (distributes remainder
  across the first $R$ workers when $256 \bmod N \neq 0$)
- `seed_to_label()` extended to handle non-printable suffix bytes via `\xHH` notation

The `run_candidate()` function is identical to B.26c: re-compress → timed decompress → SIGINT →
telemetry parse.

#### B.26.9 Expected Outcomes for B.26d

**Optimistic case.** The full byte range contains a pair with depth $\geq 92{,}000$, a further
$+1.0\%$ improvement over B.26c. This would indicate that the 36-element alphanumeric set was a
substantial restriction and that the depth function has exploitable structure in the non-printable
byte regions.

**Likely case.** The B.26c winner (CRSCLTPV + CRSCLTPP, depth 91,090) remains the global
optimum or is surpassed by fewer than 500 cells. The additional 64,240 pairs confirm the
landscape statistics from B.26c at higher resolution and establish the byte-space depth
distribution with high confidence. The 256-axis marginal distributions may reveal whether seed
quality correlates with any byte-level property (e.g., Hamming weight, parity) that could
guide future multi-dimensional searches.

**Pessimistic case.** No pair exceeds the B.26c winner. The 91,090-depth plateau represents a
structural ceiling for two-seed joint optimization under the uniform-511 architecture with fixed
$s_3$ and $s_4$. Future improvement would require either a four-dimensional joint search or
architectural changes.

#### B.26.10 Relationship to Other Proposals

*B.22 (greedy seed search).* B.26c is a direct verification of B.22's greedy methodology. The
1,759-cell improvement proves that coordinate descent was insufficient and that joint search is
necessary in the $(s_1, s_2)$ subspace.

*B.23 (clipped-triangular LTP).* B.23 was abandoned due to coverage collapse (Section B.23.9).
B.26 operates entirely within the proven uniform-511 architecture, confirming that meaningful
depth gains are available without length-distribution changes.

*B.20 (LTP substitution baseline).* B.20 achieved depth 88,503 with its original seeds. B.26c's
winner at 91,090 exceeds the B.20 baseline by 2,587 cells (+2.9%), recovering and surpassing
the 2,380-cell regression that motivated B.22.

*B.8 (adaptive lookahead).* Deeper lookahead produced zero improvement in the forcing dead zone.
B.26 addresses the dead zone by modifying which lines are co-resident in the plateau band
(via seed selection), rather than applying deeper probing to the existing topology. The two
strategies are complementary.

#### B.26.11 Open Questions

(a) Does the B.26c winner generalize across input blocks? The depth test uses a single
representative file (`useless-machine.mp4`). Validation on diverse input patterns (all-zeros,
all-ones, random, alternating) is required before concluding that the seed pair is universally
optimal.

(b) Would a four-dimensional joint search over $(s_1, s_2, s_3, s_4)$ improve further? The
B.22 observation of flat depth across $s_3$ and $s_4$ was made at the greedy-optimal
$(s_1, s_2)$ operating point. The new B.26c operating point may have shifted the sensitivity
landscape, making $s_3$ and $s_4$ no longer inert. However, a full $256^4$ search is
computationally infeasible ($4.3 \times 10^9$ evaluations), and even $36^4 = 1.7 \times 10^6$
evaluations at 23 seconds each would require approximately 1.2 years of compute. Targeted
verification (re-run B.22 Phases 3 and 4 with the new $(s_1, s_2)$ winners) is the practical
next step.

(c) Is there a structural proxy for seed quality that avoids the re-compression cost? The
$\Phi$ metric from B.22.4 evaluates cross-family line overlap in the plateau band and
requires only the static partition layout and a precomputed estimate of tight lines. If $\Phi$
correlates strongly with depth (Spearman $\rho \geq 0.7$ over the 1,296 B.26c results), it
could replace the full re-compress/decompress cycle as a pre-filter, reducing the per-candidate
cost from ~23 seconds to ~1 millisecond and enabling search over vastly larger seed spaces.

(d) What is the information content of seed2 given seed1? The landscape data from B.26c (and
eventually B.26d) provides a full $|\mathcal{S}| \times |\mathcal{S}|$ matrix of depth values.
Computing the mutual information $I(D; s_2 \mid s_1)$ would quantify how much depth variance
is attributable to seed2 conditional on seed1, informing whether future searches should
prioritize seed1 range expansion or seed2 range expansion.

(e) Can the parallel search infrastructure from B.26d be extended to a continuous background
optimization daemon? If the CRSCE toolchain supported a `--background-optimize` mode that
evaluated seed pairs during idle time and updated the production seed set when improvements
are found, the depth ceiling would continuously improve as a function of accumulated compute
without manual intervention.

