/**
 * @file log_hash_input.cpp
 * @brief JSON logging for the input-hash step.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "testrunner/detail/log_hash_input.h"
#include "testrunner/detail/json_escape.h"
#include <iostream>
#include <cstdint>
#include <filesystem>
#include <string>
#include <system_error>

namespace crsce::testrunner::detail {
    /**
     * @name log_hash_input
     * @brief Emit a JSON record describing the input-hash step.
     * @param start_ms Step start time.
     * @param end_ms Step end time.
     * @param input Input file path.
     * @param hash SHA-512 digest of the input.
     * @return void
     */
    void log_hash_input(const std::int64_t start_ms, const std::int64_t end_ms,
                        const std::filesystem::path &input, const std::string &hash) {
        std::error_code ec_isz;
        const auto in_sz = std::filesystem::file_size(input, ec_isz);
        std::cout << "{\n"
                  << "  \"step\":\"hashInput\",\n"
                  << "  \"start\":" << start_ms << ",\n"
                  << "  \"end\":" << end_ms << ",\n"
                  << "  \"hashInput\":\"" << json_escape(input.string()) << "\",\n"
                  << "  \"inputSize\":" << (ec_isz ? 0 : static_cast<std::uint64_t>(in_sz)) << ",\n"
                  << "  \"hash\":\"" << hash << "\"\n"
                  << "}\n";
        std::cout.flush();
    }
}
