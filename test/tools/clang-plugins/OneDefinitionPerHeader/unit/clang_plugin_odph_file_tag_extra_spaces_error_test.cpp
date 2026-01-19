/**
 * @file clang_plugin_odph_file_tag_extra_spaces_error_test.cpp
 * @brief Header doc has extra spaces in @file line; basename check should fail.
 */
#include "helpers/plugin_runner.h"
#include <filesystem>
#include <gtest/gtest.h>
#include <string>

TEST(ClangPluginODPH, FileTagExtraSpacesFails) {
    std::string log;
    const auto lib = ensure_plugin_built(log);
    ASSERT_FALSE(lib.empty()) << "Plugin build failed: " << log;
    if (!plugin_sanity_check(lib)) {
        GTEST_SKIP() << "ODPH plugin unavailable in this environment";
    }
    if (!plugin_sanity_check(lib)) {
        GTEST_SKIP() << "ODPH plugin unavailable in this environment";
    }

    const auto repo = repo_root_from_build_cwd();
    const auto fixture =
            repo / "test/tools/clang-plugins/OneDefinitionPerHeader/fixtures/sad/"
            "file_tag_extra_spaces/include/FileTagExtraSpaces.h";
    ASSERT_TRUE(std::filesystem::exists(fixture));

    std::string out;
    const int code = clang_compile_with_plugin(fixture, lib, out);
    EXPECT_NE(code, 0) << "Expected failure for extra spaces in @file line\n"
                     << out;
    EXPECT_NE(out.find("header @file tag must match current file basename"),
              std::string::npos)
      << out;
}
