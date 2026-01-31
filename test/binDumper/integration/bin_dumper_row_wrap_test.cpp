/**
 * @file bin_dumper_row_wrap_test.cpp
 * @brief Verify binDumper wraps at 511 bits per line.
 */
#include <gtest/gtest.h>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <optional>

#include "testrunner/detail/run_process.h"
#include "helpers/tmp_dir.h"

namespace fs = std::filesystem;

TEST(BinDumper, WrapsAt511Bits) {
    const fs::path out = fs::path(tmp_dir()) / "bin_dumper_wrap.bin";
    // 512 bits of 1s -> 64 bytes of 0xFF
    {
        std::ofstream os(out, std::ios::binary);
        const unsigned char ff = 0xFFU;
        for (int i = 0; i < 64; ++i) { os.put(static_cast<char>(ff)); }
    }

    std::string expected;
    // Line 0 prefix
    expected += "0000:";
    for (std::size_t i = 1; i <= 511; ++i) {
        expected.push_back('1');
        if ((i % 128U) == 0 && i < 511U) { expected.push_back(' '); }
    }
    expected.push_back('\n');
    // Line starting at bit offset 0x1FF (511 decimal)
    expected += "01ff:";
    expected.push_back('1');
    expected.push_back('\n');

    const fs::path bin_dir{TEST_BINARY_DIR};
    const fs::path exe = bin_dir / "binDumper";
    const auto res = crsce::testrunner::detail::run_process({exe.string(), out.string()}, std::nullopt);
    ASSERT_EQ(res.exit_code, 0);
    EXPECT_EQ(res.out, expected);
    EXPECT_TRUE(res.err.empty());
}
