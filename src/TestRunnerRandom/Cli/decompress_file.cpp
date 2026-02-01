/**
 * @file decompress_file.cpp
 * @brief Run decompress, write artifacts, compute output hash, log, and enforce timeout.
 * @author Sam Caldwell
  * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#include "testRunnerRandom/Cli/detail/decompress_file.h"

#include "testRunnerRandom/detail/proc_result.h"
#include "testRunnerRandom/detail/run_process.h"
#include "testRunnerRandom/detail/files.h"
#include "testRunnerRandom/detail/sha512.h"
#include "testRunnerRandom/detail/log_decompress.h"
#include "testRunnerRandom/detail/json_escape.h"
#include "common/exceptions/DecompressNonZeroExitException.h"
#include "common/exceptions/DecompressTimeoutException.h"

#include <filesystem>
#include <sstream>
#include <string>
#include <cstdint> //NOLINT
#include <optional>
#include <cstdlib>
#include <iostream>
#include <vector>
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

        // Resolve decompressor from build/bin by default, or TEST_BINARY_DIR/bin when set.
        std::string exe;
        if (const char *tbd = std::getenv("TEST_BINARY_DIR"); tbd && *tbd) { // NOLINT(concurrency-mt-unsafe)
            exe = (fs::path(tbd).parent_path() / "bin" / "decompress").string();
        } else {
            exe = (fs::path("bin") / "decompress").string();
        }
        // On POSIX, prefix with env to enable internal solver debug for padding by default
        std::vector<std::string> argv;
#ifdef _WIN32
        argv = { exe, "-in", cx_path.string(), "-out", dx_path.string() };
#else
        // Enable padding prelock debug and more generous solver limits for harder inputs.
        // These can be overridden by the environment by the caller if needed.
        argv = {
            "env",
            "CRSCE_PRELOCK_DEBUG=1",
            "CRSCE_DE_MAX_ITERS=4000",
            "CRSCE_GOBP_ITERS=2000",
            "CRSCE_GOBP_CONF=0.90",
            "CRSCE_GOBP_DAMP=0.30",
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
            std::ostringstream oss; oss << "decompress exited with code " << dx_res.exit_code;
            throw crsce::common::exceptions::DecompressNonZeroExitException(oss.str());
        }

        // Compute output hash and log
        const std::string output_sha512 = crsce::testrunner::detail::compute_sha512(dx_path);
        crsce::testrunner::detail::log_decompress(dx_res, cx_path, dx_path, input_sha512, output_sha512);
        return output_sha512;
    }
}
