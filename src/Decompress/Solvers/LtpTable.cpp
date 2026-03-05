/**
 * @file LtpTable.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief LTP1–LTP4 joint-tiled variable-length partition implementation (B.21).
 *
 * Builds four joint-tiled sub-tables: each sub-table has 511 lines of length
 * ltp_len(k) = min(k+1, 511-k). Together the four sub-tables cover all 261,121
 * cells with each cell in 1-2 sub-tables (1,023 corner cells in 2).
 *
 * Construction: for each pass k=0..3, build a pool of unassigned cells, sort by
 * spatial score ascending (low = corner/edge), then assign cells to lines in
 * decreasing-length order so the longest lines capture the corner cells.
 *
 * Tables are computed once on first access and shared via function-local statics.
 */
#include "decompress/Solvers/LtpTable.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace crsce::decompress::solvers {

namespace {

    /**
     * @name kN
     * @brief Total number of cells in the matrix (511 * 511 = 261,121).
     */
    constexpr std::uint32_t kN = static_cast<std::uint32_t>(kLtpS) * kLtpS;

    /**
     * @name kTotalPerSubtable
     * @brief Total cells per sub-table: sum_{k=0}^{510} ltp_len(k) = 65,536.
     */
    constexpr std::uint32_t kTotalPerSubtable = 65536U;

    /**
     * @name kSeed1
     * @brief LCG seed for pass 0 ("CRSCLTP1").
     */
    constexpr std::uint64_t kSeed1 = 0x4352'5343'4C54'5031ULL;

    /**
     * @name kSeed2
     * @brief LCG seed for pass 1 ("CRSCLTP2").
     */
    constexpr std::uint64_t kSeed2 = 0x4352'5343'4C54'5032ULL;

    /**
     * @name kSeed3
     * @brief LCG seed for pass 2 ("CRSCLTP3").
     */
    constexpr std::uint64_t kSeed3 = 0x4352'5343'4C54'5033ULL;

    /**
     * @name kSeed4
     * @brief LCG seed for pass 3 ("CRSCLTP4").
     */
    constexpr std::uint64_t kSeed4 = 0x4352'5343'4C54'5034ULL;

    /**
     * @name kLcgA
     * @brief LCG multiplier (Knuth).
     */
    constexpr std::uint64_t kLcgA = 6364136223846793005ULL;

    /**
     * @name kLcgC
     * @brief LCG additive constant (Knuth).
     */
    constexpr std::uint64_t kLcgC = 1442695040888963407ULL;

    /**
     * @struct LtpData
     * @name LtpData
     * @brief Shared storage for all four LTP sub-tables: forward and CSR reverse tables.
     */
    struct LtpData {
        /**
         * @name fwd
         * @brief Forward table: fwd[flat] = LtpMembership (1 or 2 sub-table entries).
         */
        std::vector<LtpMembership> fwd;

        /**
         * @name csrOffsets
         * @brief CSR prefix-sum offsets: csrOffsets[sub][k] = start index in csrCells[sub].
         * csrOffsets[sub][511] = kTotalPerSubtable (sentinel).
         */
        std::array<std::array<std::uint32_t, 512>, 4> csrOffsets{};

        /**
         * @name csrCells
         * @brief CSR cell storage: csrCells[sub] has kTotalPerSubtable LtpCell entries.
         */
        std::array<std::vector<LtpCell>, 4> csrCells;
    };

    /**
     * @name spatialScore
     * @brief Compute spatial score for cell (r, c): sum of triangular diag + anti-diag heights.
     *
     * Low score = corner/edge (weak diagonal coverage).
     * High score = center (strong diagonal coverage).
     *
     * @param r Row index.
     * @param c Column index.
     * @return Spatial score in [512, 1022].
     */
    [[nodiscard]] std::uint32_t spatialScore(const std::uint16_t r, const std::uint16_t c) {
        constexpr std::uint32_t s = kLtpS;
        const std::uint32_t d = static_cast<std::uint32_t>(c) + (s - 1U) - r; // c - r + (s-1)
        const std::uint32_t x = static_cast<std::uint32_t>(r) + c;
        const std::uint32_t diag2sm1 = (2U * s) - 1U; // 1021
        const std::uint32_t diagTri = std::min(d + 1U, diag2sm1 - d);
        const std::uint32_t antiTri = std::min(x + 1U, diag2sm1 - x);
        return diagTri + antiTri;
    }

