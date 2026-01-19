/**
 * @file clang_plugin_odpcpp_missing_header_brief_error_test.cpp
 * @brief Missing @brief in header should error.
 */
#include "helpers/plugin_runner.h"
#include <filesystem>
#include <gtest/gtest.h>
#include <string>

TEST(ClangPluginODPCPP, MissingHeaderBriefFails) {
  std::string log;
  const auto lib = ensure_plugin_built(log);
  ASSERT_FALSE(lib.empty()) << "Plugin build failed: " << log;

  const auto repo = repo_root_from_build_cwd();
  const auto fixture =
      repo / "test/tools/clang-plugins/one-definition-per-cpp-file/fixtures/"
             "sad/missing_header_brief/src/MissingHeaderBrief.cpp";
  ASSERT_TRUE(std::filesystem::exists(fixture));

  std::string out;
  const int code = clang_compile_with_plugin(fixture, lib, out);
  EXPECT_NE(code, 0) << out;
}
