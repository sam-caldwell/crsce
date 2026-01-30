/**
 * @file sha512.cpp
 * @brief Compute SHA-512 via external tools.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "testrunner/detail/sha512.h"
#include "testrunner/detail/run_process.h"
#include "common/exceptions/Sha512UnavailableException.h"
#include <optional>
#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

namespace crsce::testrunner::detail {
    // Helper kept inline in compute_sha512 to avoid introducing another construct in this TU.
    /**
     * @name compute_sha512
     * @brief Compute SHA-512 for a file using system tools (sha512sum/shasum -a 512).
     * @param file Path to input file.
     * @return Lowercase hex digest string.
     */
    std::string compute_sha512(const fs::path &file) {
        const std::vector<std::vector<std::string> > cmds = {
            {"sha512sum", file.string()},
            {"shasum", "-a", "512", file.string()},
        };
        for (const auto &cmd: cmds) {
            const auto res = run_process(cmd, std::nullopt);
            if (res.exit_code != 0 || res.out.empty()) { continue; }
            // Extract first token
            std::string tok; tok.reserve(128);
            for (const char ch: res.out) { if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '*') { break; } tok.push_back(ch); }
            if (!tok.empty()) { return tok; }
        }
        throw crsce::common::exceptions::Sha512UnavailableException("failed to compute SHA-512; neither sha512sum nor shasum found");
    }
}
