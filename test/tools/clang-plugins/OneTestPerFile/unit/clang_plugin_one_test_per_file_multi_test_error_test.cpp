/**
 * @file clang_plugin_one_test_per_file_multi_test_error_test.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "helpers/plugin_runner.h"
#include <filesystem>
#include <gtest/gtest.h>
#include <string>

TEST(ClangPluginOneTestPerFile, MultipleTestsFail) {
    std::string log;
    const auto lib = ensure_plugin_built(log);
    ASSERT_FALSE(lib.empty()) << "Plugin build failed: " << log;

    const auto repo = repo_root_from_build_cwd();
    const auto fixture =
            repo / "test/tools/clang-plugins/OneTestPerFile/fixtures/sad/"
            "multiple_tests/multiple_tests_fail_test.cpp";
    ASSERT_TRUE(std::filesystem::exists(fixture));

    std::string out;
    const int code = clang_compile_with_plugin(fixture, lib, out);
    EXPECT_NE(code, 0) << "Expected failure for multiple TESTs";
    EXPECT_NE(out.find("only one TEST(...) is allowed per test file"),
              std::string::npos)
      << out;
}
