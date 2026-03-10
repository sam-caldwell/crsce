/**
 * @file decompress_file.cpp
 * @brief Run decompress, write artifacts, compute output hash, log, and enforce timeout.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#include "testRunnerRandom/Cli/decompress_file.h"

#include "testRunnerRandom/proc_result.h"
#include "testRunnerRandom/run_process.h"
#include "testRunnerRandom/files.h"
#include "testRunnerRandom/sha512.h"
#include "testRunnerRandom/log_decompress.h"
#include "testRunnerRandom/resolve_exe.h"
#include "testRunnerRandom/json_escape.h"
#include "common/exceptions/DecompressNonZeroExitException.h"
#include "common/exceptions/DecompressTimeoutException.h"

#include <filesystem>
#include <sstream>
#include <string>
#include <format>
#include <cstdint> //NOLINT
#include <optional>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <fstream>
#include <system_error>

namespace fs = std::filesystem;

namespace crsce::testrunner_random::cli {
    /**
     * @name decompress_file
     * @brief Invoke decompress, write stdio artifacts, compute SHA-512 of output, and log.
     * @param cx_path Path to the CRSCE container to decompress.
     * @param dx_path Destination path for the reconstructed output file.
     * @param input_sha512 SHA-512 of the original input (for logging and validation context).
     * @param timeout_ms Maximum allowed elapsed time in milliseconds.
     * @return SHA-512 digest string of the reconstructed output file.
     */
    std::string decompress_file(const std::filesystem::path &cx_path,
                                const std::filesystem::path &dx_path,
                                const std::string &input_sha512,
                                const std::int64_t timeout_ms) {
        using crsce::testrunner::detail::run_process;
        using crsce::testrunner::detail::ProcResult;

        const std::string exe = crsce::testrunner::detail::resolve_exe("decompress");
        // On POSIX, prefix with env to enable internal solver debug for padding by default
        std::vector<std::string> argv;
#ifdef _WIN32
        argv = { exe, "-in", cx_path.string(), "-out", dx_path.string() };
#else
        // Enable padding prelock debug. GOBP schedule is fixed in code; no env overrides.
        argv = {
            "env",
            "CRSCE_PRELOCK_DEBUG=1",
            "CRSCE_DE_MAX_ITERS=4000",
            exe,
            "-in", cx_path.string(), "-out", dx_path.string()
        };
#endif
        // Debug: show command being executed
        std::ostringstream dbg;
        for (std::size_t i = 0; i < argv.size(); ++i) {
            if (i) { dbg << ' '; }
            dbg << argv[i];
        }
        std::cout << "[testrunner] running: " << dbg.str() << "\n";
        const ProcResult dx_res = run_process(argv, std::nullopt);

        // Persist child stdio regardless of success/failure
        crsce::testrunner::detail::write_file_text(fs::path(cx_path).replace_extension(".decompress.stdout.txt"), dx_res.out);
        crsce::testrunner::detail::write_file_text(fs::path(cx_path).replace_extension(".decompress.stderr.txt"), dx_res.err);

        // Echo child stdio to parent stdout for easier CI debugging
        if (!dx_res.out.empty()) {
            std::cout << "[testrunner] decompress.stdout:\n" << dx_res.out << '\n';
        }
        if (!dx_res.err.empty()) {
            std::cout << "[testrunner] decompress.stderr:\n" << dx_res.err << '\n';
        }

        // Enforce timeout first, prior to expensive hashing
        const auto elapsed = dx_res.end_ms - dx_res.start_ms;
        if (elapsed > timeout_ms || dx_res.exit_code != 0) {
            std::error_code ec_osz; const auto out_sz = std::filesystem::file_size(dx_path, ec_osz);
            std::ostringstream cmd;
            for (std::size_t i = 0; i < argv.size(); ++i) { if (i) { cmd << ' '; } cmd << argv[i]; }
            // Emit error context first
            std::cout << "{\n"
                      << "  \"step\":\"error\",\n"
                      << "  \"subStep\":\"decompress\",\n"
                      << "  \"start\":" << dx_res.start_ms << ",\n"
                      << "  \"end\":" << dx_res.end_ms << ",\n"
                      << "  \"elapsed\":" << elapsed << ",\n"
                      << "  \"timeoutMs\":" << timeout_ms << ",\n"
                      << "  \"exitCode\":" << dx_res.exit_code << ",\n"
                      << "  \"command\":\"" << crsce::testrunner::detail::json_escape(cmd.str()) << "\",\n"
                      << "  \"input\":\"" << crsce::testrunner::detail::json_escape(cx_path.string()) << "\",\n"
                      << "  \"output\":\"" << crsce::testrunner::detail::json_escape(dx_path.string()) << "\",\n"
                      << "  \"decompressedSize\":" << (ec_osz ? 0ULL : static_cast<std::uint64_t>(out_sz)) << ",\n"
                      << "  \"hashInput\":\"" << input_sha512 << "\",\n"
                      << "  \"stderr\":\"" << crsce::testrunner::detail::json_escape(dx_res.err) << "\"\n"
                      << "}\n";
            // Then emit the child's failure JSON (unescaped) as its own log record if present
            if (!dx_res.out.empty()) {
                std::cout << dx_res.out;
                if (dx_res.out.back() != '\n') { std::cout << '\n'; }
            }
            std::cout.flush();
            if (elapsed > timeout_ms) {
                throw crsce::common::exceptions::DecompressTimeoutException("decompress timed out");
            }
            throw crsce::common::exceptions::DecompressNonZeroExitException(
                std::format("decompress exited with code {}", dx_res.exit_code));
        }

        // Compute output hash and extract gobp cells solved from completion_stats.log
        const std::string output_sha512 = crsce::testrunner::detail::compute_sha512(dx_path);
        std::uint64_t gobp_cells_solved = 0;
        try {
            namespace fs = std::filesystem;
            const fs::path outdir = fs::path(dx_path).has_parent_path() ? fs::path(dx_path).parent_path() : fs::path(".");
            const fs::path rowlog = outdir / "completion_stats.log";
            std::ifstream is(rowlog);
            if (is.good()) {
                std::string line;
                std::string best;
                while (std::getline(is, line)) {
                    if (line.find("\"gobp_cells_solved\"") != std::string::npos) { best = line; }
                }
                if (!best.empty()) {
                    auto pos = best.find(':');
                    if (pos != std::string::npos) {
                        std::string num = best.substr(pos + 1);
                        // Trim trailing comma/space
                        while (!num.empty() && (num.back() == ',' || num.back() == ' ' || num.back() == '\n' || num.back()=='\r')) { num.pop_back(); }
                        // Trim leading space
                        while (!num.empty() && (num.front()==' ')) { num.erase(num.begin()); }
                        try {
                            gobp_cells_solved = static_cast<std::uint64_t>(std::stoull(num));
                        } catch (...) { gobp_cells_solved = 0; }
                    }
                }
            }
        } catch (...) {
            gobp_cells_solved = 0;
        }
        crsce::testrunner::detail::log_decompress(dx_res, cx_path, dx_path, input_sha512, output_sha512, gobp_cells_solved);
        return output_sha512;
    }
}
