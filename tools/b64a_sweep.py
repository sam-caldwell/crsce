#!/usr/bin/env python3
"""B.64a: Joint S-dimension and CRC-width parameter space sweep.

Computes payload, C_r, and estimated equations for all combinations.
Filters to C_r < 80%. Outputs the results table.
"""

import math
import json

def ceil_log2(n):
    """ceil(log2(n))"""
    if n <= 1: return 0
    return math.ceil(math.log2(n))

def B_d(s):
    """Variable-width diagonal sum cost in bits."""
    total = ceil_log2(s + 1)
    for l in range(1, s):
        total += 2 * ceil_log2(l + 1)
    return total

def diag_lengths(s):
    """Return list of (diag_index, length) for non-toroidal diagonals, sorted by length."""
    diags = []
    for d in range(2 * s - 1):
        l = min(d + 1, s, 2 * s - 1 - d)
        diags.append((d, l))
    diags.sort(key=lambda x: x[1])
    return diags

def graduated_crc_bits(length, max_tier_width):
    """Return CRC width for a line of given length under graduated scheme."""
    if length <= 8: return 8
    if length <= 16: return 16
    if length <= 32: return 32
    if length <= 64: return min(64, max_tier_width)
    if length <= 128: return min(128, max_tier_width)
    return min(256, max_tier_width)

def dh_xh_cost(s, n_diags, max_tier_width):
    """Cost in bits for DH or XH on n_diags shortest diagonals."""
    dl = diag_lengths(s)
    cost = 0
    eqs = 0
    for i in range(min(n_diags, len(dl))):
        _, l = dl[i]
        w = graduated_crc_bits(l, max_tier_width)
        cost += w
        eqs += w
    return cost, eqs

def rltp_cost(n_tables, max_line, max_tier_width):
    """Cost in bits for rLTP sub-tables."""
    cost_per = 0
    eqs_per = 0
    for l in range(1, max_line + 1):
        w = graduated_crc_bits(l, max_tier_width)
        cost_per += w
        eqs_per += w
    return n_tables * cost_per, n_tables * eqs_per

configs = []

# Parameter grid
S_values = [191, 255, 383, 511]
LH_widths = [0, 16, 32, 64, 128]
VH_widths = [0, 16, 32, 64, 128]
DH_coverages = [32, 64, 128]  # number of shortest diagonals
DH_max_tiers = [32, 64, 128]
rLTP_configs = [(0, 0, 32), (1, 16, 32), (1, 16, 64), (4, 32, 64), (8, 64, 64)]
# (n_tables, max_line, max_tier_width)

config_id = 0
for s in S_values:
    block_bits = s * s
    b = ceil_log2(s + 1)
    lsm = s * b
    vsm = s * b
    dsm = B_d(s)
    bh_di = 264

    for lh_w in LH_widths:
        lh_bits = s * lh_w
        lh_eqs = s * lh_w

        for vh_w in VH_widths:
            vh_bits = s * vh_w
            vh_eqs = s * vh_w

            for n_diags in DH_coverages:
                for dh_tier in DH_max_tiers:
                    dh_bits, dh_eqs = dh_xh_cost(s, n_diags, dh_tier)
                    xh_bits, xh_eqs = dh_xh_cost(s, n_diags, dh_tier)

                    for n_rltp, rltp_lines, rltp_tier in rLTP_configs:
                        rltp_bits, rltp_eqs = rltp_cost(n_rltp, rltp_lines, rltp_tier)

                        total = lsm + dsm + lh_bits + vh_bits + dh_bits + xh_bits + rltp_bits + bh_di
                        # Note: VSM and XSM dropped (redundant per B.62j)
                        cr = total / block_bits

                        if cr >= 0.80:
                            continue

                        total_eqs = lh_eqs + vh_eqs + dh_eqs + xh_eqs + rltp_eqs
                        # Add parity equations from DSM + LSM (1 per line)
                        total_eqs += s + (2 * s - 1)  # LSM parity + DSM parity

                        # Estimate rank using dependency ratio from empirical data
                        # B.63: 12,800 eqs → 9,585 rank (0.749)
                        # B.63n: 30,424 eqs → 13,489 rank (0.443)
                        # Dependency increases with more equations. Use conservative 0.50.
                        est_rank = int(total_eqs * 0.50)
                        est_rank = min(est_rank, block_bits)
                        null_space = block_bits - est_rank

                        config_id += 1
                        configs.append({
                            "id": f"A{config_id}",
                            "S": s,
                            "LH_W": lh_w,
                            "VH_W": vh_w,
                            "DH_diags": n_diags,
                            "DH_tier": dh_tier,
                            "rLTP": f"{n_rltp}x{rltp_lines}" if n_rltp > 0 else "none",
                            "payload": total,
                            "Cr": round(cr * 100, 1),
                            "equations": total_eqs,
                            "est_rank": est_rank,
                            "null_space": null_space,
                            "block_bits": block_bits
                        })

# Sort by null space ascending
configs.sort(key=lambda c: (c["null_space"], c["Cr"]))

# Print top 50
print(f"Total configs with C_r < 80%: {len(configs)}")
print()
print(f"{'ID':<8} {'S':>4} {'LH':>4} {'VH':>4} {'DH_d':>5} {'DH_t':>5} {'rLTP':<8} {'Payload':>8} {'Cr%':>6} {'Eqs':>8} {'Rank':>8} {'NS':>8}")
print("-" * 95)
for c in configs[:50]:
    print(f"{c['id']:<8} {c['S']:>4} {c['LH_W']:>4} {c['VH_W']:>4} {c['DH_diags']:>5} {c['DH_tier']:>5} {c['rLTP']:<8} {c['payload']:>8} {c['Cr']:>6.1f} {c['equations']:>8} {c['est_rank']:>8} {c['null_space']:>8}")

# Save full results
with open("tools/b64a_configs.json", "w") as f:
    json.dump(configs[:200], f, indent=1)

print(f"\nTop 200 configs saved to tools/b64a_configs.json")
print(f"\nBest config: {configs[0]['id']} S={configs[0]['S']} LH={configs[0]['LH_W']} VH={configs[0]['VH_W']} Cr={configs[0]['Cr']}% null_space={configs[0]['null_space']}")
