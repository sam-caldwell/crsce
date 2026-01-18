/**
 * @file clang_plugin_one_test_per_file_happy_test.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include <gtest/gtest.h>
#include "helpers/plugin_runner.h"

TEST(ClangPluginOneTestPerFile, HappyPasses) {
  std::string log;
  const auto lib = ensure_plugin_built(log);
  ASSERT_FALSE(lib.empty()) << "Plugin build failed: " << log;

  const auto repo = repo_root_from_build_cwd();
  const auto fixture = repo / "test/tools/OneTestPerFile/fixtures/happy/one_test_and_doc_ok_test.cpp";
  ASSERT_TRUE(std::filesystem::exists(fixture));

  std::string out;
  int code = clang_compile_with_plugin(fixture, lib, out);
  EXPECT_EQ(code, 0) << out;
}
