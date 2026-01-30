/**
 * @file compress_file.cpp
 * @brief Run compress, write artifacts, log, and enforce timeout.
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "testrunner/Cli/detail/compress_file.h"

#include "testrunner/detail/proc_result.h"
#include "testrunner/detail/run_process.h"
#include "testrunner/detail/log_compress.h"
#include "testrunner/detail/files.h"
#include "common/exceptions/CompressNonZeroExitException.h"
#include "common/exceptions/CompressTimeoutException.h"

#include <filesystem>
#include <sstream>
#include <string>
#include <cstdint>
#include <optional>
#include <cstdlib>

namespace fs = std::filesystem;

namespace crsce::testrunner::cli {
    /**
     * @name compress_file
     * @brief Invoke compress, write stdio artifacts, log, and enforce timeout.
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
        const ProcResult cx_res = run_process({ exe, "-in", in_path.string(), "-out", cx_path.string() }, std::nullopt);

        // Persist child stdio regardless of success/failure
        crsce::testrunner::detail::write_file_text(fs::path(cx_path).replace_extension(".compress.stdout.txt"), cx_res.out);
        crsce::testrunner::detail::write_file_text(fs::path(cx_path).replace_extension(".compress.stderr.txt"), cx_res.err);

        // Emit JSON log
        crsce::testrunner::detail::log_compress(cx_res, in_path, cx_path, input_sha512);

        // Enforce timeout
        const auto elapsed = cx_res.end_ms - cx_res.start_ms;
        if (elapsed > timeout_ms) {
            throw crsce::common::exceptions::CompressTimeoutException("compress timed out");
        }
        if (cx_res.exit_code != 0) {
            std::ostringstream oss; oss << "compress exited with code " << cx_res.exit_code;
            throw crsce::common::exceptions::CompressNonZeroExitException(oss.str());
        }
        return cx_res;
    }
}
