/**
 * @file LtpTable.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief Full-coverage uniform-511 LTP partitions (B.25/B.22/B.27).
 *
 * Builds six independent sub-tables, each covering all 261,121 cells exactly once.
 * Every line has exactly 511 cells (ltp_len(k) = kLtpS for all k).
 * Sum per sub-table = 511 * 511 = 261,121.
 *
 * Construction: for each pass k=0..5, shuffle all 261,121 cells with a per-pass LCG
 * seed, then assign consecutive 511-cell chunks to lines 0..510 in order.  The pure
 * random shuffle (no spatial sort) ensures each line draws cells from a broad cross-
 * section of all rows, providing strong cross-row constraint coupling.
 *
 * B.22 seed search: seeds are runtime-overridable via environment variables
 * CRSCE_LTP_SEED_1 through CRSCE_LTP_SEED_6 (decimal or 0x-prefixed hex uint64).
 * Optimized defaults (B.26c joint 2-seed exhaustive search, 20s/candidate, 1,296 pairs):
 *   kSeed1 = CRSCLTPV (B.26c joint winner; depth 91,090)
 *   kSeed2 = CRSCLTPP (B.26c joint winner; depth 91,090)
 *   kSeed3 = CRSCLTP3 (all 36 candidates tie at 89,331 — invariant)
 *   kSeed4 = CRSCLTP4 (all 36 candidates tie at 89,331 — invariant)
 *   kSeed5 = CRSCLTP5 (B.27: additional partition for increased constraint density)
 *   kSeed6 = CRSCLTP6 (B.27: additional partition for increased constraint density)
 *
 * B.27: added LTP5/LTP6 to increase per-cell constraint density from 8 to 10 lines.
 * Prior B.22 greedy sequential result: CRSCLTP0 + CRSCLTPG = 89,331 (local optimum).
 * B.26c exhaustive joint search over 36x36=1,296 pairs found CRSCLTPV+CRSCLTPP = 91,090
 * (+1,759 improvement, +1.97%).
 *
 * Tables are computed once on first access and shared via function-local statics.
 */
