# Byte-Position Priors and Structural Regularities as a Decompression Heuristic for CRSCE

Sam Caldwell

---

## 1. Benford's Law as a Framework for Structural Priors

Benford's Law, first formalized by Newcomb (1881) and later independently articulated by Benford (1938), observes that in many naturally occurring numerical datasets, the leading significant digit is not uniformly distributed. Rather, smaller digits appear as the most significant digit with disproportionate frequency: the digit 1 leads approximately 30.1% of the time, while 9 leads in fewer than 5% of cases. Formally, the probability that the leading digit equals *d* is given by P(d) = log₁₀(1 + 1/d). This distribution emerges wherever data spans multiple orders of magnitude and arises from scale-invariant generating processes (Hill, 1995). In binary representations, the analogous observation is that for naturally occurring positive integers, the probability that the most significant bit (MSB) is 0 is greater than the probability that it is 1, since the preponderance of values in bounded natural distributions cluster below the midpoint of any given bit-width.

Benford's Law is not invoked here as a mechanism to be applied directly to CRSCE decompression. Rather, it is presented as a motivating framework: it demonstrates that real-world data, even when treated as a stream of bits, is not a uniform random process. Structure persists at the level of individual bit positions within bytes, and that structure is exploitable. The practical implication for CRSCE is that the Cross-Sum Matrix (CSM) — populated in deterministic row-major order from the source data — inherits the statistical regularities of its input, including systematic biases at specific intra-byte bit positions. These biases are invisible to the constraint system as currently specified, but they are recoverable and constitute a genuine prior over individual cell values that can meaningfully guide decompression.

## 2. Byte-Level Structure of the CSM

The CSM at dimension *s* = 127 is a 16,129-cell binary matrix populated row-major from the input data stream. Because the mapping is deterministic and byte-aligned, every cell at linear index *i* corresponds to bit position *i* mod 8 within its source byte, with bit position 7 being the MSB and bit position 0 the LSB. This alignment is not incidental — it is a fixed structural property of the CRSCE format that holds regardless of the source content.

The consequence is that the 16,129 cells of the CSM are not exchangeable with respect to source statistics. Cells at bit position 7 (MSB cells) systematically carry the most significant bit of their respective source bytes; cells at bit position 0 carry the LSB. For any source whose byte distribution is non-uniform — which characterizes virtually all practical data — these positions carry different expected values. The current CRSCE solver, including the combinator-algebraic Phase I fixpoint and the random-restart Phase II DFS, treats all undetermined cells as exchangeable during branching. This represents a discarded prior that the solver could in principle recover and exploit without modifying the compressed payload format at all.

## 3. The MSB Prior in Natural and Semi-Structured Data

For natural language data encoded in ASCII or UTF-8, the MSB prior is nearly deterministic: the ASCII standard (ANSI, 1986) assigns all printable characters to the range 0x20–0x7E, meaning the MSB of every standard ASCII byte is 0 with probability approaching 1. Even for UTF-8 multibyte sequences, the leading byte has a constrained MSB pattern dictated by the encoding specification (Yergeau, 2003). For binary file formats — including MPEG-4 containers, which constitute the primary test corpus in the CRSCE experimental series — field widths, version numbers, and codec identifiers populate large regions of the file with values well below 128, again biasing MSB cells toward 0 with measurable confidence.

More broadly, any source dataset derived from human-generated or algorithmically structured content will exhibit what Diaconis and Freedman (1979) characterize as concentration phenomena: the empirical distribution of byte values concentrates around a limited range rather than spreading uniformly across [0, 255]. This concentration is precisely what Benford's Law describes at the level of leading digits in decimal, and its binary analogue is the MSB bias under discussion here. For a 127×127 CSM populated from a typical MP4 file, the expected density of MSB cells below 0.5 implies a meaningful, exploitable deviation from the 0.5 prior that the solver currently assumes for all cells.

## 4. Structural Regularities in Encrypted Data

A common assumption is that encrypted data is indistinguishable from uniformly random bytes, rendering any structural prior inapplicable. This assumption is substantially correct for the ciphertext payload proper, but it is importantly incomplete for real encrypted files as they appear in practice. Encrypted data, as actually encountered by a compression system, is embedded within file formats that impose mandatory plaintext structure at deterministic byte offsets (Ferguson, Schneier, & Kohno, 2010).

Consider AES-CBC encryption as a representative case. The encrypted object is wrapped in a container — whether a TLS record, a PGP packet, an OpenSSL envelope, or an encrypted ZIP entry — that includes plaintext fields for the magic number, format version, cipher algorithm identifier, initialization vector or nonce, and payload length (Dierks & Rescorla, 2008). These fields appear at known offsets relative to the start of the encrypted object. Because the CSM block offset within the file is known at decompression time, the solver can compute which CSM cells correspond to which file-offset positions and assign priors accordingly: near-certain values for magic bytes and version fields, uniform priors for IV bytes, and constrained priors for algorithm identifiers drawn from a small known enumeration.

