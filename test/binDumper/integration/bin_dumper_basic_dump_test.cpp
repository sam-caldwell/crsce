/**
 * @file bin_dumper_basic_dump_test.cpp
 * @brief Verify binDumper prints expected bits for a small file.
 */
#include <gtest/gtest.h>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <optional>

#include "testRunnerRandom/detail/run_process.h"
#include "helpers/tmp_dir.h"

namespace fs = std::filesystem;

TEST(BinDumper, BasicTwoBytes) {
    const fs::path out = fs::path(tmp_dir()) / "bin_dumper_basic.bin";
    {
        std::ofstream os(out, std::ios::binary);
        const unsigned char a = 0xAAU; // 10101010
        const unsigned char b = 0x55U; // 01010101
        os.put(static_cast<char>(a));
        os.put(static_cast<char>(b));
    }

    const fs::path bin_dir{TEST_BINARY_DIR};
    const fs::path exe = bin_dir / "binDumper";
    const auto res = crsce::testrunner::detail::run_process({exe.string(), out.string()}, std::nullopt);

    ASSERT_EQ(res.exit_code, 0);
    EXPECT_EQ(res.out, std::string("0000:1010101001010101\n"));
    EXPECT_TRUE(res.err.empty());
}
