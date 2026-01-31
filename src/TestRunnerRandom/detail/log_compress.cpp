/**
 * @file log_compress.cpp
 * @brief JSON logging for the compress step.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "testrunner/detail/log_compress.h"
#include "testrunner/detail/json_escape.h"
#include <iostream>
#include <filesystem>
#include <string>
#include <system_error>
#include <cstdint>
#include "testrunner/detail/proc_result.h"

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
        std::cout << "{\n"
                  << "  \"step\":\"compress\",\n"
                  << "  \"start\":" << res.start_ms << ",\n"
                  << "  \"end\":" << res.end_ms << ",\n"
                  << "  \"input\":\"" << json_escape(input.string()) << "\",\n"
                  << "  \"hashInput\":\"" << input_hash << "\",\n"
                  << "  \"output\":\"" << json_escape(output.string()) << "\",\n"
                  << "  \"inputSize\":" << (ec_isz ? 0 : static_cast<std::uint64_t>(in_sz)) << ",\n"
                  << "  \"compressedSize\":" << (ec_osz ? 0 : static_cast<std::uint64_t>(out_sz)) << ",\n"
                  << "  \"compressTime\":" << elapsed << "\n"
                  << "}\n";
        std::cout.flush();
    }
}
