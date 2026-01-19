/**
 * @file clang_plugin_odph_unterminated_header_block_error_test.cpp
 * @brief Header doc has an unterminated Doxygen block (no closing star-slash);
 * expect compile failure.
 */
#include "helpers/plugin_runner.h"
#include <filesystem>
#include <gtest/gtest.h>
#include <string>

TEST(ClangPluginODPH, UnterminatedHeaderBlockFails) {
    std::string log;
    const auto lib = ensure_plugin_built(log);
    ASSERT_FALSE(lib.empty()) << "Plugin build failed: " << log;
    if (!plugin_sanity_check(lib)) {
        GTEST_SKIP() << "ODPH plugin unavailable in this environment";
    }

    const auto repo = repo_root_from_build_cwd();
    const auto fixture =
            repo / "test/tools/clang-plugins/OneDefinitionPerHeader/fixtures/sad/"
            "unterminated_header_block/include/UnterminatedHeaderBlock.h";
    ASSERT_TRUE(std::filesystem::exists(fixture));

    std::string out;
    const int code = clang_compile_with_plugin(fixture, lib, out);
    // Lexing should fail before plugin runs; accept any non-zero and common
    // diagnostic substrings.
    EXPECT_NE(code, 0)
      << "Expected compile failure for unterminated header block\n"
      << out;
    const bool saw_unterminated =
            (out.find("unterminated") != std::string::npos) ||
            (out.find("expected '*/'") != std::string::npos) ||
            (out.find("expected */") != std::string::npos);
    EXPECT_TRUE(saw_unterminated) << out;
}
