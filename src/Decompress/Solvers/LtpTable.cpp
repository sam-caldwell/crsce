/**
 * @file LtpTable.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief LTP1–LTP4 lookup-table partition implementation.
 *
 * Builds four pseudorandom partitions of all 261,121 cells into 511 lines of
 * 511 cells each via Fisher-Yates shuffle with deterministic LCG seeds.
 * Tables are computed once on first access and shared via function-local statics.
 */
#include "decompress/Solvers/LtpTable.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace crsce::decompress::solvers {

namespace {

    /**
     * @name kN
     * @brief Total number of cells in the matrix (511 * 511 = 261,121).
     */
    constexpr std::uint32_t kN = static_cast<std::uint32_t>(kLtpS) * kLtpS;

    /**
     * @name kSeed1
     * @brief LCG seed for LTP1 partition ("CRSCLTP1").
     */
    constexpr std::uint64_t kSeed1 = 0x4352'5343'4C54'5031ULL;

    /**
     * @name kSeed2
     * @brief LCG seed for LTP2 partition ("CRSCLTP2").
     */
    constexpr std::uint64_t kSeed2 = 0x4352'5343'4C54'5032ULL;

    /**
     * @name kSeed3
     * @brief LCG seed for LTP3 partition ("CRSCLTP3").
     */
    constexpr std::uint64_t kSeed3 = 0x4352'5343'4C54'5033ULL;

    /**
     * @name kSeed4
     * @brief LCG seed for LTP4 partition ("CRSCLTP4").
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
     * @name lcgNext
     * @brief Advance LCG state and return new value.
     * @param state Current LCG state (updated in place).
     * @return New state value.
     */
    inline std::uint64_t lcgNext(std::uint64_t &state) {
        state = (state * kLcgA) + kLcgC;
        return state;
    }

    /**
     * @struct LtpData
     * @name LtpData
     * @brief Shared storage for all four LTP partitions: forward and reverse tables.
     */
    struct LtpData {
        /**
         * @name fwd
         * @brief Forward table: fwd[flat] = {LTP1 flat stat idx, LTP2, LTP3, LTP4}.
         */
        std::vector<std::array<std::uint16_t, 4>> fwd;

        /**
         * @name rev1
         * @brief Reverse table for LTP1: rev1[k] = array of 511 cells on line k.
         */
        std::vector<std::array<LtpCell, 511>> rev1;

        /**
         * @name rev2
         * @brief Reverse table for LTP2: rev2[k] = array of 511 cells on line k.
         */
        std::vector<std::array<LtpCell, 511>> rev2;

        /**
         * @name rev3
         * @brief Reverse table for LTP3: rev3[k] = array of 511 cells on line k.
         */
        std::vector<std::array<LtpCell, 511>> rev3;

        /**
         * @name rev4
         * @brief Reverse table for LTP4: rev4[k] = array of 511 cells on line k.
         */
        std::vector<std::array<LtpCell, 511>> rev4;
    };

