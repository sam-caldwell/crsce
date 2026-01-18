/**
 * @file clang_plugin_one_test_per_file_header_missing_error_test.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include <gtest/gtest.h>
#include "helpers/plugin_runner.h"

TEST(ClangPluginOneTestPerFile, MissingHeaderFails) {
  std::string log;
  const auto lib = ensure_plugin_built(log);
  ASSERT_FALSE(lib.empty()) << "Plugin build failed: " << log;

  const auto repo = repo_root_from_build_cwd();
  const auto fixture = repo / "test/tools/fixtures/OneTestPerFile/sad/missing_header/missing_header_fail_test.cpp";
  ASSERT_TRUE(std::filesystem::exists(fixture));

  std::string out;
  int code = clang_compile_with_plugin(fixture, lib, out);
  EXPECT_NE(code, 0) << "Expected failure for missing header docstring";
  EXPECT_NE(out.find("test file must start with the required docstring header"), std::string::npos) << out;
}
