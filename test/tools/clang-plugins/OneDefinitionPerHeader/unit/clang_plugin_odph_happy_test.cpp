/**
 * @file clang_plugin_odph_happy_test.cpp
 * @brief OneDefinitionPerHeader: happy path passes.
 */
#include "helpers/plugin_runner.h"
#include <filesystem>
#include <gtest/gtest.h>
#include <string>

TEST(ClangPluginODPH, HappyPasses) {
  std::string log;
  const auto lib = ensure_plugin_built(log);
  ASSERT_FALSE(lib.empty()) << "Plugin build failed: " << log;

  const auto repo = repo_root_from_build_cwd();
  const auto fixture = repo / "test/tools/clang-plugins/OneDefinitionPerHeader/"
                              "fixtures/happy/include/Happy.h";
  ASSERT_TRUE(std::filesystem::exists(fixture));

  std::string out;
  const int code = clang_compile_with_plugin(fixture, lib, out);
  EXPECT_EQ(code, 0) << out;
}
