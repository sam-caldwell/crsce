/**
 * @file write_one_file_test.cpp
 * @brief Unit tests for write_one_file helper.
 */
#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <vector>
#include <iterator>
#include <string>

#include "testrunner/detail/write_one_file.h"
#include "helpers/tmp_dir.h"

namespace fs = std::filesystem;

namespace {
std::vector<std::uint8_t> read_all(const fs::path &p) {
    std::ifstream is(p, std::ios::binary);
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(is)), {});
}
}

TEST(TestRunnerOnes, WriteOneFileVariousSizes) {
    const fs::path td = fs::path(tmp_dir()) / "trones_ones_unit";
    fs::create_directories(td);

    const std::vector<std::uint64_t> sizes = {0ULL, 1ULL, 511ULL, 1027ULL};
    for (std::size_t i = 0; i < sizes.size(); ++i) {
        const fs::path p = td / ("ones_" + std::to_string(i) + ".bin");
        crsce::testrunner::detail::write_one_file(p, sizes[i]);
        ASSERT_TRUE(fs::exists(p));
        EXPECT_EQ(fs::file_size(p), sizes[i]);
        const auto bytes = read_all(p);
        for (const auto b : bytes) {
            EXPECT_EQ(b, 0xFFU);
        }
    }
}
