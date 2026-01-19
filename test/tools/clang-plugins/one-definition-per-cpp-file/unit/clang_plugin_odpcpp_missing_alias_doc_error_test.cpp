/**
 * @file clang_plugin_odpcpp_missing_alias_doc_error_test.cpp
 * @brief ODPCPP: missing alias doc should fail.
 */
#include "helpers/plugin_runner.h"
#include <filesystem>
#include <gtest/gtest.h>
#include <string>

TEST(ClangPluginODPCPP, MissingAliasDocFails) {
    std::string log;
    const auto lib = ensure_plugin_built(log);
    ASSERT_FALSE(lib.empty()) << "Plugin build failed: " << log;
    if (!plugin_sanity_check(lib)) {
        GTEST_SKIP() << "ODPCPP plugin unavailable in this environment";
    }

    const auto repo = repo_root_from_build_cwd();
    const auto fixture = repo / "test/tools/clang-plugins/one-definition-per-cpp-file/"
                         "fixtures/sad/missing_alias_doc/src/MissingAliasDoc.cpp";
    ASSERT_TRUE(std::filesystem::exists(fixture));

    std::string out;
    const int code = clang_compile_with_plugin(fixture, lib, out);
    EXPECT_NE(code, 0) << out;
    EXPECT_NE(out.find("Doxygen block"), std::string::npos) << out;
}
