/**
 * @file clang_plugin_odph_missing_header_brief_error_test.cpp
 * @brief Missing @brief in header doc should fail.
 */
#include "helpers/plugin_runner.h"
#include <filesystem>
#include <gtest/gtest.h>
#include <string>

/**

 * @name ClangPluginODPH.MissingHeaderBriefFails

 * @brief Intent: exercise the expected behavior of this test.

 *         Passing indicates the behavior holds; failing indicates a regression.

 *         Assumptions: default environment and explicit setup within this test.

 */

TEST(ClangPluginODPH, MissingHeaderBriefFails) {
    std::string log;
    const auto lib = ensure_plugin_built(log);
    ASSERT_FALSE(lib.empty()) << "Plugin build failed: " << log;
    if (!plugin_sanity_check(lib)) {
        GTEST_SKIP() << "ODPH plugin unavailable in this environment";
    }

    const auto repo = repo_root_from_build_cwd();
    const auto fixture =
            repo / "test/tools/clang-plugins/OneDefinitionPerHeader/fixtures/sad/"
            "missing_header_brief/include/MissingHeaderBrief.h";
    ASSERT_TRUE(std::filesystem::exists(fixture));

    std::string out;
    const int code = clang_compile_with_plugin(fixture, lib, out);
    EXPECT_NE(code, 0) << "Expected failure for missing @brief in header doc\n"
                     << out;
    EXPECT_NE(out.find("header must start with Doxygen block containing "
                  "@file/@brief and copyright"),
              std::string::npos)
      << out;
}
