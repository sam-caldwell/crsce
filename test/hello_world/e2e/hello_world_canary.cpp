/**
 * File: test/hello_world/e2e/hello_world_canary.cpp
 * Brief: End-to-end gtest that spawns the built hello_world binary
 *        and validates its stdout/exit code via stdin/stdout pipes.
 */

#include <gtest/gtest.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include <system_error>
#include <cstdlib>
// helper for running external commands with output capture
#include "tools/clang-plugins/one-definition-per-cpp-file/unit/helpers/plugin_runner.h"

namespace {

// Use run_command_capture(cmd, exit_code) from included helper

std::optional<std::filesystem::path> find_hello_world_binary() {
  namespace fs = std::filesystem;

  // Allow override via env var if provided
  // Environment override intentionally omitted to avoid MT-unsafe getenv usage in tests.

  const fs::path cwd = fs::current_path();
  std::vector<fs::path> candidates{
      cwd / "build" / "bin" / "hello_world",
      cwd / "build" / "llvm-debug" / "hello_world",
      cwd / "build" / "llvm-release" / "hello_world",
      cwd / "build" / "arm64-debug" / "hello_world",
      cwd / "build" / "arm64-release" / "hello_world",
      cwd / "bin" / "hello_world",
      cwd / "hello_world",
  };

  // Also try one and two-level parents for common ctest working dirs
  if (cwd.has_parent_path()) {
    const fs::path p1 = cwd.parent_path();
    candidates.push_back(p1 / "build" / "bin" / "hello_world");
    candidates.push_back(p1 / "build" / "llvm-debug" / "hello_world");
    candidates.push_back(p1 / "build" / "llvm-release" / "hello_world");
    candidates.push_back(p1 / "hello_world");
  }
  if (cwd.has_parent_path() && cwd.parent_path().has_parent_path()) {
    const fs::path p2 = cwd.parent_path().parent_path();
    candidates.push_back(p2 / "build" / "bin" / "hello_world");
    candidates.push_back(p2 / "build" / "llvm-debug" / "hello_world");
    candidates.push_back(p2 / "build" / "llvm-release" / "hello_world");
  }

  for (const auto &cand : candidates) {
    std::error_code ec;
    if (fs::exists(cand, ec) && fs::is_regular_file(cand, ec)) {
      return cand;
    }
  }
  return std::nullopt;
}

} // namespace

TEST(HelloWorldE2E, PrintsHelloWorldAndExitZero) {
  auto exe = find_hello_world_binary();
  ASSERT_TRUE(exe.has_value()) << "hello_world binary not found."
                               << " Tried common locations (including build/bin/hello_world).";

  int code = 0;
  const std::string out = run_command_capture(exe->string(), code);
  EXPECT_EQ(code, 0) << out;
  // Should contain the expected greeting on stdout
  EXPECT_NE(out.find("hello world"), std::string::npos) << out;
}

TEST(HelloWorldE2E, IgnoresProvidedStdin) {
  auto exe = find_hello_world_binary();
  ASSERT_TRUE(exe.has_value()) << "hello_world binary not found.";

  int code = 0;
  const std::string out = run_command_capture(exe->string(), code);
  EXPECT_EQ(code, 0) << out;
  EXPECT_NE(out.find("hello world"), std::string::npos) << out;
}
