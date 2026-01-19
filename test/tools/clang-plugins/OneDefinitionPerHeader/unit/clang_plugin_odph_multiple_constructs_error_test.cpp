/**
 * @file clang_plugin_odph_multiple_constructs_error_test.cpp
 * @brief OneDefinitionPerHeader: multiple constructs should fail.
 */
#include "helpers/plugin_runner.h"
#include <filesystem>
#include <gtest/gtest.h>
#include <string>

TEST(ClangPluginODPH, MultipleConstructsFail) {
  std::string log;
  const auto lib = ensure_plugin_built(log);
  ASSERT_FALSE(lib.empty()) << "Plugin build failed: " << log;

  const auto repo = repo_root_from_build_cwd();
  const auto fixture =
      repo / "test/tools/clang-plugins/OneDefinitionPerHeader/fixtures/sad/"
             "multiple_constructs/include/Multiple.h";
  ASSERT_TRUE(std::filesystem::exists(fixture));

  std::string out;
  const int code = clang_compile_with_plugin(fixture, lib, out);
  EXPECT_NE(code, 0) << "Expected failure for multiple constructs\n" << out;
  EXPECT_NE(out.find("exactly one construct must be defined per header"),
            std::string::npos)
      << out;
}
