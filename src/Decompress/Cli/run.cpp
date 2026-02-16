/**
 * @file run.cpp
 * @brief Implements the decompressor CLI runner: parse args, validate paths, run decompression.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "decompress/Cli/detail/run.h"

#include "common/ArgParser/ArgParser.h"
#include "decompress/Decompressor/Decompressor.h"
#include "decompress/Cli/detail/heartbeat_worker.h"
#include "common/Cli/ValidateInOut.h"

#include <exception>
#include <span>
#include <cstdio>
#include <cstddef>
#include <string>
#include <print>
#include <cstdint>
#include <thread>
#include <atomic>
#include <cstdlib>
#include "common/O11y/O11y.h"

namespace crsce::decompress::cli {
    /**
     * @name run
     * @brief Run the decompression CLI pipeline.
     * @param args Raw argv span (argv[0]..argv[argc-1]).
     * @return Process exit code: 0 on success; non-zero on failure.
     */
    int run(std::span<char *> const args) {
        try {
            if (args.size() <= 1) {
                // No-arg behavior: do nothing and exit successfully.
                return 0;
            }
            // Short-circuit help requests to avoid running a pipeline.
            for (std::size_t i = 1; i < args.size(); ++i) {
                const std::string a = args[i];
                if (a == "-h" || a == "--help") {
                    std::print("usage: {}\n", "decompress -in <file> -out <file>");
                    return 0;
                }
            }
            crsce::common::ArgParser parser("decompress");
            if (const int vrc = crsce::common::cli::validate_in_out(parser, args); vrc != 0) {
                return vrc;
            }
            const auto &[input, output, help] = parser.options();

            ::crsce::o11y::O11y::instance().metric("decompress_begin", static_cast<std::int64_t>(1), {{"in", input}, {"out", output}});
            ::crsce::o11y::O11y::instance().counter("decompress_files_attempted");

            // Heartbeat thread: periodically print timestamp, phase, and a few progress metrics.
            std::atomic<bool> hb_run{true};
            unsigned hb_ms = 1000U;
            if (const char *p = std::getenv("CRSCE_HEARTBEAT_MS"); p && *p) { // NOLINT(concurrency-mt-unsafe)
                const std::int64_t v = std::strtoll(p, nullptr, 10);
                if (v > 0) { hb_ms = static_cast<unsigned>(v); }
            }
            // Allow disabling via CRSCE_HEARTBEAT=0
            bool hb_enabled = true;
            if (const char *p = std::getenv("CRSCE_HEARTBEAT"); p && *p) { // NOLINT(concurrency-mt-unsafe)
                hb_enabled = (std::string(p) != "0");
            }

            std::thread hb_thr;
            if (hb_enabled) {
                hb_thr = std::thread(&crsce::decompress::cli::detail::heartbeat_worker, &hb_run, hb_ms);
            }

            bool ok = true;
            if (crsce::decompress::Decompressor dx(input, output); !dx.decompress_file()) {
                ok = false;
            }
            hb_run.store(false, std::memory_order_relaxed);
            if (hb_enabled && hb_thr.joinable()) { hb_thr.join(); }
            if (ok) {
                ::crsce::o11y::O11y::instance().counter("decompress_files_success");
            } else {
                ::crsce::o11y::O11y::instance().event("decompress_error", {{"type", std::string("pipeline_failed")}});
                ::crsce::o11y::O11y::instance().counter("decompress_files_failed");
            }
            ::crsce::o11y::O11y::instance().metric("decompress_end", static_cast<std::int64_t>(1), {{"status", (ok ? std::string("OK") : std::string("FAIL"))}});
            if (!ok) { return 4; }
            return 0;
        } catch (const std::exception &e) {
            std::fputs("error: ", stderr);
            std::fputs(e.what(), stderr);
            std::fputs("\n", stderr);
            ::crsce::o11y::O11y::instance().event("decompress_error", {{"type", std::string("exception")}, {"what", std::string(e.what())}});
            ::crsce::o11y::O11y::instance().counter("decompress_files_failed");
            return 1;
        } catch (...) {
            std::fputs("error: unknown exception\n", stderr);
            ::crsce::o11y::O11y::instance().event("decompress_error", {{"type", std::string("exception_unknown")}});
            ::crsce::o11y::O11y::instance().counter("decompress_files_failed");
            return 1;
        }
    }
} // namespace crsce::decompress::cli
