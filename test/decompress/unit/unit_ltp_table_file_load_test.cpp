/**
 * @file unit_ltp_table_file_load_test.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief Unit tests for LtpTable LTPB file loading (B.32).
 *
 * Tests cover:
 *   FileLoadRoundTrip     — write a trivial LTPB file, load it via CRSCE_LTP_TABLE_FILE,
 *                           verify ltp1CellsForLine(k) returns the expected cells.
 *   FileLoadInvalidMagic  — wrong magic bytes → ltpFileIsValid returns false.
 *   FileLoadTruncated     — valid header but truncated body → ltpFileIsValid returns false.
 *   FileLoadWrongDimension — header S=512 (not 511) → ltpFileIsValid returns false.
 *
 * The LTPB binary format:
 *   Offset   Size   Field
 *   0        4      magic = "LTPB"
 *   4        4      version = 1  (uint32_t LE)
 *   8        4      S = 511      (uint32_t LE)
 *   12       4      num_subtables (uint32_t LE)
 *   16       N*2*num_subtables  uint16_t assignment[sub][flat]
 *
 * The trivial assignment used in the round-trip test:
 *   assignment[sub][flat] = flat / kLtpS   (cell flat index → line = row index)
 * So ltp1CellsForLine(k) = all 511 cells in row k: (k,0), (k,1), …, (k,510).
 * All four sub-tables use the same trivial assignment; sub-tables 5+6 fall back to
 * Fisher-Yates defaults (the file only carries num_subtables=4).
 *
 * Environment-variable strategy:
 *   FileLoadRoundTrip calls setenv() before the first ltpXCellsForLine() call in the
 *   process; the getLtpData() function-local static is then initialized from the file.
 *   All subsequent tests share the same initialized static.
 *   Error-path tests use ltpFileIsValid() which bypasses the static entirely.
 */
#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>

#include "decompress/Solvers/LtpTable.h"

using crsce::decompress::solvers::kLtpS;
using crsce::decompress::solvers::ltp1CellsForLine;
using crsce::decompress::solvers::ltp2CellsForLine;
using crsce::decompress::solvers::ltpFileIsValid;
using crsce::decompress::solvers::ltpMembership;

namespace {

    constexpr std::uint32_t kN            = static_cast<std::uint32_t>(kLtpS) * kLtpS;  // 261,121
    constexpr std::uint32_t kNumSubTables = 4;

    /// @brief Path to the temp LTPB file written by the round-trip test setup.
    std::string gTmpFilePath;   // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

    /**
     * @brief Write a minimal valid LTPB file to path.
     *
     * Uses the trivial row-based assignment: assignment[sub][flat] = flat / kLtpS.
     * Writes kNumSubTables (4) sub-tables, all identical.
     *
     * @param path Filesystem path to write.
     * @param magic Four-byte magic string (pass "LTPB" for valid, other for error tests).
     * @param version File version field.
     * @param S Matrix dimension field.
     * @param numSub Number of sub-tables to write.
     * @param truncate If true, write only the header (no assignment data).
     */
    void writeLtpbFile(const std::string &path,
                       const std::array<char, 4> &magic,
                       const std::uint32_t version,
                       const std::uint32_t S,
                       const std::uint32_t numSub,
                       const bool truncate = false) {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open()) << "Cannot open temp file: " << path;

