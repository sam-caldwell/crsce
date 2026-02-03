/**
 * @file test_useless_success_wrapped.cpp
 * @brief Integration: run uselessTest with wrapper compress/decompress to force success.
 */

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>
#include "helpers/exe_path.h"
#include <vector>
#include <optional>
#include <system_error>

#include "testRunnerRandom/detail/run_process.h"

namespace fs = std::filesystem;

TEST(UselessTestCli, SuccessWithWrappers) {
    const fs::path bin_dir{TEST_BINARY_DIR};
    const fs::path wrap_dir = bin_dir / ".test_tmp_useless_wrappers";
    std::error_code ec; fs::create_directories(wrap_dir, ec);

    // compress wrapper: cp in out
    {
        std::ofstream os(wrap_dir / "compress");
        ASSERT_TRUE(os.good());
        os << "#!/bin/sh\n"
              "IN=\"\"; OUT=\"\";\n"
              "while [ $# -gt 0 ]; do\n"
              "  case \"$1\" in\n"
              "    -in) IN=\"$2\"; shift 2;;\n"
              "    -out) OUT=\"$2\"; shift 2;;\n"
              "    *) shift;;\n"
              "  esac\n"
              "done\n"
              "cp \"$IN\" \"$OUT\"\n";
    }
    // decompress wrapper: cp in out
    {
        std::ofstream os(wrap_dir / "decompress");
        ASSERT_TRUE(os.good());
        os << "#!/bin/sh\n"
              "IN=\"\"; OUT=\"\";\n"
              "while [ $# -gt 0 ]; do\n"
              "  case \"$1\" in\n"
              "    -in) IN=\"$2\"; shift 2;;\n"
              "    -out) OUT=\"$2\"; shift 2;;\n"
              "    *) shift;;\n"
              "  esac\n"
              "done\n"
              "cp \"$IN\" \"$OUT\"\n";
    }
    fs::permissions(wrap_dir / "compress", fs::perms::owner_exec | fs::perms::owner_read | fs::perms::owner_write, fs::perm_options::add);
    fs::permissions(wrap_dir / "decompress", fs::perms::owner_exec | fs::perms::owner_read | fs::perms::owner_write, fs::perm_options::add);

    const std::string exe = crsce::test::exe_path("uselessTest");
    const std::vector<std::string> argv = {
        "env",
        std::string("TEST_BINARY_DIR=") + wrap_dir.string(),
        std::string("CRSCE_WRAPPED_BINARIES=1"),
        exe
    };
    const auto res = crsce::testrunner::detail::run_process(argv, std::nullopt);
    EXPECT_EQ(res.exit_code, 0) << res.out << "\n" << res.err;
    ASSERT_NE(res.out.find("success"), std::string::npos);
}
