/**
 * @file make_input.cpp
 * @brief Implement make_input: compute SHA-512, blocks, and log for an existing file.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#include "testrunner/Cli/detail/make_input.h"

#include "testrunner/Cli/detail/generated_input.h"
#include "testrunner/detail/now_ms.h"
#include "testrunner/detail/sha512.h"
#include "testrunner/detail/log_hash_input.h"
#include "testrunner/detail/blocks_for_input_bytes.h"
#include <filesystem>
#include <cstdint>
#include <string>

namespace crsce::testrunner::cli {
    /**
     * @name make_input
     * @brief Compute metadata for an existing input file and log the hash timing.
     * @param in_path Absolute path of the input file.
     * @param in_bytes Size of the input file in bytes (precomputed by caller).
     * @return GeneratedInput containing path, bytes, blocks, and sha512.
     */
    GeneratedInput make_input(const std::filesystem::path &in_path, const std::uint64_t in_bytes) {
        const auto hash_start = detail::now_ms();
        const std::string input_sha512 = detail::compute_sha512(in_path);
        detail::log_hash_input(hash_start, detail::now_ms(), in_path, input_sha512);

        GeneratedInput gi{};
        gi.path = in_path;
        gi.bytes = in_bytes;
        gi.blocks = detail::blocks_for_input_bytes(in_bytes);
        if (gi.blocks == 0) { gi.blocks = 1; }
        gi.sha512 = input_sha512;
        return gi;
    }
}
