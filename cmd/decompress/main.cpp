/**
 * @file cmd/decompress/main.cpp
 * @brief CLI entry for decompressor; parse args and dispatch to runner.
 *        Reconstructs CSMs via solvers, verifies LH, and writes original bytes.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "common/O11y/O11y.h"
#include "common/Cli/ValidateInOut.h"
#include "decompress/Cli/run.h"
#include "common/ArgParser/ArgParser.h"
#include "common/exceptions/CliHelpRequested.h"
#include "common/exceptions/CliNoArgs.h"
#include "common/exceptions/CliParseError.h"
#include "decompress/Cli/detail/heartbeat_worker.h"
#include <span>
#include <atomic>
#include <thread>
#include <cstdlib>
#include <string>
#include <print>
#include <cstdio>
#include <cstdint> // NOLINT

/**
 * @brief Program entry point for decompressor CLI.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Process exit code (0 on success, non-zero represents an error state).
 */
auto main(const int argc, char *argv[]) -> int {
    // Start background o11y so metrics/counters/events persist asynchronously.
    ::crsce::o11y::O11y::instance().start();
    const std::span<char *> args{
        argv, static_cast<std::size_t>(argc)
    };
    try {
        crsce::common::ArgParser parser("decompress", args);
        if (const int vrc = crsce::common::cli::validate_in_out(parser, args); vrc != 0) { return vrc; }
        const auto &[input, output, help] = parser.options();

        ::crsce::o11y::O11y::instance().metric("decompress_begin", static_cast<std::int64_t>(1),
                                               {{"in", input}, {"out", output}});

        ::crsce::o11y::O11y::instance().counter("decompress_files_attempted");

        // Heartbeat thread (optional): periodically log snapshot/phase metrics
        std::atomic<bool> hb_run{true};
        unsigned hb_ms = 1000U;
        if (const char *p = std::getenv("CRSCE_HEARTBEAT_MS"); p && *p) { // NOLINT(concurrency-mt-unsafe)
            const std::int64_t v = std::strtoll(p, nullptr, 10);
            if (v > 0) { hb_ms = static_cast<unsigned>(v); }
        }

        auto hb_thr = std::thread(&crsce::decompress::cli::detail::heartbeat_worker, &hb_run, hb_ms);

        const int rc = crsce::decompress::cli::run(input, output);

        if (rc == 0) {
            ::crsce::o11y::O11y::instance().counter("decompress_files_success");
        } else {
            ::crsce::o11y::O11y::instance().event("decompress_error", {{"type", std::string("pipeline_failed")}});
            ::crsce::o11y::O11y::instance().counter("decompress_files_failed");
        }

        ::crsce::o11y::O11y::instance().metric("decompress_end", static_cast<std::int64_t>(1),
                                               {{"status", (rc == 0 ? std::string("OK") : std::string("FAIL"))}});

        hb_run.store(false, std::memory_order_relaxed);
        if (hb_thr.joinable()) {
            hb_thr.join();
        }

        return rc;
    } catch (const crsce::common::exceptions::CliNoArgs & /*e*/) {
        return 0;
    } catch (const crsce::common::exceptions::CliHelpRequested &e) {
        std::println("usage: {}", e.what());
        return 0;
    } catch (const crsce::common::exceptions::CliParseError &e) {
        std::println(stderr, "usage: {}", e.what());
        return 2;
    }
}
