/**
 * @file run.cpp
 * @brief Implements the compressor CLI runner: parse args, validate paths, run compression.
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for more information.
 */
#include "compress/Cli/run.h"

#include "compress/Compress/Compress.h"

#include <exception>
#include <string>
#include <cstdint>

// Local logging macros (avoid extra constructs for ODPCPP)
// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define CRSCE_NOW_MS() (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count())
#include "common/O11y/O11y.h"
// NOLINTEND(cppcoreguidelines-macro-usage)

namespace crsce::compress::cli {
    /**
     * @name run
     * @brief Run the compression CLI pipeline.
     * @param input input filename (source)
     * @param output output filename (target)
     * @return Process exit code: 0 on success; non-zero on failure.
     */
    int run(const std::string &input, const std::string &output) { // NOLINT(misc-use-internal-linkage)
        try {
            // Note: zero-length inputs are valid (produce a header with zero blocks).

            ::crsce::o11y::O11y::instance().metric("compress_begin", static_cast<std::int64_t>(1), {{"in", input}, {"out", output}});
            ::crsce::o11y::O11y::instance().counter("compress_files_attempted");
            bool ok = true;
            if (crsce::compress::Compress cx(input, output); !cx.compress_file()) {
                ok = false;
            }
            if (ok) {
                ::crsce::o11y::O11y::instance().counter("compress_files_success");
            } else {
                ::crsce::o11y::O11y::instance().event("compress_error", {{"type", std::string("pipeline_failed")}});
                ::crsce::o11y::O11y::instance().counter("compress_files_failed");
            }
            ::crsce::o11y::O11y::instance().metric("compress_end", static_cast<std::int64_t>(1), {{"status", (ok ? std::string("OK") : std::string("FAIL"))}});
            if (!ok) {
                return 4;
            }
            return 0;
        // The following catch blocks are defensive and practically unreachable
        // in normal operation. Exclude them from coverage to avoid skewing
        // region metrics while keeping robust error reporting in release.
        // GCOVR_EXCL_START
        } catch (const std::exception &e) {
            // NOSONAR
            ::crsce::o11y::O11y::instance().event("compress_error", {{"type", std::string("exception")}, {"what", std::string(e.what())}});
            ::crsce::o11y::O11y::instance().counter("compress_files_failed");
            return 1;
        } catch (...) {
            // NOSONAR
            ::crsce::o11y::O11y::instance().event("compress_error", {{"type", std::string("exception_unknown")}});
            ::crsce::o11y::O11y::instance().counter("compress_files_failed");
            return 1;
        }
        // GCOVR_EXCL_STOP
    }
} // namespace crsce::compress::cli
