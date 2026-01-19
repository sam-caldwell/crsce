/**
 * @file clang_plugin_odph_non_doxygen_block_header_error_test.cpp
 * @brief Header doc uses non-Doxygen block at line 1; should fail.
 */
#include "helpers/plugin_runner.h"
#include <filesystem>
#include <gtest/gtest.h>
#include <string>

TEST(ClangPluginODPH, NonDoxygenBlockHeaderFails) {
    std::string log;
    const auto lib = ensure_plugin_built(log);
    ASSERT_FALSE(lib.empty()) << "Plugin build failed: " << log;
    if (!plugin_sanity_check(lib)) {
        GTEST_SKIP() << "ODPH plugin unavailable in this environment";
    }

    const auto repo = repo_root_from_build_cwd();
    const auto fixture =
            repo / "test/tools/clang-plugins/OneDefinitionPerHeader/fixtures/sad/"
            "non_doxygen_block/include/NonDoxygenBlock.h";
    ASSERT_TRUE(std::filesystem::exists(fixture));

    std::string out;
    const int code = clang_compile_with_plugin(fixture, lib, out);
    EXPECT_NE(code, 0)
      << "Expected failure for non-Doxygen block header at line 1\n"
      << out;
    EXPECT_NE(out.find("header must start with Doxygen block containing "
                  "@file/@brief and copyright"),
              std::string::npos)
      << out;
}