Of particular value is PKCS#7 padding (Kaliski, 1998). The final ciphertext block of a PKCS#7-padded message contains padding bytes whose values are entirely determined by the padding length: a single padding byte has value 0x01; two padding bytes each have value 0x02; and so forth. All PKCS#7 padding values from 0x01 through 0x0F have their upper nibble as zero, which means bits 7 through 4 of each padding byte are zero with certainty. Any CSM cells that map to these padding byte positions carry a prior of P(cell = 0) = 1.0 for four of their eight bit positions. This is not a probabilistic softening of the constraint system — it is a deterministic assignment that the solver can inject before Phase I even begins, potentially seeding cascade propagation through the GaussElim and IntBound combinators.

## 5. Application to CRSCE: A Byte-Position Prior Layer

The proposal is a pre-Phase I processing stage — a Byte-Position Prior (BPP) layer — that maps each of the 16,129 CSM cells to its source byte position and, if the file format is known or detectable, its semantic role. The BPP layer produces a per-cell confidence weight w(i) ∈ [0, 1] representing the prior probability that cell *i* equals 0. These weights are then consumed by the Phase II DFS in two ways.

First, cells with w(i) approaching 1.0 — MSB cells in ASCII regions, or cells in known-zero header fields — are assigned their prior value before DFS begins and submitted to IntBound propagation. If even a modest number of additional cells can be seeded this way, the effect on cascade depth is potentially disproportionate, because each seeded cell reduces unknown counts on multiple constraint lines simultaneously. The B.63e random restart results already demonstrate that DFS cascade from the combinator base has a 32-fold amplification factor: one cell assignment determines approximately 280,000 cell-states through IntBound propagation (Caldwell, 2026). A small number of high-confidence prior assignments could initiate cascades that the purely algebraic Phase I cannot.

Second, for cells with intermediate w(i) — MSB cells in binary data where the prior is 0.85 rather than 1.0 — the weight biases the branching order during Phase II DFS restarts. Rather than purely random cell orderings, the restart ensemble draws orderings that prioritize high-confidence-zero cells. This retains the macroscopic diversity that made random restarts succeed over most-constrained-first ordering in B.63e (Caldwell, 2026), while adding a structured gradient that steers restarts toward the region of the search space most consistent with the source statistics.

The critical architectural advantage is that the BPP layer requires no modification to the compressed payload format. The byte-to-CSM mapping is already deterministic from the format specification, and file format identification (if used) can be performed from the uncompressed file header before any CRSCE block is processed. The BPP layer is purely a decompressor-side heuristic that exploits information already implicit in the relationship between the CSM structure and the source data. This preserves CRSCE's compression ratio and its format specification while potentially reducing or eliminating the 583-cell structural residual on natural and semi-structured data.

The limitation is acknowledged directly: for maximally entropic data — true random byte streams with no format wrapper — the BPP layer contributes nothing, and the 583-cell residual persists. However, this limitation is already present in classical compression, which also fails on random data. CRSCE's theoretical novelty lies in its source-independent compression ratio, and the BPP layer does not disturb that property for the format's adversarial case. What it offers is a practical improvement for the overwhelming majority of real-world inputs, where byte-level structural regularities provide a genuine, exploitable prior over the cells that the algebraic constraint system cannot independently determine.

---

## References

ANSI. (1986). *American national standard for information systems — coded character sets — 7-bit American national standard code for information interchange (7-bit ASCII)* (ANSI X3.4-1986). American National Standards Institute.

Benford, F. (1938). The law of anomalous numbers. *Proceedings of the American Philosophical Society, 78*(4), 551–572.

Caldwell, S. (2026). *CRSCE decompression using combinators and constraint-based search*. Champlain College.

Diaconis, P., & Freedman, D. (1979). On rounding percentages. *Journal of the American Statistical Association, 74*(366), 359–364.

Dierks, T., & Rescorla, E. (2008). *The transport layer security (TLS) protocol version 1.2* (RFC 5246). Internet Engineering Task Force.

Ferguson, N., Schneier, B., & Kohno, T. (2010). *Cryptography engineering: Design principles and practical applications*. Wiley.

Hill, T. P. (1995). A statistical derivation of the significant-digit law. *Statistical Science, 10*(4), 354–363.

Kaliski, B. (1998). *PKCS #7: Cryptographic message syntax version 1.5* (RFC 2315). Internet Engineering Task Force.

Newcomb, S. (1881). Note on the frequency of use of the different digits in natural numbers. *American Journal of Mathematics, 4*(1), 39–40.

Yergeau, F. (2003). *UTF-8, a transformation format of ISO 10646* (RFC 3629). Internet Engineering Task Force.