    /**
     * @name buildOnePartition
     * @brief Run Fisher-Yates for one partition and fill forward/reverse tables.
     *
     * @param seed       LCG seed for this partition.
     * @param base       Flat stat base offset (kLtp1Base .. kLtp4Base).
     * @param fwdSlot    Index into fwd[][slot] (0=LTP1, 1=LTP2, 2=LTP3, 3=LTP4).
     * @param fwd        Forward table to fill.
     * @param rev        Reverse table to fill (511 lines of 511 cells).
     */
    void buildOnePartition(const std::uint64_t seed,
                           const std::uint32_t base,
                           const std::size_t fwdSlot,
                           std::vector<std::array<std::uint16_t, 4>> &fwd,
                           std::vector<std::array<LtpCell, 511>> &rev) {
        // Initialize index array [0 .. kN-1]
        std::vector<std::uint32_t> indices(kN);
        for (std::uint32_t k = 0; k < kN; ++k) { indices[k] = k; } // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

        // Fisher-Yates shuffle: for i = kN-1 downto 1, swap(indices[i], indices[lcg() % (i+1)])
        std::uint64_t state = seed;
        for (std::uint32_t i = kN - 1U; i >= 1U; --i) {
            const std::uint64_t rval = lcgNext(state);
            auto j = static_cast<std::uint32_t>(
                rval % (static_cast<std::uint64_t>(i) + 1ULL));
            const std::uint32_t tmp = indices[i]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            indices[i] = indices[j];              // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            indices[j] = tmp;                     // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        }

        // Build forward and reverse tables from the shuffled indices.
        // Line k gets cells: indices[k*kLtpS + 0 .. k*kLtpS + kLtpS - 1]
        for (std::uint16_t k = 0; k < kLtpNumLines; ++k) {
            for (std::uint16_t i = 0; i < kLtpS; ++i) {
                const std::uint32_t flat =
                    indices[(static_cast<std::uint32_t>(k) * kLtpS) + i]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                // Forward: flat stat index = base + k
                fwd[flat][fwdSlot] = static_cast<std::uint16_t>(base + k); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                // Reverse: line k, position i = cell (flat/kLtpS, flat%kLtpS)
                rev[k][i] = {.r = static_cast<std::uint16_t>(flat / kLtpS), // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                             .c = static_cast<std::uint16_t>(flat % kLtpS)};
            }
        }
    }

    /**
     * @name buildLtpData
     * @brief Build all four LTP partitions and return the populated LtpData struct.
     * @return Fully initialized LtpData with fwd, rev1, rev2, rev3, rev4.
     */
    LtpData buildLtpData() {
        LtpData data;
        data.fwd.resize(kN);
        data.rev1.resize(kLtpNumLines);
        data.rev2.resize(kLtpNumLines);
        data.rev3.resize(kLtpNumLines);
        data.rev4.resize(kLtpNumLines);

        buildOnePartition(kSeed1, kLtp1Base, 0, data.fwd, data.rev1);
        buildOnePartition(kSeed2, kLtp2Base, 1, data.fwd, data.rev2);
        buildOnePartition(kSeed3, kLtp3Base, 2, data.fwd, data.rev3);
        buildOnePartition(kSeed4, kLtp4Base, 3, data.fwd, data.rev4);

        return data;
    }

    /**
     * @name getLtpData
     * @brief Return reference to the shared (lazily-initialized) LTP tables.
     * @return Const reference to the one-time initialized LtpData.
     */
    const LtpData &getLtpData() {
        static const LtpData data = buildLtpData();
        return data;
    }

} // anonymous namespace

/**
 * @name ltpFlatIndices
 * @brief Return precomputed flat stat indices for LTP1–LTP4 at cell (r, c).
 * @param r Row index in [0, kLtpS).
 * @param c Column index in [0, kLtpS).
 * @return Reference to array of 4 flat stat indices.
 */
const std::array<std::uint16_t, 4> &ltpFlatIndices(const std::uint16_t r, const std::uint16_t c) {
    return getLtpData().fwd[(static_cast<std::size_t>(r) * kLtpS) + c];
}

/**
 * @name ltp1CellsForLine
 * @brief Return the 511 cells on LTP1 line k.
 * @param k Line index in [0, kLtpNumLines).
 * @return Reference to array of 511 LtpCell entries.
 */
const std::array<LtpCell, 511> &ltp1CellsForLine(const std::uint16_t k) {
    return getLtpData().rev1[k];
}

/**
 * @name ltp2CellsForLine
 * @brief Return the 511 cells on LTP2 line k.
 * @param k Line index in [0, kLtpNumLines).
 * @return Reference to array of 511 LtpCell entries.
 */
const std::array<LtpCell, 511> &ltp2CellsForLine(const std::uint16_t k) {
    return getLtpData().rev2[k];
}

/**
 * @name ltp3CellsForLine
 * @brief Return the 511 cells on LTP3 line k.
 * @param k Line index in [0, kLtpNumLines).
 * @return Reference to array of 511 LtpCell entries.
 */
const std::array<LtpCell, 511> &ltp3CellsForLine(const std::uint16_t k) {
    return getLtpData().rev3[k];
}

/**
 * @name ltp4CellsForLine
 * @brief Return the 511 cells on LTP4 line k.
 * @param k Line index in [0, kLtpNumLines).
 * @return Reference to array of 511 LtpCell entries.
 */
const std::array<LtpCell, 511> &ltp4CellsForLine(const std::uint16_t k) {
    return getLtpData().rev4[k];
}

} // namespace crsce::decompress::solvers
