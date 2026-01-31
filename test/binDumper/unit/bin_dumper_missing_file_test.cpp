/**
 * @file bin_dumper_missing_file_test.cpp
 * @brief Verify binDumper error path on missing file.
 */
#include <gtest/gtest.h>

#include <cstddef>
#include <filesystem>
#include <string>
#include <optional>

#include "testrunner/detail/run_process.h"

namespace fs = std::filesystem;

TEST(BinDumper, MissingFile) {
    const fs::path bin_dir{TEST_BINARY_DIR};
    const fs::path exe = bin_dir / "binDumper";
    const fs::path missing = fs::path("/this/path/should/not/exist.bin");
    const auto res = crsce::testrunner::detail::run_process({exe.string(), missing.string()}, std::nullopt);
    EXPECT_NE(res.exit_code, 0);
    EXPECT_NE(res.err.find("error: cannot open"), std::string::npos);
}
