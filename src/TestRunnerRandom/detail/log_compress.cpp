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
#include "testrunner/detail/proc_result.h"

namespace crsce::testrunner::detail {
    /**
     * @name log_compress
     * @brief Emit a JSON record describing the compress step.
     * @param res Subprocess result data.
     * @param input Input file path.
     * @param output Output container path.
     * @param input_hash SHA-512 digest of the input.
     */
    void log_compress(const ProcResult &res,
                      const std::filesystem::path &input,
                      const std::filesystem::path &output,
                      const std::string &input_hash) {
        const auto elapsed = res.end_ms - res.start_ms;
        std::cout << "{\n"
                  << "  \"step\":\"compress\",\n"
                  << "  \"start\":" << res.start_ms << ",\n"
                  << "  \"end\":" << res.end_ms << ",\n"
                  << "  \"input\":\"" << json_escape(input.string()) << "\",\n"
                  << "  \"hashInput\":\"" << input_hash << "\",\n"
                  << "  \"output\":\"" << json_escape(output.string()) << "\",\n"
                  << "  \"compressTime\":" << elapsed << "\n"
                  << "}\n";
        std::cout.flush();
    }
}
