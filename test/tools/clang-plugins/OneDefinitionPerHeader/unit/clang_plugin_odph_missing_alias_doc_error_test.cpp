/**
 * @file clang_plugin_odph_missing_alias_doc_error_test.cpp
 * @brief ODPH: missing alias doc should fail.
 */
#include "helpers/plugin_runner.h"
#include <filesystem>
#include <gtest/gtest.h>
#include <string>

TEST(ClangPluginODPH, MissingAliasDocFails) {
    std::string log;
    const auto lib = ensure_plugin_built(log);
    ASSERT_FALSE(lib.empty()) << "Plugin build failed: " << log;
    if (!plugin_sanity_check(lib)) {
        GTEST_SKIP() << "ODPH plugin unavailable in this environment";
    }

    const auto repo = repo_root_from_build_cwd();
    const auto fixture = repo / "test/tools/clang-plugins/OneDefinitionPerHeader/"
                         "fixtures/sad/missing_construct_doc_alias/include/MissingAliasDoc.h";
    ASSERT_TRUE(std::filesystem::exists(fixture));

    std::string out;
    const int code = clang_compile_with_plugin(fixture, lib, out);
    EXPECT_NE(code, 0) << out;
    EXPECT_NE(out.find("Doxygen block"), std::string::npos) << out;
}
