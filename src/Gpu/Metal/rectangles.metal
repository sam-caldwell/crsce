//
// rectangles.metal
// Compute ΔΦ (L1 error change) for DSM/XSM for a batch of 2x2 swap candidates.
// Inputs:
//  - buffer(0): dcnt[S] current diag counts (uint16)
//  - buffer(1): xcnt[S] current anti-diag counts (uint16)
//  - buffer(2): dsm[S]  target diag sums (uint16)
//  - buffer(3): xsm[S]  target anti-diag sums (uint16)
//  - buffer(4): candidates[N] packed: d[4] (ushort), x[4] (ushort), delta[4] (char)
//  - buffer(5): out_dphi[N] (int)
//  - buffer(6): params { uint S; uint N; }

#include <metal_stdlib>
using namespace metal;

struct Candidate {
    ushort d0, d1, d2, d3;
    ushort x0, x1, x2, x3;
    char   t0, t1, t2, t3; // deltas (+1 / -1)
    char   pad0;
};

struct Params { uint S; uint N; };

kernel void score_rectangles(
    device const ushort *      dcnt   [[ buffer(0) ]],
    device const ushort *      xcnt   [[ buffer(1) ]],
    device const ushort *      dsm    [[ buffer(2) ]],
    device const ushort *      xsm    [[ buffer(3) ]],
    device const Candidate *   cands  [[ buffer(4) ]],
    device int *               out    [[ buffer(5) ]],
    constant Params &          P      [[ buffer(6) ]],
    uint tid                          [[ thread_position_in_grid ]])
{
    if (tid >= P.N) return;
    const Candidate c = cands[tid];

    // Load current counts and targets for affected indices
    const ushort d_idx[4] = { c.d0, c.d1, c.d2, c.d3 };
    const ushort x_idx[4] = { c.x0, c.x1, c.x2, c.x3 };
    const short  t[4]     = { (short)c.t0, (short)c.t1, (short)c.t2, (short)c.t3 };

    // Compute before/after abs error for DSM/XSM over touched indices
    int before = 0;
    int after  = 0;

    // Deduplicate indices minimally by pairwise compare (at most 4 touched per family)
    // DSM
    ushort d_set[4];
    short  d_acc[4];
    ushort d_count = 0;
    for (ushort i = 0; i < 4; ++i) {
        const ushort idx = d_idx[i];
        bool found = false;
        for (ushort j = 0; j < d_count; ++j) {
            if (d_set[j] == idx) { d_acc[j] += t[i]; found = true; break; }
        }
        if (!found) { d_set[d_count] = idx; d_acc[d_count] = t[i]; d_count++; }
    }
    for (ushort j = 0; j < d_count; ++j) {
        const int cb = (int)dcnt[d_set[j]];
        const int tb = (int)dsm[d_set[j]];
        const int ca = cb + (int)d_acc[j];
        before += abs(cb - tb);
        after  += abs(ca - tb);
    }

    // XSM
    ushort x_set[4];
    short  x_acc[4];
    ushort x_count = 0;
    for (ushort i = 0; i < 4; ++i) {
        const ushort idx = x_idx[i];
        bool found = false;
        for (ushort j = 0; j < x_count; ++j) {
            if (x_set[j] == idx) { x_acc[j] += t[i]; found = true; break; }
        }
        if (!found) { x_set[x_count] = idx; x_acc[x_count] = t[i]; x_count++; }
    }
    for (ushort j = 0; j < x_count; ++j) {
        const int cb = (int)xcnt[x_set[j]];
        const int tb = (int)xsm[x_set[j]];
        const int ca = cb + (int)x_acc[j];
        before += abs(cb - tb);
        after  += abs(ca - tb);
    }

    out[tid] = (after - before);
}

