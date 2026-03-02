/**
 * @file line_stat_audit.metal
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief Metal compute kernel for bulk line-stat recomputation and forced-assignment detection.
 *
 * One thread per constraint line (3064 total = 511 rows + 511 cols + 1021 diags + 1021 anti-diags).
 * Each thread iterates cells on its line, computes u(L), a(L), rho(L), and emits force proposals
 * when rho=0 or rho=u with u>0.
 */
#include <metal_stdlib>
using namespace metal;

/**
 * @struct GpuLineStat
 * @brief Per-line statistics (mirrors C++ GpuLineStat, 8 bytes).
 */
struct GpuLineStat {
    uint16_t unknown;
    uint16_t assigned;
    int16_t residual;
    uint16_t pad;
};

/**
 * @struct GpuForceProposal
 * @brief Forced-assignment proposal (mirrors C++ GpuForceProposal, 8 bytes).
 */
struct GpuForceProposal {
    uint16_t r;
    uint16_t c;
    uint8_t value;
    uint8_t pad0;
    uint8_t pad1;
    uint8_t pad2;
};

/**
 * @name getBit
 * @brief Extract a single bit from the row buffer.
 * @param rowBase Pointer to the start of the row (16 uint32 words).
 * @param col Column index (0..510).
 * @return 1 if the bit is set, 0 otherwise.
 */
static inline uint getBit(const device uint32_t* rowBase, uint col) {
    uint word = col / 32;
    uint bit = 31 - (col % 32);
    return (rowBase[word] >> bit) & 1;
}

/**
 * @name line_stat_audit
 * @brief Compute line statistics and emit force proposals for one constraint line.
 * @param assignmentBits Assignment bits: 511 rows x 16 uint32 (buffer 0, read-only).
 * @param knownBits Known bits: same layout (buffer 1, read-only).
 * @param lineStats Output line statistics (buffer 2, write-only).
 * @param targets Cross-sum target values per line (buffer 3, read-only).
 * @param proposalCount Atomic counter for proposals (buffer 4, read-write).
 * @param proposals Force proposals array (buffer 5, write-only).
 * @param constants Pointer to kS value (buffer 6, read-only).
 * @param tid Thread position in grid (one per line).
 */
