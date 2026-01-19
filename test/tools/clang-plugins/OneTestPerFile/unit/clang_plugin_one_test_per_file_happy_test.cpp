/**
 * @file clang_plugin_one_test_per_file_happy_test.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "helpers/plugin_runner.h"
#include <filesystem>
#include <gtest/gtest.h>
#include <string>

TEST(ClangPluginOneTestPerFile, HappyPasses) {
    std::string log;
    const auto lib = ensure_plugin_built(log);
    ASSERT_FALSE(lib.empty()) << "Plugin build failed: " << log;
    if (!plugin_sanity_check(lib)) {
        GTEST_SKIP() << "OneTestPerFile plugin unavailable in this environment";
    }
    if (!plugin_sanity_check(lib)) {
        GTEST_SKIP() << "OneTestPerFile plugin unavailable in this environment";
    }

    const auto repo = repo_root_from_build_cwd();
    const auto fixture = repo / "test/tools/clang-plugins/OneTestPerFile/"
                         "fixtures/happy/one_test_and_doc_ok_test.cpp";
    ASSERT_TRUE(std::filesystem::exists(fixture));

    std::string out;
    const int code = clang_compile_with_plugin(fixture, lib, out);
    EXPECT_EQ(code, 0) << out;
}
