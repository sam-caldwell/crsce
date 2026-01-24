/**
 * @file clang_plugin_odpcpp_happy_test.cpp
 * @brief OneDefinitionPerCppFile: happy path passes.
 */
#include "helpers/plugin_runner.h"
#include <filesystem>
#include <gtest/gtest.h>
#include <string>

/**

 * @name ClangPluginODPCPP.HappyPasses

 * @brief Intent: exercise the expected behavior of this test.

 *         Passing indicates the behavior holds; failing indicates a regression.

 *         Assumptions: default environment and explicit setup within this test.

 */

TEST(ClangPluginODPCPP, HappyPasses) {
    std::string log;
    const auto lib = ensure_plugin_built(log);
    ASSERT_FALSE(lib.empty()) << "Plugin build failed: " << log;
    if (!plugin_sanity_check(lib)) {
        GTEST_SKIP() << "ODPCPP plugin unavailable in this environment";
    }
    if (!plugin_sanity_check(lib)) {
        GTEST_SKIP() << "ODPCPP plugin unavailable in this environment";
    }

    const auto repo = repo_root_from_build_cwd();
    const auto fixture =
            repo / "test/tools/clang-plugins/OneDefinitionPerCppFile/fixtures/"
            "happy/src/Happy.cpp";
    ASSERT_TRUE(std::filesystem::exists(fixture));

    std::string out;
    const int code = clang_compile_with_plugin(fixture, lib, out);
    EXPECT_EQ(code, 0) << out;
}