        out.write(magic.data(), 4);
        out.write(reinterpret_cast<const char *>(&version), sizeof(version)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        out.write(reinterpret_cast<const char *>(&S),       sizeof(S));       // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        out.write(reinterpret_cast<const char *>(&numSub),  sizeof(numSub));  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

        if (truncate) { return; }

        // Trivial row-based assignment: cell flat → line = flat / kLtpS
        const auto n = static_cast<std::size_t>(kN);
        for (std::uint32_t sub = 0; sub < numSub; ++sub) {
            for (std::uint32_t flat = 0; flat < n; ++flat) {
                const auto line = static_cast<std::uint16_t>(flat / kLtpS);
                out.write(reinterpret_cast<const char *>(&line), sizeof(line)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
            }
        }
    }

} // anonymous namespace


// ---------------------------------------------------------------------------
// FileLoadRoundTrip
// ---------------------------------------------------------------------------

/**
 * @brief Load a trivially-constructed LTPB file via CRSCE_LTP_TABLE_FILE and verify structure.
 *
 * Writes a valid LTPB file with trivial row-based assignment (cell (r,c) → line r).
 * Sets CRSCE_LTP_TABLE_FILE before the first getLtpData() call in this process.
 * Verifies:
 *   - ltp1CellsForLine(k).size() == kLtpS for all sampled k.
 *   - Every cell in ltp1CellsForLine(k) has r == k (row-based assignment was loaded).
 *   - ltp2CellsForLine(k) also reflects the same trivial assignment (sub-table 2).
 *   - ltpMembership(r,c).count == 6 (sub-tables 5+6 still default Fisher-Yates).
 */
TEST(LtpFileLoadTest, FileLoadRoundTrip) {
    // Write the trivial LTPB file.
    const auto tmpPath = std::filesystem::temp_directory_path() / "crsce_b32_roundtrip.ltpb";
    gTmpFilePath = tmpPath.string();
    writeLtpbFile(gTmpFilePath, {'L','T','P','B'}, 1, kLtpS, kNumSubTables, false);

    // Set the env var so getLtpData() uses the file on first call.
    // NOLINTNEXTLINE(concurrency-mt-unsafe,misc-include-cleaner)
    ASSERT_EQ(::setenv("CRSCE_LTP_TABLE_FILE", gTmpFilePath.c_str(), 1), 0);

    // First call to ltp1CellsForLine triggers static initialization from the file.
    for (const std::uint16_t k : {static_cast<std::uint16_t>(0),
                                   static_cast<std::uint16_t>(100),
                                   static_cast<std::uint16_t>(255),
                                   static_cast<std::uint16_t>(510)}) {
        const auto cells = ltp1CellsForLine(k);
        ASSERT_EQ(cells.size(), static_cast<std::size_t>(kLtpS))
            << "ltp1CellsForLine(" << k << ") has wrong size";
        for (const auto &cell : cells) {
            EXPECT_EQ(cell.r, k)
                << "ltp1CellsForLine(" << k << "): unexpected row " << cell.r;
        }
    }

    // ltp2CellsForLine should also reflect the trivial row-based assignment.
    for (const std::uint16_t k : {static_cast<std::uint16_t>(0),
                                   static_cast<std::uint16_t>(255),
                                   static_cast<std::uint16_t>(510)}) {
        const auto cells = ltp2CellsForLine(k);
        ASSERT_EQ(cells.size(), static_cast<std::size_t>(kLtpS));
        for (const auto &cell : cells) {
            EXPECT_EQ(cell.r, k)
                << "ltp2CellsForLine(" << k << "): unexpected row " << cell.r;
        }
    }

    // ltpMembership count is always 6 (sub-tables 5+6 from default Fisher-Yates).
    EXPECT_EQ(ltpMembership(0, 0).count,       static_cast<std::uint8_t>(6));
    EXPECT_EQ(ltpMembership(255, 100).count,   static_cast<std::uint8_t>(6));
    EXPECT_EQ(ltpMembership(510, 510).count,   static_cast<std::uint8_t>(6));

    // Clean up temp file.
    std::filesystem::remove(tmpPath);
}


// ---------------------------------------------------------------------------
// Error-path tests (use ltpFileIsValid, which bypasses the static)
// ---------------------------------------------------------------------------

/**
 * @brief A file with wrong magic bytes ("XXXX") is rejected by ltpFileIsValid.
 */
TEST(LtpFileLoadTest, FileLoadInvalidMagic) {
    const auto tmpPath = std::filesystem::temp_directory_path() / "crsce_b32_bad_magic.ltpb";
    writeLtpbFile(tmpPath.string(), {'X','X','X','X'}, 1, kLtpS, kNumSubTables, false);
    EXPECT_FALSE(ltpFileIsValid(tmpPath.string().c_str()));
    std::filesystem::remove(tmpPath);
}

/**
 * @brief A file with valid header but truncated body is rejected by ltpFileIsValid.
 */
TEST(LtpFileLoadTest, FileLoadTruncated) {
    const auto tmpPath = std::filesystem::temp_directory_path() / "crsce_b32_truncated.ltpb";
    writeLtpbFile(tmpPath.string(), {'L','T','P','B'}, 1, kLtpS, kNumSubTables, true);
    EXPECT_FALSE(ltpFileIsValid(tmpPath.string().c_str()));
    std::filesystem::remove(tmpPath);
}

/**
 * @brief A file with S=512 (wrong dimension) is rejected by ltpFileIsValid.
 */
TEST(LtpFileLoadTest, FileLoadWrongDimension) {
    const auto tmpPath = std::filesystem::temp_directory_path() / "crsce_b32_wrong_dim.ltpb";
    writeLtpbFile(tmpPath.string(), {'L','T','P','B'}, 1, 512, kNumSubTables, false);
    EXPECT_FALSE(ltpFileIsValid(tmpPath.string().c_str()));
    std::filesystem::remove(tmpPath);
}

/**
 * @brief A non-existent path is rejected by ltpFileIsValid.
 */
TEST(LtpFileLoadTest, FileLoadNonexistentPath) {
    EXPECT_FALSE(ltpFileIsValid("/tmp/crsce_b32_does_not_exist_xyz.ltpb"));
}
