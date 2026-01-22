/**
 * @file clang_plugin_odpcpp_missing_construct_brief_error_test.cpp
 * @brief ODPCPP: construct doc missing @brief should fail.
 */
#include "helpers/plugin_runner.h"
#include <filesystem>
#include <gtest/gtest.h>
#include <string>

/**

 * @name ClangPluginODPCPP.MissingConstructBriefFails

 * @brief Intent: exercise the expected behavior of this test.

 *         Passing indicates the behavior holds; failing indicates a regression.

 *         Assumptions: default environment and explicit setup within this test.

 */

TEST(ClangPluginODPCPP, MissingConstructBriefFails) {
    std::string log;
    const auto lib = ensure_plugin_built(log);
    ASSERT_FALSE(lib.empty()) << "Plugin build failed: " << log;
    if (!plugin_sanity_check(lib)) {
        GTEST_SKIP() << "ODPCPP plugin unavailable in this environment";
    }

    const auto repo = repo_root_from_build_cwd();
    const auto fixture = repo / "test/tools/clang-plugins/one-definition-per-cpp-file/"
                         "fixtures/sad/missing_construct_brief/src/MissingConstructBrief.cpp";
    ASSERT_TRUE(std::filesystem::exists(fixture));

    std::string out;
    const int code = clang_compile_with_plugin(fixture, lib, out);
    EXPECT_NE(code, 0) << out;
    EXPECT_NE(out.find("with @brief"), std::string::npos) << out;
}
