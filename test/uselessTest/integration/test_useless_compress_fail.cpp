/**
 * @file test_useless_compress_fail.cpp
 * @brief Integration: run uselessTest with failing compressor to cover failure path.
 */

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include "helpers/exe_path.h"
#include <optional>
#include <system_error>

#include "testRunnerRandom/detail/run_process.h"

namespace fs = std::filesystem;

TEST(UselessTestCli, CompressFail) {
    const fs::path bin_dir{TEST_BINARY_DIR};
    const fs::path wrap_dir = bin_dir / ".test_tmp_useless_fail_cx";
    std::error_code ec; fs::create_directories(wrap_dir, ec);

    // compress wrapper: exit 3
    {
        std::ofstream os(wrap_dir / "compress");
        ASSERT_TRUE(os.good());
        os << "#!/bin/sh\nexit 3\n";
    }
    // decompress wrapper: stub ok
    {
        std::ofstream os(wrap_dir / "decompress");
        ASSERT_TRUE(os.good());
        os << "#!/bin/sh\nexit 0\n";
    }
    fs::permissions(wrap_dir / "compress", fs::perms::owner_exec | fs::perms::owner_read | fs::perms::owner_write, fs::perm_options::add);
    fs::permissions(wrap_dir / "decompress", fs::perms::owner_exec | fs::perms::owner_read | fs::perms::owner_write, fs::perm_options::add);

    const std::string exe = crsce::test::exe_path("uselessTest");
    const std::vector<std::string> argv = {
        "env",
        std::string("TEST_BINARY_DIR=") + wrap_dir.string(),
        exe
    };
    const auto res = crsce::testrunner::detail::run_process(argv, std::nullopt);
    EXPECT_EQ(res.exit_code, 0) << res.out << "\n" << res.err;
    ASSERT_NE(res.out.find("failed"), std::string::npos);
}
