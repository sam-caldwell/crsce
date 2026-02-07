/**
 * @file run.cpp
 * @brief Implements the compressor CLI runner: parse args, validate paths, run compression.
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for more information.
 */
#include "compress/Cli/detail/run.h"

#include "common/ArgParser/ArgParser.h"
#include "compress/Compress/Compress.h"
#include "common/Cli/ValidateInOut.h"

#include <exception>
#include <print>
#include <span>
#include <string>
#include <string>
#include <cstddef>
#include <cstdio>

// Local logging macros (avoid extra constructs for ODPCPP)
// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define CRSCE_NOW_MS() (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count())
#include "common/O11y/metric.h"
// NOLINTEND(cppcoreguidelines-macro-usage)

namespace crsce::compress::cli {
    /**
     * @name run
     * @brief Run the compression CLI pipeline.
     * @param args Raw argv span (argv[0]..argv[argc-1]).
     * @return Process exit code: 0 on success; non-zero on failure.
     */
    int run(const std::span<char *> args) {
        try {
            if (args.size() <= 1) {
                // Friendly no-arg behavior: greet and exit successfully.
                std::println("crsce-compress: ready");
                return 0;
            }
            // Short-circuit help requests to avoid running pipeline.
            for (std::size_t i = 1; i < args.size(); ++i) {
                const std::string a = args[i];
                if (a == "-h" || a == "--help") {
                    std::println("usage: {}", std::string("compress -in <file> -out <file>"));
                    return 0;
                }
            }
            crsce::common::ArgParser parser("compress");
            if (const int vrc = crsce::common::cli::validate_in_out(parser, args); vrc != 0) {
                return vrc;
            }
            const auto &[input, output, help] = parser.options();

            // Note: zero-length inputs are valid (produce a header with zero blocks).

            ::crsce::o11y::metric("compress_begin", 1LL, {{"in", input}, {"out", output}});
            ::crsce::o11y::counter("compress_files_attempted");
            bool ok = true;
            if (crsce::compress::Compress cx(input, output); !cx.compress_file()) {
                std::println(stderr, "error: compression failed");
                ok = false;
            }
            if (ok) {
                ::crsce::o11y::counter("compress_files_success");
            } else {
                ::crsce::o11y::event("compress_error", {{"type", std::string("pipeline_failed")}});
                ::crsce::o11y::counter("compress_files_failed");
            }
            ::crsce::o11y::metric("compress_end", 1LL, {{"status", (ok ? std::string("OK") : std::string("FAIL"))}});
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
            std::fputs("error: ", stderr);
            std::fputs(e.what(), stderr);
            std::fputs("\n", stderr);
            ::crsce::o11y::event("compress_error", {{"type", std::string("exception")}, {"what", std::string(e.what())}});
            ::crsce::o11y::counter("compress_files_failed");
            return 1;
        } catch (...) {
            // NOSONAR
            std::fputs("error: unknown exception\n", stderr);
            ::crsce::o11y::event("compress_error", {{"type", std::string("exception_unknown")}});
            ::crsce::o11y::counter("compress_files_failed");
            return 1;
        }
        // GCOVR_EXCL_STOP
    }
} // namespace crsce::compress::cli