#include "decompress/Solvers/LtpTable.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <ios>
#include <optional>
#include <span>
#include <utility>
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
     * @brief LCG seed for pass 0 ("CRSCLTPV" — B.26c joint winner; depth 91,090).
     *
     * B.26c exhaustive joint search over all 36×36=1,296 pairs (seeds 3/4 fixed).
     * CRSCLTPV+CRSCLTPP achieves 91,090, beating the B.22 greedy result of 89,331.
     */
    constexpr std::uint64_t kSeed1 = 0x4352'5343'4C54'5056ULL;

    /**
     * @name kSeed2
     * @brief LCG seed for pass 1 ("CRSCLTPP" — B.26c joint winner; depth 91,090).
     *
     * Jointly optimal with kSeed1=CRSCLTPV. B.26c result: +1,759 over B.22 baseline.
     */
    constexpr std::uint64_t kSeed2 = 0x4352'5343'4C54'5050ULL;

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
     * @name kSeed5
     * @brief LCG seed for pass 4 ("CRSCLTP5" — B.27 additional partition).
     */
    constexpr std::uint64_t kSeed5 = 0x4352'5343'4C54'5035ULL;

    /**
     * @name kSeed6
     * @brief LCG seed for pass 5 ("CRSCLTP6" — B.27 additional partition).
     */
    constexpr std::uint64_t kSeed6 = 0x4352'5343'4C54'5036ULL;

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
     * @brief Shared storage for all six LTP sub-tables: forward and CSR reverse tables.
     */
    struct LtpData {
        /**
         * @name fwd
         * @brief Forward table: fwd[flat] = LtpMembership (always count=6).
         */
        std::vector<LtpMembership> fwd;

        /**
         * @name csrOffsets
         * @brief CSR prefix-sum offsets: csrOffsets[sub][k] = start index in csrCells[sub].
         * csrOffsets[sub][511] = kTotalPerSubtable (sentinel).
         */
        std::array<std::array<std::uint32_t, 512>, 6> csrOffsets{};

        /**
         * @name csrCells
         * @brief CSR cell storage: csrCells[sub] has kTotalPerSubtable LtpCell entries.
         */
        std::array<std::vector<LtpCell>, 6> csrCells;
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
     * @brief Build all six B.27 full-coverage uniform-511 LTP partitions and populate LtpData.
     *
     * Six independent passes (one per sub-table):
     *   - Pool = all 261,121 cells in flat order.
     *   - Fisher-Yates LCG shuffle with per-pass seed.
     *   - Assign consecutive 511-cell chunks to lines 0..510 in order.
     *   - Every cell assigned exactly once per pass → count=6 for all cells.
     *
     * @return Fully initialized LtpData.
     */
    LtpData buildAllPartitions() {
        LtpData data;
        data.fwd.resize(kN);
        for (std::size_t sub = 0; sub < 6; ++sub) {
            data.csrCells[sub].resize(kTotalPerSubtable); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        }

        // All 6 sub-tables share the same CSR offset structure
        std::array<std::uint32_t, 512> sharedOffsets{};
        buildCsrOffsets(sharedOffsets);
        for (std::size_t sub = 0; sub < 6; ++sub) {
            data.csrOffsets[sub] = sharedOffsets; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        }

        // Lines processed in natural order 0..510 (all same length, no need to sort)
        std::array<std::uint16_t, kLtpNumLines> sortedLines{};
        { std::uint16_t v = 0; for (auto &e : sortedLines) { e = v++; } }

        // Per-pass LCG seeds (B.22/B.27: runtime-overridable via CRSCE_LTP_SEED_1..6)
        const std::array<std::uint64_t, 6> seeds = {
            parseOptEnvSeed("CRSCE_LTP_SEED_1", kSeed1),
            parseOptEnvSeed("CRSCE_LTP_SEED_2", kSeed2),
            parseOptEnvSeed("CRSCE_LTP_SEED_3", kSeed3),
            parseOptEnvSeed("CRSCE_LTP_SEED_4", kSeed4),
            parseOptEnvSeed("CRSCE_LTP_SEED_5", kSeed5),
            parseOptEnvSeed("CRSCE_LTP_SEED_6", kSeed6),
        };
        // Flat stat bases for each sub-table
        const std::array<std::uint32_t, 6> bases = {kLtp1Base, kLtp2Base, kLtp3Base, kLtp4Base, kLtp5Base, kLtp6Base};

        // Full cell pool: every cell exactly once per pass
        std::vector<std::uint32_t> pool(kN);
        { std::uint32_t v = 0; for (auto &e : pool) { e = v++; } }

        for (std::size_t k = 0; k < 6; ++k) {
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
     * @name kMagic
     * @brief 4-byte magic identifier for LTPB binary table files.
     */
    constexpr std::array<char, 4> kMagic = {'L', 'T', 'P', 'B'};

    /**
     * @name kFileVersion
     * @brief Version field written and expected in LTPB binary files.
     */
    constexpr std::uint32_t kFileVersion = 1;

    /**
     * @name buildFromAssignment
     * @brief Reconstruct LtpData from an explicit per-cell line-index assignment.
     *
     * Validates that every line in the first numSub sub-tables has exactly kLtpS
     * cells. Builds fwd + csrCells from the assignment arrays. Sub-tables beyond
     * numSub are built by independent Fisher-Yates using their configured seeds
     * (deterministic: both compress and decompress produce identical geometry).
     *
     * @param numSub  Number of sub-tables supplied in assign (1..6).
     * @param assign  assign[sub][flat] = line index (0..510) for each cell.
     * @return Optional LtpData; nullopt if any validation check fails.
     */
    [[nodiscard]] std::optional<LtpData>
    buildFromAssignment(const std::uint32_t numSub,
                        const std::array<std::vector<std::uint16_t>, 6> &assign) {
        if (numSub < 1 || numSub > 6) { return std::nullopt; }

        // Validate: each line in each supplied sub-table must have exactly kLtpS cells
        for (std::uint32_t sub = 0; sub < numSub; ++sub) {
            if (assign[sub].size() != kN) { return std::nullopt; } // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
            std::array<std::uint16_t, kLtpNumLines> cnt{};
            for (const auto lineIdx : assign[sub]) { // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                if (lineIdx >= kLtpNumLines) { return std::nullopt; }
                ++cnt[lineIdx]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
            }
            for (std::uint16_t k = 0; k < kLtpNumLines; ++k) {
                if (cnt[k] != kLtpS) { return std::nullopt; } // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
            }
        }

        LtpData data;
        data.fwd.resize(kN);

        // Build CSR offsets (same for all sub-tables under uniform-511)
        std::array<std::uint32_t, 512> sharedOffsets{};
        buildCsrOffsets(sharedOffsets);
        for (std::size_t sub = 0; sub < 6; ++sub) {
            data.csrOffsets[sub] = sharedOffsets; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        }
        for (std::size_t sub = 0; sub < 6; ++sub) {
            data.csrCells[sub].resize(kTotalPerSubtable); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        }

        const std::array<std::uint32_t, 6> bases = {
            kLtp1Base, kLtp2Base, kLtp3Base, kLtp4Base, kLtp5Base, kLtp6Base};

        // Build sub-tables from the supplied assignment arrays
        for (std::uint32_t sub = 0; sub < numSub; ++sub) {
            std::array<std::uint32_t, kLtpNumLines> writePos{};
            for (std::uint16_t k = 0; k < kLtpNumLines; ++k) {
                writePos[k] = sharedOffsets[k]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
            }
            for (std::uint32_t flat = 0; flat < kN; ++flat) {
                const std::uint16_t lineIdx = assign[sub][flat]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                const auto row = static_cast<std::uint16_t>(flat / kLtpS);
                const auto col = static_cast<std::uint16_t>(flat % kLtpS);
                data.csrCells[sub][writePos[lineIdx]] = {.r = row, .c = col}; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                ++writePos[lineIdx]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                auto &mem = data.fwd[flat]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                mem.flat[sub] = static_cast<std::uint16_t>( // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                    bases[sub] + lineIdx); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                ++mem.count;
            }
        }

        // Build remaining sub-tables using independent Fisher-Yates (fresh pool per
        // sub-table — deterministic from seed alone, no cross-pass pool sharing).
        if (numSub < 6) {
            const std::array<std::uint64_t, 6> seeds = {
                parseOptEnvSeed("CRSCE_LTP_SEED_1", kSeed1),
                parseOptEnvSeed("CRSCE_LTP_SEED_2", kSeed2),
                parseOptEnvSeed("CRSCE_LTP_SEED_3", kSeed3),
                parseOptEnvSeed("CRSCE_LTP_SEED_4", kSeed4),
                parseOptEnvSeed("CRSCE_LTP_SEED_5", kSeed5),
                parseOptEnvSeed("CRSCE_LTP_SEED_6", kSeed6),
            };
            std::array<std::uint16_t, kLtpNumLines> sortedLines{};
            { std::uint16_t v = 0; for (auto &e : sortedLines) { e = v++; } }
            std::vector<std::uint32_t> pool(kN);

            for (std::size_t k = numSub; k < 6; ++k) {
                // Fresh pool from identity permutation — independent of other passes
                { std::uint32_t v = 0; for (auto &e : pool) { e = v++; } }
                std::uint64_t state = seeds[k]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                for (std::uint32_t i = kN - 1U; i >= 1U; --i) {
                    state = (state * kLcgA) + kLcgC;
                    const auto j = static_cast<std::uint32_t>(
                        state % (static_cast<std::uint64_t>(i) + 1ULL));
                    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                    const std::uint32_t tmp = pool[i];
                    pool[i] = pool[j];
                    pool[j] = tmp;
                    // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                }
                std::uint32_t pos = 0;
                for (const std::uint16_t lineIdx : sortedLines) {
                    const std::uint32_t len  = ltpLineLen(lineIdx);
                    const std::uint32_t csrOff = sharedOffsets[lineIdx]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                    for (std::uint32_t j = 0; j < len; ++j) {
                        const std::uint32_t flat = pool[pos + j]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                        const auto row = static_cast<std::uint16_t>(flat / kLtpS);
                        const auto col = static_cast<std::uint16_t>(flat % kLtpS);
                        data.csrCells[k][csrOff + j] = {.r = row, .c = col}; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                        auto &mem = data.fwd[flat]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                        mem.flat[k] = static_cast<std::uint16_t>( // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                            bases[k] + lineIdx); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                        ++mem.count;
                    }
                    pos += len;
                }
            }
        }

        return data;
    }

    /**
     * @name tryLoadFromFile
     * @brief Try to load LTP sub-table assignments from a binary LTPB file.
     *
     * File layout (all integers little-endian):
     *   Offset  Size       Field
     *   0       4          magic = "LTPB"
     *   4       4 uint32   version = 1
     *   8       4 uint32   S (must equal kLtpS = 511)
     *   12      4 uint32   num_subtables (1..6)
     *   16      num_subtables * kN * 2  uint16[] assignment arrays, sub-major
     *
     * Returns nullopt on any error (file not found, wrong size, bad magic, etc.).
     *
     * @param path Filesystem path to the LTPB file.
     * @return Optional LtpData; nullopt on any failure.
     */
    [[nodiscard]] std::optional<LtpData> tryLoadFromFile(const char *const path) noexcept {
        try {
            std::ifstream fin(path, std::ios::binary | std::ios::ate);
            if (!fin.is_open()) { return std::nullopt; }

            struct FileHeader {
                std::array<char, 4> magic{};  // "LTPB"
                std::uint32_t version{0};
                std::uint32_t S{0};
                std::uint32_t num_subtables{0};
            };
            static_assert(sizeof(FileHeader) == 16);

            const auto fileSize = static_cast<std::uint64_t>(fin.tellg());
            if (fileSize < sizeof(FileHeader)) { return std::nullopt; }
            fin.seekg(0);

            FileHeader hdr{};
            fin.read(reinterpret_cast<char *>(&hdr), sizeof(FileHeader)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
            if (!fin.good()) { return std::nullopt; }

            // Validate header fields
            if (hdr.magic != kMagic) { return std::nullopt; }
            if (hdr.version != kFileVersion) { return std::nullopt; }
            if (hdr.S != static_cast<std::uint32_t>(kLtpS)) { return std::nullopt; }
            if (hdr.num_subtables < 1 || hdr.num_subtables > 6) { return std::nullopt; }

            const auto expectedBytes = sizeof(FileHeader)
                + ((static_cast<std::uint64_t>(hdr.num_subtables) * kN) * sizeof(std::uint16_t));
            if (fileSize != expectedBytes) { return std::nullopt; }

            std::array<std::vector<std::uint16_t>, 6> assign;
            for (std::uint32_t sub = 0; sub < hdr.num_subtables; ++sub) {
                assign[sub].resize(kN); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                fin.read( // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                    reinterpret_cast<char *>(assign[sub].data()), // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-constant-array-index)
                    static_cast<std::streamsize>(kN * sizeof(std::uint16_t)));
                if (!fin.good()) { return std::nullopt; }
            }

            return buildFromAssignment(hdr.num_subtables, assign);
        } catch (...) {
            return std::nullopt;
        }
    }

    /**
     * @name getLtpData
     * @brief Return reference to the shared (lazily-initialized) LTP tables.
     *
     * Checks CRSCE_LTP_TABLE_FILE environment variable first. If set and the file
     * parses successfully, uses the hill-climber-optimized table. Otherwise falls
     * back to the standard seed-based build (buildAllPartitions). The result is
     * cached in a function-local static for thread-safe single initialization.
     *
     * @return Const reference to the one-time initialized LtpData.
     */
    const LtpData &getLtpData() {
        static const LtpData data = []() -> LtpData {
            const char *const path = std::getenv("CRSCE_LTP_TABLE_FILE"); // NOLINT(concurrency-mt-unsafe)
            if (path != nullptr && *path != '\0') {
                if (auto loaded = tryLoadFromFile(path)) { return std::move(*loaded); }
            }
            return buildAllPartitions();
        }();
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

/**
 * @name ltp5CellsForLine
 * @brief Return the cells on LTP5 line k.
 * @param k Line index in [0, kLtpNumLines).
 * @return Span of ltpLineLen(k) LtpCell entries.
 */
std::span<const LtpCell> ltp5CellsForLine(const std::uint16_t k) {
    const auto &d = getLtpData();
    return {d.csrCells[4].data() + d.csrOffsets[4][k], ltpLineLen(k)}; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index,cppcoreguidelines-pro-bounds-pointer-arithmetic)
}

/**
 * @name ltp6CellsForLine
 * @brief Return the cells on LTP6 line k.
 * @param k Line index in [0, kLtpNumLines).
 * @return Span of ltpLineLen(k) LtpCell entries.
 */
std::span<const LtpCell> ltp6CellsForLine(const std::uint16_t k) {
    const auto &d = getLtpData();
    return {d.csrCells[5].data() + d.csrOffsets[5][k], ltpLineLen(k)}; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index,cppcoreguidelines-pro-bounds-pointer-arithmetic)
}

/**
 * @name ltpFileIsValid
 * @brief Return true iff the file at path can be parsed as a valid LTPB LTP table.
 *
 * Calls tryLoadFromFile without touching the shared getLtpData() singleton.
 * Used by tests and health-check tooling.
 *
 * @param path Filesystem path to an LTPB file.
 * @return True if the file parses successfully; false on any error.
 */
bool ltpFileIsValid(const char *const path) noexcept {
    return tryLoadFromFile(path).has_value();
}

} // namespace crsce::decompress::solvers
