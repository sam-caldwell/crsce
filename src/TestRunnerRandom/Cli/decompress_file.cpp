/**
 * @file decompress_file.cpp
 * @brief Run decompress, write artifacts, compute output hash, log, and enforce timeout.
 * @author Sam Caldwell
 * © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "testrunner/Cli/detail/decompress_file.h"

#include "testrunner/detail/proc_result.h"
#include "testrunner/detail/run_process.h"
#include "testrunner/detail/files.h"
#include "testrunner/detail/sha512.h"
#include "testrunner/detail/log_decompress.h"
#include "common/exceptions/DecompressNonZeroExitException.h"
#include "common/exceptions/DecompressTimeoutException.h"

#include <filesystem>
#include <sstream>
#include <string>
#include <cstdint>
#include <optional>
#include <cstdlib>

namespace fs = std::filesystem;

namespace crsce::testrunner::cli {
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

        std::string exe;
        if (fs::exists("decompress")) {
            exe = "./decompress";
        } else {
            const char *tbd = std::getenv("TEST_BINARY_DIR"); // NOLINT(concurrency-mt-unsafe)
            exe = (tbd && *tbd) ? (std::string(tbd) + "/decompress") : std::string("decompress");
        }
        const ProcResult dx_res = run_process({ exe, "-in", cx_path.string(), "-out", dx_path.string() }, std::nullopt);

        // Persist child stdio regardless of success/failure
        crsce::testrunner::detail::write_file_text(fs::path(cx_path).replace_extension(".decompress.stdout.txt"), dx_res.out);
        crsce::testrunner::detail::write_file_text(fs::path(cx_path).replace_extension(".decompress.stderr.txt"), dx_res.err);

        // Enforce timeout first, prior to expensive hashing
        const auto elapsed = dx_res.end_ms - dx_res.start_ms;
        if (elapsed > timeout_ms) {
            throw crsce::common::exceptions::DecompressTimeoutException("decompress timed out");
        }
        if (dx_res.exit_code != 0) {
            std::ostringstream oss; oss << "decompress exited with code " << dx_res.exit_code;
            throw crsce::common::exceptions::DecompressNonZeroExitException(oss.str());
        }

        // Compute output hash and log
        const std::string output_sha512 = crsce::testrunner::detail::compute_sha512(dx_path);
        crsce::testrunner::detail::log_decompress(dx_res, cx_path, dx_path, input_sha512, output_sha512);
        return output_sha512;
    }
}
