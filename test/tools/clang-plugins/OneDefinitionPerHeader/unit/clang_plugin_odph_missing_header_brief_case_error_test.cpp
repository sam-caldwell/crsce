/**
 * @file clang_plugin_odph_missing_header_brief_case_error_test.cpp
 * @brief Header doc uses @Brief (wrong case); should fail.
 */
#include "helpers/plugin_runner.h"
#include <filesystem>
#include <gtest/gtest.h>
#include <string>

TEST(ClangPluginODPH, MissingHeaderBriefCaseFails) {
  std::string log;
  const auto lib = ensure_plugin_built(log);
  ASSERT_FALSE(lib.empty()) << "Plugin build failed: " << log;

  const auto repo = repo_root_from_build_cwd();
  const auto fixture =
      repo / "test/tools/clang-plugins/OneDefinitionPerHeader/fixtures/sad/"
             "missing_header_brief_case/include/MissingHeaderBriefCase.h";
  ASSERT_TRUE(std::filesystem::exists(fixture));

  std::string out;
  const int code = clang_compile_with_plugin(fixture, lib, out);
  EXPECT_NE(code, 0) << "Expected failure for wrong-case @Brief tag\n" << out;
  EXPECT_NE(out.find("header must start with Doxygen block containing "
                     "@file/@brief and copyright"),
            std::string::npos)
      << out;
}