kernel void line_stat_audit(
    const device uint32_t* assignmentBits [[buffer(0)]],
    const device uint32_t* knownBits      [[buffer(1)]],
    device GpuLineStat* lineStats          [[buffer(2)]],
    const device uint16_t* targets         [[buffer(3)]],
    device atomic_uint* proposalCount      [[buffer(4)]],
    device GpuForceProposal* proposals     [[buffer(5)]],
    const device uint16_t* constants       [[buffer(6)]],
    uint tid [[thread_position_in_grid]]
) {
    const uint s = constants[0]; // 511
    const uint numRows = s;
    const uint numCols = s;
    const uint numDiags = 2 * s - 1;    // 1021
    // const uint numAntiDiags = 2 * s - 1; // 1021
    const uint wordsPerRow = 16;
    const uint totalLines = numRows + numCols + numDiags + numDiags;

    if (tid >= totalLines) {
        return;
    }

    uint unknownCount = 0;
    uint assignedOnes = 0;

    // Determine line type and iterate cells
    if (tid < numRows) {
        // --- Row line ---
        uint r = tid;
        const device uint32_t* assignRow = assignmentBits + r * wordsPerRow;
        const device uint32_t* knownRow = knownBits + r * wordsPerRow;
        for (uint c = 0; c < s; ++c) {
            uint known = getBit(knownRow, c);
            if (known == 0) {
                unknownCount++;
            } else {
                assignedOnes += getBit(assignRow, c);
            }
        }
    } else if (tid < numRows + numCols) {
        // --- Column line ---
        uint c = tid - numRows;
        for (uint r = 0; r < s; ++r) {
            const device uint32_t* assignRow = assignmentBits + r * wordsPerRow;
            const device uint32_t* knownRow = knownBits + r * wordsPerRow;
            uint known = getBit(knownRow, c);
            if (known == 0) {
                unknownCount++;
            } else {
                assignedOnes += getBit(assignRow, c);
            }
        }
    } else if (tid < numRows + numCols + numDiags) {
        // --- Diagonal line: d = c - r + (s-1) ---
        uint d = tid - numRows - numCols;
        int offset = int(d) - int(s - 1);
        int rMin = max(0, -offset);
        int rMax = min(int(s) - 1, int(s) - 1 - offset);
        for (int ri = rMin; ri <= rMax; ++ri) {
            uint r = uint(ri);
            int ci = ri + offset;
            if (ci < 0 || ci >= int(s)) continue;
            uint c = uint(ci);
            const device uint32_t* assignRow = assignmentBits + r * wordsPerRow;
            const device uint32_t* knownRow = knownBits + r * wordsPerRow;
            uint known = getBit(knownRow, c);
            if (known == 0) {
                unknownCount++;
            } else {
                assignedOnes += getBit(assignRow, c);
            }
        }
    } else {
        // --- Anti-diagonal line: x = r + c ---
        uint x = tid - numRows - numCols - numDiags;
        int rMin = max(0, int(x) - int(s) + 1);
        int rMax = min(int(x), int(s) - 1);
        for (int ri = rMin; ri <= rMax; ++ri) {
            uint r = uint(ri);
            uint c = uint(int(x) - ri);
            const device uint32_t* assignRow = assignmentBits + r * wordsPerRow;
            const device uint32_t* knownRow = knownBits + r * wordsPerRow;
            uint known = getBit(knownRow, c);
            if (known == 0) {
                unknownCount++;
            } else {
                assignedOnes += getBit(assignRow, c);
            }
        }
    }

    int16_t target = int16_t(targets[tid]);
    int16_t residual = target - int16_t(assignedOnes);

    lineStats[tid].unknown = uint16_t(unknownCount);
    lineStats[tid].assigned = uint16_t(assignedOnes);
    lineStats[tid].residual = residual;
    lineStats[tid].pad = 0;

    // Emit force proposals when rho=0 or rho=u with u>0
    if (unknownCount == 0) {
        return;
    }

    uint8_t forceValue = 255; // sentinel: no forcing
    if (residual == 0) {
        forceValue = 0;
    } else if (uint(residual) == unknownCount) {
        forceValue = 1;
    }

    if (forceValue == 255) {
        return;
    }

    // Re-iterate cells to find unknowns and emit proposals
    if (tid < numRows) {
        uint r = tid;
        const device uint32_t* knownRow = knownBits + r * wordsPerRow;
        for (uint c = 0; c < s; ++c) {
            if (getBit(knownRow, c) == 0) {
                uint idx = atomic_fetch_add_explicit(proposalCount, 1, memory_order_relaxed);
                if (idx < s * s) {
                    proposals[idx].r = uint16_t(r);
                    proposals[idx].c = uint16_t(c);
                    proposals[idx].value = forceValue;
                }
            }
        }
    } else if (tid < numRows + numCols) {
        uint c = tid - numRows;
        for (uint r = 0; r < s; ++r) {
            const device uint32_t* knownRow = knownBits + r * wordsPerRow;
            if (getBit(knownRow, c) == 0) {
                uint idx = atomic_fetch_add_explicit(proposalCount, 1, memory_order_relaxed);
                if (idx < s * s) {
                    proposals[idx].r = uint16_t(r);
                    proposals[idx].c = uint16_t(c);
                    proposals[idx].value = forceValue;
                }
            }
        }
    } else if (tid < numRows + numCols + numDiags) {
        uint d = tid - numRows - numCols;
        int offset = int(d) - int(s - 1);
        int rMin = max(0, -offset);
        int rMax = min(int(s) - 1, int(s) - 1 - offset);
        for (int ri = rMin; ri <= rMax; ++ri) {
            uint r = uint(ri);
            int ci = ri + offset;
            if (ci < 0 || ci >= int(s)) continue;
            uint c = uint(ci);
            const device uint32_t* knownRow = knownBits + r * wordsPerRow;
            if (getBit(knownRow, c) == 0) {
                uint idx = atomic_fetch_add_explicit(proposalCount, 1, memory_order_relaxed);
                if (idx < s * s) {
                    proposals[idx].r = uint16_t(r);
                    proposals[idx].c = uint16_t(c);
                    proposals[idx].value = forceValue;
                }
            }
        }
    } else {
        uint x = tid - numRows - numCols - numDiags;
        int rMin = max(0, int(x) - int(s) + 1);
        int rMax = min(int(x), int(s) - 1);
        for (int ri = rMin; ri <= rMax; ++ri) {
            uint r = uint(ri);
            uint c = uint(int(x) - ri);
            const device uint32_t* knownRow = knownBits + r * wordsPerRow;
            if (getBit(knownRow, c) == 0) {
                uint idx = atomic_fetch_add_explicit(proposalCount, 1, memory_order_relaxed);
                if (idx < s * s) {
                    proposals[idx].r = uint16_t(r);
                    proposals[idx].c = uint16_t(c);
                    proposals[idx].value = forceValue;
                }
            }
        }
    }
}
