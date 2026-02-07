/**
 * @file ComputeControlSha256.cpp
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "common/HasherUtils/ComputeControlSha256.h"
#include "common/HasherUtils/detail/run_sha256_stdin.h"

#include <cstdlib>
#include "common/O11y/metric.h"
#include <vector>
#include <string>
#include <cstdint>

namespace crsce::common::hasher {
    /**
     * @name compute_control_sha256
     * @brief Compute the SHA-256 digest (hex) using an external tool.
     * @param data Input bytes to hash.
     * @param hex_out Output string to receive lowercase hex digest.
     * @return bool True on success; false if no tool could run or failed.
     */
    bool compute_control_sha256(const std::vector<std::uint8_t> &data,
                                std::string &hex_out) {
#if defined(__unix__) || defined(__APPLE__)
        if (const char *tool = std::getenv("CRSCE_HASHER_CMD"); tool && *tool) { // NOLINT(concurrency-mt-unsafe)
            if (!run_sha256_stdin({tool}, data, hex_out)) {
                ::crsce::o11y::event("hasher_error", {{"cause", std::string("CRSCE_HASHER_CMD_failed")}});
                return false;
            }
            return true;
        }

        if (const char *candidate = std::getenv("CRSCE_HASHER_CANDIDATE"); candidate && *candidate) { // NOLINT(concurrency-mt-unsafe)
            if (!run_sha256_stdin({candidate}, data, hex_out)) {
                ::crsce::o11y::event("hasher_error", {{"cause", std::string("candidate_override_failed")}});
                return false;
            }
            return true;
        }

        const std::vector<std::vector<std::string> > candidates{
            {"/usr/bin/sha256sum", "-"},
            {"/usr/local/bin/sha256sum", "-"},
            {"/bin/sha256sum", "-"},
            {"/sbin/sha256sum", "-"},
            {"/opt/homebrew/bin/sha256sum", "-"},
            {"/usr/bin/shasum", "-a", "256", "-"},
            {"/opt/homebrew/bin/shasum", "-a", "256", "-"}
        };
        for (const auto &cmd: candidates) {
            if (run_sha256_stdin(cmd, data, hex_out)) {
                return true;
            }
        }
        ::crsce::o11y::event("hasher_error", {{"cause", std::string("sha256sum_shasum_not_found")}});
        return false;
#else
        (void) data;
        (void) hex_out;
        ::crsce::o11y::event("hasher_error", {{"cause", std::string("unsupported_platform")}});
        return false;
#endif
    }
} // namespace crsce::common::hasher
