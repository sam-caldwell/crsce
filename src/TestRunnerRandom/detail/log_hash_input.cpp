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
        // Compute padded input size in bytes for full CRSCE blocks (511x511 bits per block)
        const std::uint64_t raw_bytes = ec_isz ? 0ULL : static_cast<std::uint64_t>(in_sz);
        static constexpr std::uint64_t S = 511ULL;
        const std::uint64_t bits_per_block = S * S;
        const std::uint64_t bits = raw_bytes * 8ULL;
        const std::uint64_t blocks = (bits == 0ULL) ? 0ULL : ((bits + bits_per_block - 1ULL) / bits_per_block);
        const std::uint64_t padded_bits = blocks * bits_per_block;
        const std::uint64_t padded_bytes = (padded_bits + 7ULL) / 8ULL;
        const std::uint64_t padding_bytes = (padded_bytes > raw_bytes) ? (padded_bytes - raw_bytes) : 0ULL;
        std::cout << "{\n"
                  << "  \"step\":\"hashInput\",\n"
                  << "  \"start\":" << start_ms << ",\n"
                  << "  \"end\":" << end_ms << ",\n"
                  << "  \"hashInput\":\"" << json_escape(input.string()) << "\",\n"
                  << "  \"rawInputSize\":" << raw_bytes << ",\n"
                  << "  \"paddingSize\":" << padding_bytes << ",\n"
                  << "  \"inputSize\":" << padded_bytes << ",\n"
                  << "  \"hash\":\"" << hash << "\"\n"
                  << "}\n";
        std::cout.flush();
    }
}
