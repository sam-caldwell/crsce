/**
 * @file log_decompress.cpp
 * @brief JSON logging for the decompress step.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "testrunner/detail/log_decompress.h"
#include "testrunner/detail/json_escape.h"
#include <iostream>
#include <filesystem>
#include <string>
#include "testrunner/detail/proc_result.h"

namespace crsce::testrunner::detail {
    /**
     * @name log_decompress
     * @brief Emit a JSON record describing the decompress step.
     * @param res Subprocess result data.
     * @param input Input container path.
     * @param output Reconstructed output file path.
     * @param input_hash SHA-512 digest of the input.
     * @param output_hash SHA-512 digest of the reconstructed output.
     * @return void
     */
    void log_decompress(const ProcResult &res,
                        const std::filesystem::path &input,
                        const std::filesystem::path &output,
                        const std::string &input_hash,
                        const std::string &output_hash) {
        const auto elapsed = res.end_ms - res.start_ms;
        std::cout << "{\n"
                  << "  \"step\":\"decompress\",\n"
                  << "  \"start\":" << res.start_ms << ",\n"
                  << "  \"end\":" << res.end_ms << ",\n"
                  << "  \"input\":\"" << json_escape(input.string()) << "\",\n"
                  << "  \"output\":\"" << json_escape(output.string()) << "\",\n"
                  << "  \"decompressTime\":" << elapsed << ",\n"
                  << "  \"hashInput\":\"" << input_hash << "\",\n"
                  << "  \"hashOutput\":\"" << output_hash << "\"\n"
                  << "}\n";
        std::cout.flush();
    }
}
