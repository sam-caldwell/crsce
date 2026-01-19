/**
 * @file clang_plugin_one_test_per_file_function_doc_missing_error_test.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "helpers/plugin_runner.h"
#include <filesystem>
#include <gtest/gtest.h>
#include <string>

TEST(ClangPluginOneTestPerFile, MissingDocBeforeFunctionFails) {
    std::string log;
    const auto lib = ensure_plugin_built(log);
    ASSERT_FALSE(lib.empty()) << "Plugin build failed: " << log;
    if (!plugin_sanity_check(lib)) {
        GTEST_SKIP() << "OneTestPerFile plugin unavailable in this environment";
    }

    const auto repo = repo_root_from_build_cwd();
    const auto fixture =
            repo /
            "test/tools/clang-plugins/OneTestPerFile/fixtures/sad/"
            "missing_doc_for_function/missing_doc_before_function_fail_test.cpp";
    ASSERT_TRUE(std::filesystem::exists(fixture));

    std::string out;
    const int code = clang_compile_with_plugin(fixture, lib, out);
    EXPECT_NE(code, 0)
      << "Expected failure for missing Doxygen block before function";
    EXPECT_NE(out.find("function definition must be immediately preceded by a "
                  "Doxygen block"),
              std::string::npos)
      << out;
}
