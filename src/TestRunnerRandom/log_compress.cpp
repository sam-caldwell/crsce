/**
 * @file log_compress.cpp
 * @brief JSON logging for the compress step.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "testRunnerRandom/log_compress.h"
#include "testRunnerRandom/json_escape.h"
#include <iostream>
#include <filesystem>
#include <string>
#include <system_error>
#include <cstdint>
#include "testRunnerRandom/proc_result.h"

namespace crsce::testrunner::detail {
    /**
     * @name log_compress
     * @brief Emit a JSON record describing the compress step.
     * @param res Subprocess result data.
     * @param input Input file path.
     * @param output Output container path.
     * @param input_hash SHA-512 digest of the input.
     * @return void
     */
    void log_compress(const ProcResult &res,
                      const std::filesystem::path &input,
                      const std::filesystem::path &output,
                      const std::string &input_hash) {
        const auto elapsed = res.end_ms - res.start_ms;
        std::error_code ec_isz;
        std::error_code ec_osz;
        const auto in_sz = std::filesystem::file_size(input, ec_isz);
        const auto out_sz = std::filesystem::file_size(output, ec_osz);
        // Compute padded input size in bytes for full CRSCE blocks (511x511 bits per block)
        const std::uint64_t raw_bytes = ec_isz ? 0ULL : static_cast<std::uint64_t>(in_sz);
        static constexpr std::uint64_t S = 127ULL;
        const std::uint64_t bits_per_block = S * S;
        const std::uint64_t bits = raw_bytes * 8ULL;
        const std::uint64_t blocks = (bits == 0ULL) ? 0ULL : ((bits + bits_per_block - 1ULL) / bits_per_block);
        const std::uint64_t padded_bits = blocks * bits_per_block;
        const std::uint64_t padded_bytes = (padded_bits + 7ULL) / 8ULL;
        const std::uint64_t padding_bytes = (padded_bytes > raw_bytes) ? (padded_bytes - raw_bytes) : 0ULL;
        std::cout << "{\n"
                  << "  \"step\":\"compress\",\n"
                  << "  \"start\":" << res.start_ms << ",\n"
                  << "  \"end\":" << res.end_ms << ",\n"
                  << "  \"input\":\"" << json_escape(input.string()) << "\",\n"
                  << "  \"hashInput\":\"" << input_hash << "\",\n"
                  << "  \"output\":\"" << json_escape(output.string()) << "\",\n"
                  << "  \"rawInputSize\":" << raw_bytes << ",\n"
                  << "  \"paddingSize\":" << padding_bytes << ",\n"
                  << "  \"inputSize\":" << padded_bytes << ",\n"
                  << "  \"compressedSize\":" << (ec_osz ? 0 : static_cast<std::uint64_t>(out_sz)) << ",\n"
                  << "  \"compressTime\":" << elapsed << "\n"
                  << "}\n";
        std::cout.flush();
    }
}
