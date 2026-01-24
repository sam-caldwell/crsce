/**
 * @file hello_world_canary.cpp
 * @brief E2E: run the built hello_world binary, capture stdout/stderr, assert exit code.
 */
#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include "helpers/run_command_capture.h"

TEST(HelloWorldE2E, PrintsHelloWorldAndExitZero) {
    // CMake passes TEST_BINARY_DIR to tests; hello_world is emitted alongside tests.
    const auto exe = std::filesystem::path(TEST_BINARY_DIR) / "hello_world";
    ASSERT_TRUE(std::filesystem::exists(exe)) << "hello_world binary not found at " << exe.string();
    int code = 0;
    const std::string out = run_command_capture(exe.string(), code);
    EXPECT_EQ(code, 0) << out;
    EXPECT_NE(out.find("hello world"), std::string::npos) << out;
}
