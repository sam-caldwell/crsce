/**
 * @file LtpTable.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief Full-coverage uniform-511 LTP partitions (B.25/B.22).
 *
 * Builds four independent sub-tables, each covering all 261,121 cells exactly once.
 * Every line has exactly 511 cells (ltp_len(k) = kLtpS for all k).
 * Sum per sub-table = 511 * 511 = 261,121.
 *
 * Construction: for each pass k=0..3, shuffle all 261,121 cells with a per-pass LCG
 * seed, then assign consecutive 511-cell chunks to lines 0..510 in order.  The pure
 * random shuffle (no spatial sort) ensures each line draws cells from a broad cross-
 * section of all rows, providing strong cross-row constraint coupling.
 *
 * B.22 seed search: seeds are runtime-overridable via environment variables
 * CRSCE_LTP_SEED_1 through CRSCE_LTP_SEED_4 (decimal or 0x-prefixed hex uint64).
 * Optimized defaults (4-phase independent search, 45s/candidate):
 *   kSeed1 = CRSCLTPR (phase-1 winner; depth 89,672)
 *   kSeed2 = CRSCLTPG (phase-2 winner; depth 90,448)
 *   kSeed3 = CRSCLTP3 (phase-3: all 36 candidates tie at 90,448 — invariant)
 *   kSeed4 = CRSCLTP4 (phase-4: all 36 candidates tie at 90,448 — invariant)
 *
 * Tables are computed once on first access and shared via function-local statics.
 */
#include "decompress/Solvers/LtpTable.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
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
     * @brief Total cells per sub-table: 511 lines * 511 cells = 261,121.
     */
    constexpr std::uint32_t kTotalPerSubtable = kN;

    /**
     * @name kSeed1
     * @brief LCG seed for pass 0 ("CRSCLTPR" — B.22 phase-1 winner; depth 89,672).
     */
    constexpr std::uint64_t kSeed1 = 0x4352'5343'4C54'5052ULL;

    /**
     * @name kSeed2
     * @brief LCG seed for pass 1 ("CRSCLTPG" — B.22 phase-2 winner; depth 90,448).
     */
    constexpr std::uint64_t kSeed2 = 0x4352'5343'4C54'5047ULL;

    /**
     * @name kSeed3
     * @brief LCG seed for pass 2 ("CRSCLTP3" — phase-3 search: all 36 candidates tie at 90,448).
     */
    constexpr std::uint64_t kSeed3 = 0x4352'5343'4C54'5033ULL;

    /**
     * @name kSeed4
     * @brief LCG seed for pass 3 ("CRSCLTP4" — phase-4 search: all 36 candidates tie at 90,448).
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
     * @name parseOptEnvSeed
     * @brief Read an optional uint64 seed from an environment variable.
     *
     * Used by B.22 (partition seed search) to allow runtime seed override without
     * rebuilding.  Format: decimal (e.g. "4702111234474983473") or 0x-prefixed hex
     * (e.g. "0x4352534C54503031").  Returns defaultVal when the variable is absent
     * or unparseable.
     *
     * @param envName   Environment variable name (e.g. "CRSCE_LTP_SEED_1").
     * @param defaultVal Default seed value when the variable is absent.
     * @return Parsed seed, or defaultVal.
     */
    [[nodiscard]] std::uint64_t parseOptEnvSeed(const char *const envName,
                                                 const std::uint64_t defaultVal) noexcept {
        const char *const v = std::getenv(envName); // NOLINT(concurrency-mt-unsafe)
        if (v == nullptr || *v == '\0') { return defaultVal; }
        // strtoull with base=0 handles decimal, 0x-hex, 0-octal.
        // Returns 0 for unparseable input; seed=0 is accepted (not the default check).
        return static_cast<std::uint64_t>(std::strtoull(v, nullptr, 0));
    }

    /**
     * @struct LtpData
     * @name LtpData
     * @brief Shared storage for all four LTP sub-tables: forward and CSR reverse tables.
     */
    struct LtpData {
        /**
         * @name fwd
         * @brief Forward table: fwd[flat] = LtpMembership (always count=4).
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
     * @name buildCsrOffsets
     * @brief Precompute CSR prefix-sum offsets from ltp_len values.
     * @param offsets Output: offsets[0..511] where offsets[511] = kTotalPerSubtable.
     */
    void buildCsrOffsets(std::array<std::uint32_t, 512> &offsets) {
        offsets[0] = 0;
        for (std::uint16_t k = 0; k < kLtpNumLines; ++k) {
            offsets[k + 1] = offsets[k] + ltpLineLen(k); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        }
        // offsets[511] == kTotalPerSubtable == kN
    }

    /**
     * @name buildAllPartitions
     * @brief Build all four B.23 full-coverage uniform-511 LTP partitions and populate LtpData.
     *
     * Four independent passes (one per sub-table):
     *   - Pool = all 261,121 cells in flat order.
     *   - Fisher-Yates LCG shuffle with per-pass seed.
     *   - Assign consecutive 511-cell chunks to lines 0..510 in order.
     *   - Every cell assigned exactly once per pass → count=4 for all cells.
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

        // Lines processed in natural order 0..510 (all same length, no need to sort)
        std::array<std::uint16_t, kLtpNumLines> sortedLines{};
        { std::uint16_t v = 0; for (auto &e : sortedLines) { e = v++; } }

        // Per-pass LCG seeds (B.22: runtime-overridable via CRSCE_LTP_SEED_1..4)
        const std::array<std::uint64_t, 4> seeds = {
            parseOptEnvSeed("CRSCE_LTP_SEED_1", kSeed1),
            parseOptEnvSeed("CRSCE_LTP_SEED_2", kSeed2),
            parseOptEnvSeed("CRSCE_LTP_SEED_3", kSeed3),
            parseOptEnvSeed("CRSCE_LTP_SEED_4", kSeed4),
        };
        // Flat stat bases for each sub-table
        const std::array<std::uint32_t, 4> bases = {kLtp1Base, kLtp2Base, kLtp3Base, kLtp4Base};

        // Full cell pool: every cell exactly once per pass
        std::vector<std::uint32_t> pool(kN);
        { std::uint32_t v = 0; for (auto &e : pool) { e = v++; } }

        for (std::size_t k = 0; k < 4; ++k) {
            // Fisher-Yates shuffle with per-pass LCG seed
            std::uint64_t state = seeds[k]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
            for (std::uint32_t i = kN - 1U; i >= 1U; --i) {
                state = (state * kLcgA) + kLcgC;
                const auto j = static_cast<std::uint32_t>(state % (static_cast<std::uint64_t>(i) + 1ULL));
                // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                const std::uint32_t tmp = pool[i];
                pool[i] = pool[j];
                pool[j] = tmp;
                // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            }

            // Assign consecutive chunks of the shuffled pool to lines
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

                    // Write forward table entry (sub-table k's slot)
                    auto &mem = data.fwd[flat]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                    mem.flat[k] = static_cast<std::uint16_t>( // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                        bases[k] + lineIdx); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                    mem.count++;
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
 * @return Const reference to LtpMembership for this cell (always count=4).
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