    /**
     * @name buildCsrOffsets
     * @brief Precompute CSR prefix-sum offsets from ltp_len values.
     * @param offsets Output: offsets[0..511] where offsets[511] = kTotalPerSubtable.
     */
    void buildCsrOffsets(std::array<std::uint32_t, 512> &offsets) {
        offsets[0] = 0;
        for (std::uint16_t k = 0; k < kLtpNumLines; ++k) {
            offsets[k + 1] = offsets[k] + ltpLineLen(k); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        }
        // offsets[511] == kTotalPerSubtable == 65536
    }

    /**
     * @name buildAllPartitions
     * @brief Build all four B.21 joint-tiled LTP partitions and populate LtpData.
     *
     * Four passes:
     *   Pass 0-2 (threshold=1): each cell assigned to at most 1 sub-table.
     *   Pass 3   (threshold=2): corner cells re-used (count goes 1 → 2).
     *
     * Within each pass:
     *   1. Build pool of cells with count < threshold, sorted by spatial score ASC
     *      (ties broken by LCG shuffle with per-pass seed).
     *   2. Process lines in decreasing ltp_len order, assigning cells from pool front.
     *      Long lines get corner cells; short lines get center cells.
     *
     * @return Fully initialized LtpData.
     */
    LtpData buildAllPartitions() {
        LtpData data;
        data.fwd.resize(kN);
        for (std::size_t sub = 0; sub < 4; ++sub) {
            data.csrCells[sub].resize(kTotalPerSubtable); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        }

        // All 4 sub-tables share the same CSR offset structure
        std::array<std::uint32_t, 512> sharedOffsets{};
        buildCsrOffsets(sharedOffsets);
        for (std::size_t sub = 0; sub < 4; ++sub) {
            data.csrOffsets[sub] = sharedOffsets; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        }

        // Line processing order: decreasing ltp_len (longest first)
        std::array<std::uint16_t, kLtpNumLines> sortedLines{};
        { std::uint16_t v = 0; for (auto &e : sortedLines) { e = v++; } }
        std::ranges::stable_sort(sortedLines,
            [](const std::uint16_t a, const std::uint16_t b) {
                return ltpLineLen(a) > ltpLineLen(b);
            });

        // Precompute spatial scores for all cells
        std::vector<std::uint32_t> scores(kN);
        for (std::uint32_t flat = 0; flat < kN; ++flat) {
            scores[flat] = spatialScore( // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                static_cast<std::uint16_t>(flat / kLtpS),
                static_cast<std::uint16_t>(flat % kLtpS));
        }

        // Per-pass LCG seeds
        const std::array<std::uint64_t, 4> seeds = {kSeed1, kSeed2, kSeed3, kSeed4};
        // Flat stat bases for each sub-table
        const std::array<std::uint32_t, 4> bases = {kLtp1Base, kLtp2Base, kLtp3Base, kLtp4Base};

        // Per-cell assignment count (to enforce max 2 memberships)
        std::vector<std::uint8_t> count(kN, 0);

        for (std::size_t k = 0; k < 4; ++k) {
            const std::uint8_t threshold = (k < 3U) ? std::uint8_t{1} : std::uint8_t{2};

            // Build pool of eligible cells
            std::vector<std::uint32_t> pool;
            pool.reserve(kN);
            for (std::uint32_t flat = 0; flat < kN; ++flat) {
                if (count[flat] < threshold) { // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                    pool.push_back(flat);
                }
            }

            // Fisher-Yates shuffle with per-pass LCG seed
            std::uint64_t state = seeds[k]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
            const auto poolSz = static_cast<std::uint32_t>(pool.size());
            for (std::uint32_t i = poolSz - 1U; i >= 1U; --i) {
                state = (state * kLcgA) + kLcgC;
                const auto j = static_cast<std::uint32_t>(state % (static_cast<std::uint64_t>(i) + 1ULL));
                // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                const std::uint32_t tmp = pool[i];
                pool[i] = pool[j];
                pool[j] = tmp;
                // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            }

            // Stable sort by spatial score ascending (ties retain shuffle order).
            // Pass 3 (k==3): sort with unassigned cells (count==0) first to guarantee
            // all 64,513 previously-uncovered cells are selected before double-assigning
            // the 1,023 lowest-score (corner) cells.
            if (k < 3U) {
                std::ranges::stable_sort(pool,
                    [&scores](const std::uint32_t a, const std::uint32_t b) {
                        return scores[a] < scores[b]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                    });
            } else {
                std::ranges::stable_sort(pool,
                    [&scores, &count](const std::uint32_t a, const std::uint32_t b) {
                        const std::uint8_t ca = count[a]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                        const std::uint8_t cb = count[b]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                        if (ca != cb) { return ca < cb; }  // count=0 before count=1
                        return scores[a] < scores[b];      // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                    });
            }

            // Assign cells from pool front to lines in decreasing-length order
            std::uint32_t pos = 0;
            for (const std::uint16_t lineIdx : sortedLines) {
                const std::uint32_t len = ltpLineLen(lineIdx);
                const std::uint32_t csrOff = sharedOffsets[lineIdx]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)

                for (std::uint32_t j = 0; j < len; ++j) {
                    const std::uint32_t flat = pool[pos + j]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                    const auto row = static_cast<std::uint16_t>(flat / kLtpS);
                    const auto col = static_cast<std::uint16_t>(flat % kLtpS);

                    // Write reverse table entry
                    data.csrCells[k][csrOff + j] = {.r = row, .c = col}; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)

                    // Write forward table entry
                    auto &mem = data.fwd[flat]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                    mem.flat[mem.count] = static_cast<std::uint16_t>( // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                        bases[k] + lineIdx); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                    mem.count++;

                    count[flat]++; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                }
                pos += len;
            }
        }

        return data;
    }

    /**
     * @name getLtpData
     * @brief Return reference to the shared (lazily-initialized) LTP tables.
     * @return Const reference to the one-time initialized LtpData.
     */
    const LtpData &getLtpData() {
        static const LtpData data = buildAllPartitions();
        return data;
    }

} // anonymous namespace

/**
 * @name ltpMembership
 * @brief Return precomputed LTP sub-table membership for cell (r, c).
 * @param r Row index in [0, kLtpS).
 * @param c Column index in [0, kLtpS).
 * @return Const reference to LtpMembership for this cell.
 */
const LtpMembership &ltpMembership(const std::uint16_t r, const std::uint16_t c) {
    return getLtpData().fwd[(static_cast<std::size_t>(r) * kLtpS) + c]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
}

/**
 * @name ltp1CellsForLine
 * @brief Return the cells on LTP1 line k.
 * @param k Line index in [0, kLtpNumLines).
 * @return Span of ltpLineLen(k) LtpCell entries.
 */
std::span<const LtpCell> ltp1CellsForLine(const std::uint16_t k) {
    const auto &d = getLtpData();
    return {d.csrCells[0].data() + d.csrOffsets[0][k], ltpLineLen(k)}; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index,cppcoreguidelines-pro-bounds-pointer-arithmetic)
}

/**
 * @name ltp2CellsForLine
 * @brief Return the cells on LTP2 line k.
 * @param k Line index in [0, kLtpNumLines).
 * @return Span of ltpLineLen(k) LtpCell entries.
 */
std::span<const LtpCell> ltp2CellsForLine(const std::uint16_t k) {
    const auto &d = getLtpData();
    return {d.csrCells[1].data() + d.csrOffsets[1][k], ltpLineLen(k)}; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index,cppcoreguidelines-pro-bounds-pointer-arithmetic)
}

/**
 * @name ltp3CellsForLine
 * @brief Return the cells on LTP3 line k.
 * @param k Line index in [0, kLtpNumLines).
 * @return Span of ltpLineLen(k) LtpCell entries.
 */
std::span<const LtpCell> ltp3CellsForLine(const std::uint16_t k) {
    const auto &d = getLtpData();
    return {d.csrCells[2].data() + d.csrOffsets[2][k], ltpLineLen(k)}; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index,cppcoreguidelines-pro-bounds-pointer-arithmetic)
}

/**
 * @name ltp4CellsForLine
 * @brief Return the cells on LTP4 line k.
 * @param k Line index in [0, kLtpNumLines).
 * @return Span of ltpLineLen(k) LtpCell entries.
 */
std::span<const LtpCell> ltp4CellsForLine(const std::uint16_t k) {
    const auto &d = getLtpData();
    return {d.csrCells[3].data() + d.csrOffsets[3][k], ltpLineLen(k)}; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index,cppcoreguidelines-pro-bounds-pointer-arithmetic)
}

} // namespace crsce::decompress::solvers
