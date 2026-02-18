/**
 * @file cmd/compress/main.cpp
 * @author Sam Caldwell
 * @brief CLI entry for compressor; validates args and dispatches to runner.
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for more information.
 */
#include "compress/Cli/run.h"
#include "common/O11y/O11y.h"
#include "compress/Cli/Heartbeat.h"
#include "common/ArgParser/ArgParser.h"
#include "common/exceptions/CliInputMissing.h"
#include "common/exceptions/CliOutputExists.h"
#include "common/exceptions/CliHelpRequested.h"
#include "common/exceptions/CliNoArgs.h"
#include "common/exceptions/CliParseError.h"

#include <span>
#include <cstddef>
#include <print>
#include <cstdio>

/**
 * @brief Program entry point for compressor CLI.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Process exit code (0 on success, non-zero represents an error state).
 */
auto main(const int argc, char *argv[]) -> int {
    // Start background o11y so metrics/counters/events persist asynchronously.
    ::crsce::o11y::O11y::instance().start();
    const std::span<char *> args{argv, static_cast<std::size_t>(argc)};

    try {
        const crsce::common::ArgParser parser("compress", args);
        const auto &[input, output, help] = parser.options();
        crsce::compress::cli::Heartbeat heartbeat;
        heartbeat.start();
        const int rc = crsce::compress::cli::run(input, output);
        heartbeat.wait();
        return rc;
    } catch (const crsce::common::exceptions::CliNoArgs & /*e*/) {
        // Friendly no-arg behavior: greet and exit successfully.
        std::puts("crsce-compress: ready");
        return 0;
    } catch (const crsce::common::exceptions::CliHelpRequested &e) {
        std::println("usage: {}", e.what());
        return 0;
    } catch (const crsce::common::exceptions::CliParseError &e) {
        std::println(stderr, "usage: {}", e.what());
        return 2;
    } catch (const crsce::common::exceptions::CliInputMissing &e) {
        std::println(stderr, "{}", e.what());
        return 3;
    } catch (const crsce::common::exceptions::CliOutputExists &e) {
        std::println(stderr, "{}", e.what());
        return 3;
    }
}
