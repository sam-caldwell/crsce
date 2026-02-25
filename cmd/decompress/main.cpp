/**
 * @file cmd/decompress/main.cpp
 * @brief CLI entry for decompressor; parse args and dispatch to runner.
 *        Reconstructs CSMs via solvers, verifies LH, and writes original bytes.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "common/O11y/O11y.h"
#include "common/exceptions/CliInputMissing.h"
#include "common/exceptions/CliOutputExists.h"
#include "decompress/Cli/run.h"
#include "common/ArgParser/ArgParser.h"
#include "common/exceptions/CliHelpRequested.h"
#include "common/exceptions/CliNoArgs.h"
#include "common/exceptions/CliParseError.h"
#include "decompress/Cli/Heartbeat.h"
#include <span>
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
    try {
        ::crsce::o11y::O11y::instance().start();

        const std::span<char *> args{argv, static_cast<std::size_t>(argc)};
        const crsce::common::ArgParser parser("decompress", args);
        const auto &[input, output, help] = parser.options();

        ::crsce::o11y::O11y::instance().metric("decompress_begin", static_cast<std::int64_t>(1),
                                               {{"in", input}, {"out", output}});

        crsce::decompress::cli::Heartbeat heartbeat;
        heartbeat.start();
        const int rc = crsce::decompress::cli::run(input, output);
        ::crsce::o11y::O11y::instance().metric("decompress_end", static_cast<std::int64_t>(1),
                                               {{"status", (rc == 0 ? std::string("OK") : std::string("FAIL"))}});
        heartbeat.wait();
        std::puts(rc == 0 ? "ok" : "failed");
        return rc;
    } catch (const crsce::common::exceptions::CliNoArgs & /*e*/) {
        ::crsce::o11y::O11y::instance().metric("decompress_end", static_cast<std::int64_t>(1),
                                               {
                                                   {"status", "FAIL"},
                                                   {"detail", "NO_ARG"}
                                               });
        std::puts("ok");
        return 0;
    } catch (const crsce::common::exceptions::CliHelpRequested &e) {
        ::crsce::o11y::O11y::instance().metric("decompress_end", static_cast<std::int64_t>(1),
                                               {
                                                   {"status", "FAIL"},
                                                   {"detail", "BAD_USAGE"}
                                               });
        std::println("usage: {}", e.what());
        std::puts("ok");
        return 0;
    } catch (const crsce::common::exceptions::CliParseError &e) {
        ::crsce::o11y::O11y::instance().metric("decompress_end", static_cast<std::int64_t>(1),
                                               {
                                                   {"status", "FAIL"},
                                                   {"detail", "PARSE_ERROR"}
                                               });
        std::println(stderr, "usage: {}", e.what());
        std::puts("failed");
        return 2;
    } catch (const crsce::common::exceptions::CliInputMissing &e) {
        ::crsce::o11y::O11y::instance().metric("decompress_end", static_cast<std::int64_t>(1),
                                               {{"status", "FAIL"}, {"detail", "INPUT_MISSING"}});
        std::println(stderr, "{}", e.what());
        std::puts("failed");
        return 3;
    } catch (const crsce::common::exceptions::CliOutputExists &e) {
        ::crsce::o11y::O11y::instance().metric("decompress_end", static_cast<std::int64_t>(1),
                                               {{"status", "FAIL"}, {"detail", "OUTPUT_EXISTS"}});
        std::println(stderr, "{}", e.what());
        std::puts("failed");
        return 3;
    } catch (...) {
        ::crsce::o11y::O11y::instance().metric("decompress_end", static_cast<std::int64_t>(1),
                                               {
                                                   {"status", "FAIL"},
                                                   {"detail", "UNHANDLED_EXCEPTION"}
                                               });
        std::puts("failed");
        return 2;
    }
}
