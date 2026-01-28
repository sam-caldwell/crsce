/**
 * @file hasher_binary_ok_with_real_tool_test.cpp
 * @brief Unit: run hasher binary using a real system tool (sha256sum/shasum) when present.
 */
#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>
#include <system_error>

namespace fs = std::filesystem;

/**
 * @name HasherBinary.OkWithRealTool
 * @brief If a real system SHA-256 tool is available, ensure hasher exits 0.
 */
TEST(HasherBinary, OkWithRealTool) { // NOLINT(readability-identifier-naming)
    // Mirror the candidate list used by compute_control_sha256 (paths only)
    const std::vector<std::string> candidates = {
        "/usr/bin/sha256sum",
        "/usr/local/bin/sha256sum",
        "/bin/sha256sum",
        "/sbin/sha256sum",
        "/opt/homebrew/bin/sha256sum",
        "/usr/bin/shasum",
        "/opt/homebrew/bin/shasum"
    };

    bool have_real_tool = false;
    for (const auto &p : candidates) {
        std::error_code ec;
        const bool exists = fs::exists(p, ec) && !ec;
        if (exists) {
            have_real_tool = true;
            break;
        }
    }
    if (!have_real_tool) {
        GTEST_SKIP() << "No system sha256sum/shasum found; skipping real tool validation.";
    }

    // Ensure overrides are not set so the binary uses the system tool discovery
    (void) ::unsetenv("CRSCE_HASHER_CMD");        // NOLINT(concurrency-mt-unsafe,misc-include-cleaner)
    (void) ::unsetenv("CRSCE_HASHER_CANDIDATE"); // NOLINT(concurrency-mt-unsafe,misc-include-cleaner)

    const std::string exe = std::string(TEST_BINARY_DIR) + "/hasher";
    const int rc = std::system(exe.c_str()); // NOLINT(concurrency-mt-unsafe)
    EXPECT_EQ(rc, 0) << "hasher should exit 0 using real system tool";
}
