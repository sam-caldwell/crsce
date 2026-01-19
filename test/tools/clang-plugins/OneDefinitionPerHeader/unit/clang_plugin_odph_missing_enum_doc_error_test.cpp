/**
 * @file clang_plugin_odph_missing_enum_doc_error_test.cpp
 * @brief OneDefinitionPerHeader: missing enum doc should fail.
 */
#include "helpers/plugin_runner.h"
#include <filesystem>
#include <gtest/gtest.h>
#include <string>

TEST(ClangPluginODPH, MissingEnumDocFails) {
  std::string log;
  const auto lib = ensure_plugin_built(log);
  ASSERT_FALSE(lib.empty()) << "Plugin build failed: " << log;

  const auto repo = repo_root_from_build_cwd();
  const auto fixture =
      repo / "test/tools/clang-plugins/OneDefinitionPerHeader/fixtures/sad/"
             "missing_construct_doc/include/MissingEnumDoc.h";
  ASSERT_TRUE(std::filesystem::exists(fixture));

  std::string out;
  const int code = clang_compile_with_plugin(fixture, lib, out);
  EXPECT_NE(code, 0) << "Expected failure for missing enum doc\n" << out;
  EXPECT_NE(
      out.find("definition must be immediately preceded by a Doxygen block"),
      std::string::npos)
      << out;
}
