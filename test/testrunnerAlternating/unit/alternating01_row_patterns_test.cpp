/**
 * @file alternating01_row_patterns_test.cpp
 * @brief Verify alternating01 generator yields 0101.. on row0 and 1010.. on row1 when mapped row-major.
 */
#include <gtest/gtest.h>

#include <cstddef>
#include <filesystem>
#include <vector>

#include "testrunner/detail/write_alternating01_file.h"
#include "common/FileBitSerializer/FileBitSerializer.h"
#include "decompress/Csm/detail/Csm.h"

using crsce::decompress::Csm;

/**
 * @name Alternating01.Row0Row1Patterns
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(Alternating01, Row0Row1Patterns) { // NOLINT
    namespace fs = std::filesystem;
    const fs::path path = fs::path(TEST_BINARY_DIR) / "alt01_rows.bin"; // NOLINT
    // Need at least 2*S bits; write 128 bytes (1024 bits)
    constexpr std::size_t bytes = 128;
    crsce::testrunner::detail::write_alternating01_file(path, bytes);

    crsce::common::FileBitSerializer s(path.string());
    ASSERT_TRUE(s.good());

    // Read 2*S bits and verify row0=0101..., row1=1010...
    constexpr std::size_t S = Csm::kS;
    const std::size_t need = 2 * S;
    std::vector<int> bits; bits.reserve(need);
    for (std::size_t i = 0; i < need; ++i) {
        auto b = s.pop();
        ASSERT_TRUE(b.has_value());
        bits.push_back(*b ? 1 : 0);
    }

    // Check mapping k -> (r,c) with row-major: k = r*S + c
    for (std::size_t k = 0; k < need; ++k) {
        const std::size_t r = k / S;
        const std::size_t c = k % S;
        const int expect = static_cast<int>((r + c) % 2); // row0: c%2; row1: 1-c%2
        EXPECT_EQ(bits[k], expect) << "mismatch at r=" << r << ", c=" << c;
    }
}
