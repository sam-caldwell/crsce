<div>
    <table>
        <tr>
            <td style="min-width:200px"><img src="docs/img/logo.png" alt="CRSCE logo"/></td>
            <td>
                <h1>Decompression Algorithm Notes</h1>
                <p>
                  This document expands on practical reconstruction techniques for CRSCE decoding on CPU. It is 
                  solver‑agnostic but assumes common patterns described.
                </p>
            </td>
        </tr>
    </table>
</div>

## Factor graph view

- Variables: one binary variable per CSM cell, x[r,c] ∈ {0,1}.
- Factors: one constraint per line in each family (LSM, VSM, DSM, XSM). Each factor encodes an equality over the sum of
  its incident variables.
- The resulting graph is loopy; message‑passing requires damping and careful scheduling.

## Deterministic elimination (DE)

For each line, maintain two running quantities: residual R (target sum minus already‑assigned ones) and the number of
unassigned variables U on that line. Then:

- If R = 0, assign all remaining variables on that line to 0.
- If R = U, assign all remaining variables on that line to 1.
- Update residuals and repeat until a fixed point.

## Known‑hash folding

We know certain row-patterns are challenging for LBP/GOBP to solve, and that these are common. To reduce risk and
increase speed, we can:

- Maintain a small library of frequently occurring row patterns and their SHA‑256.
- If R = U, assign all remaining variables on that line to 1.
- Update residuals and repeat until a fixed point.

## Known‑hash folding

We know certain row-patterns are challenging for LBP/GOBP to solve, and that these are common. To reduce risk and
increase speed, we can:

- Maintain a small library of frequently occurring row patterns and their SHA‑256.
- When a row candidate’s structure matches a known pattern, assign bits directly and verify against the LH constraint.
- This is an engineering shortcut; it does not change acceptance criteria.

## Iterative inference (GOBP/LBP)

- Run loopy belief propagation (LBP) or a custom “Game of Beliefs Protocol” (GOBP) with damping on messages.
- Prioritize assignments for variables with strong bias.
- Re‑enter DE when new exact assignments become implied by updated residuals.

## LH‑based selection/oracle

- The LH chain is not for search; it is a cryptographic oracle to reject cross‑sum‑consistent but incorrect candidates.
- For any candidate CSM that satisfies all cross‑sum equalities, recompute the LH chain and compare. Only exact matches
  are accepted.

## Performance notes

- The over‑constrained nature (every cell participates in multiple factors) helps both DE and message passing.
- A conservative time model suggests ≈4.77 ms/block at s=511 on an 8‑core ~3.5 GHz CPU, or ≈6.8 MB/s throughput.
