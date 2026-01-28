# Custom Exceptions

This project defines focused, targetable exceptions under
`include/common/exceptions/` to replace generic standard exceptions.
Each exception carries enough context to craft an informative error message
and to selectively catch related failures.

| Exception | Namespace | Purpose | Key Context |
| --- | --- | --- | --- |
| `crsce::common::exceptions::CrsceException` | Common | Base type for all CRSCE-specific exceptions (derives from `std::runtime_error`). | Message only |
| `crsce::decompress::CsmIndexOutOfBounds` | Decompress | CSM API called with `r` or `c` outside `[0,kS)`. | `r`, `c`, `limit=kS` |
| `crsce::decompress::WriteFailureOnLockedCsmElement` | Decompress | Attempt to write a bit value into a locked CSM cell. | Message only |
| `crsce::decompress::DeterministicEliminationError` | Decompress | Deterministic Elimination detected a contradiction while applying a cell. | Message + cell/line details in text |
| `crsce::decompress::ConstraintBoundsInvalid` | Decompress | A `U_*` value exceeds `S` (invalid upper bound). | `family`, `index`, `value`, `S` |
| `crsce::decompress::ConstraintInvariantViolation` | Decompress | Invariant `R ≤ U` violated for a line. | `family`, `index`, `R`, `U` |
| `crsce::decompress::GobpResidualUnderflow` | Decompress | GOBP attempted an invalid assignment (U zero) or would underflow R when assigning 1. | `kind`, `r`, `c`, `d`, `x` |

Notes

- All exceptions derive (directly or indirectly) from `std::runtime_error`
  via `CrsceException` to simplify catching at the application boundary,
  while remaining specific within components.
- Where practical, exceptions expose getters for relevant numeric context to simplify diagnostics and testing.
