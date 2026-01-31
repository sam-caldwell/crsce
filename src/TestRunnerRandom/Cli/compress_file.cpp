/**
 * @file compress_file.cpp
 * @brief Run compress, write artifacts, log, and enforce timeout.
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "testrunner/Cli/detail/compress_file.h"

#include "testrunner/detail/proc_result.h"
#include "testrunner/detail/run_process.h"
#include "testrunner/detail/log_compress.h"
#include "testrunner/detail/json_escape.h"
#include "testrunner/detail/files.h"
#include "common/exceptions/CompressNonZeroExitException.h"
#include "common/exceptions/CompressTimeoutException.h"

#include <filesystem>
#include <sstream>
#include <string>
#include <cstdint>
#include <optional>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <system_error>

namespace fs = std::filesystem;

namespace crsce::testrunner::cli {
    /**
     * @name compress_file
     * @brief Invoke compress, write stdio artifacts, log, and enforce timeout.
     * @param in_path Path of the input file to compress.
     * @param cx_path Destination path for the generated CRSCE container.
     * @param input_sha512 SHA-512 digest of the input file (used for logging).
     * @param timeout_ms Maximum allowed elapsed time in milliseconds.
     * @return ProcResult describing the compress subprocess execution.
     */
    crsce::testrunner::detail::ProcResult compress_file(const std::filesystem::path &in_path,
                                                        const std::filesystem::path &cx_path,
                                                        const std::string &input_sha512,
                                                        const std::int64_t timeout_ms) {
        using crsce::testrunner::detail::ProcResult;
        using crsce::testrunner::detail::run_process;

        std::string exe;
        if (fs::exists("compress")) {
            exe = "./compress";
        } else {
            const char *tbd = std::getenv("TEST_BINARY_DIR"); // NOLINT(concurrency-mt-unsafe)
            exe = (tbd && *tbd) ? (std::string(tbd) + "/compress") : std::string("compress");
        }
        // Debug: show command being executed
        std::cout << "[testrunner] running: " << exe
                  << " -in " << in_path.string() << " -out " << cx_path.string() << "\n";
        const std::vector<std::string> argv = { exe, "-in", in_path.string(), "-out", cx_path.string() };
        const ProcResult cx_res = run_process(argv, std::nullopt);

        // Persist child stdio regardless of success/failure
        crsce::testrunner::detail::write_file_text(fs::path(cx_path).replace_extension(".compress.stdout.txt"), cx_res.out);
        crsce::testrunner::detail::write_file_text(fs::path(cx_path).replace_extension(".compress.stderr.txt"), cx_res.err);

        // Echo child stdio to parent stdout for easier CI debugging
        if (!cx_res.out.empty()) {
            std::cout << "[testrunner] compress.stdout:\n" << cx_res.out << '\n';
        }
        if (!cx_res.err.empty()) {
            std::cout << "[testrunner] compress.stderr:\n" << cx_res.err << '\n';
        }

        // Emit JSON log
        crsce::testrunner::detail::log_compress(cx_res, in_path, cx_path, input_sha512);

        // Enforce timeout
        const auto elapsed = cx_res.end_ms - cx_res.start_ms;
        if (elapsed > timeout_ms || cx_res.exit_code != 0) {
            // Emit structured error JSON with as much context as possible
            std::error_code ec_isz; const auto in_sz = std::filesystem::file_size(in_path, ec_isz);
            const std::uint64_t raw_bytes = ec_isz ? 0ULL : static_cast<std::uint64_t>(in_sz);
            static constexpr std::uint64_t S = 511ULL;
            const std::uint64_t bits_per_block = S * S;
            const std::uint64_t bits = raw_bytes * 8ULL;
            const std::uint64_t blocks = (bits == 0ULL) ? 0ULL : ((bits + bits_per_block - 1ULL) / bits_per_block);
            const std::uint64_t padded_bits = blocks * bits_per_block;
            const std::uint64_t padded_bytes = (padded_bits + 7ULL) / 8ULL;
            const std::uint64_t padding_bytes = (padded_bytes > raw_bytes) ? (padded_bytes - raw_bytes) : 0ULL;
            std::ostringstream cmd;
            for (std::size_t i = 0; i < argv.size(); ++i) { if (i) { cmd << ' '; } cmd << argv[i]; }
            std::cout << "{\n"
                      << "  \\\"step\\\":\\\"error\\\",\n"
                      << "  \\\"subStep\\\":\\\"compress\\\",\n"
                      << "  \\\"start\\\":" << cx_res.start_ms << ",\n"
                      << "  \\\"end\\\":" << cx_res.end_ms << ",\n"
                      << "  \\\"elapsed\\\":" << elapsed << ",\n"
                      << "  \\\"timeoutMs\\\":" << timeout_ms << ",\n"
                      << "  \\\"exitCode\\\":" << cx_res.exit_code << ",\n"
                      << "  \\\"command\\\":\\\"" << crsce::testrunner::detail::json_escape(cmd.str()) << "\\\",\n"
                      << "  \\\"input\\\":\\\"" << crsce::testrunner::detail::json_escape(in_path.string()) << "\\\",\n"
                      << "  \\\"output\\\":\\\"" << crsce::testrunner::detail::json_escape(cx_path.string()) << "\\\",\n"
                      << "  \\\"hashInput\\\":\\\"" << input_sha512 << "\\\",\n"
                      << "  \\\"rawInputSize\\\":" << raw_bytes << ",\n"
                      << "  \\\"paddingSize\\\":" << padding_bytes << ",\n"
                      << "  \\\"inputSize\\\":" << padded_bytes << ",\n"
                      << "  \\\"stdout\\\":\\\"" << crsce::testrunner::detail::json_escape(cx_res.out) << "\\\",\n"
                      << "  \\\"stderr\\\":\\\"" << crsce::testrunner::detail::json_escape(cx_res.err) << "\\\"\n"
                      << "}\n";
            std::cout.flush();
            if (elapsed > timeout_ms) {
                throw crsce::common::exceptions::CompressTimeoutException("compress timed out");
            }
            std::ostringstream oss; oss << "compress exited with code " << cx_res.exit_code;
            throw crsce::common::exceptions::CompressNonZeroExitException(oss.str());
        }
        return cx_res;
    }
}
