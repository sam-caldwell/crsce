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
#include <iostream>
#include <span>
#include <cstddef>
#include <cstdint> // NOLINT
#include <cstdio>

/**
 * @brief Program entry point for compressor CLI.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Process exit code (0 on success, non-zero represents an error state).
 */
auto main(const int argc, char *argv[]) -> int {
    try {
        ::crsce::o11y::O11y::instance().start();

        const std::span<char *> args{argv, static_cast<std::size_t>(argc)};
        const crsce::common::ArgParser parser("compress", args);
        const auto &[input, output, help] = parser.options();

        ::crsce::o11y::O11y::instance().event("compress_begin", {{"in", input}, {"out", output}});

        crsce::compress::cli::Heartbeat heartbeat;
        heartbeat.start();
        const int rc = crsce::compress::cli::run(input, output);
        ::crsce::o11y::O11y::instance().event("compress_end", {{"status", (rc == 0 ? std::string("OK") : std::string("FAIL"))}});
        heartbeat.wait();

        return rc;
    } catch (const crsce::common::exceptions::CliNoArgs &e) {
        ::crsce::o11y::O11y::instance().event("compress_end", {
                                                   {"status", "FAIL"},
                                                   {"detail", "NO_ARG"},
                                                   {"what", e.what()}
                                               });
        std::cout << "crsce-compress: ready" << '\n';
        return 0;
    } catch (const crsce::common::exceptions::CliHelpRequested &e) {
        ::crsce::o11y::O11y::instance().event("compress_end", {
                                                   {"status", "FAIL"},
                                                   {"detail", "BAD_USAGE"},
                                                   {"what", e.what()}
                                               });
        std::cout << "usage: " << e.what() << '\n';
        return 0;
    } catch (const crsce::common::exceptions::CliParseError &e) {
        ::crsce::o11y::O11y::instance().event("compress_end", {
                                                   {"status", "FAIL"},
                                                   {"detail", "PARSE_ERROR"},
                                                   {"what", e.what()}
                                               });
        std::cerr << "usage: " << e.what() << '\n';
        return 2;
    } catch (const crsce::common::exceptions::CliInputMissing &e) {
        ::crsce::o11y::O11y::instance().event("compress_end", {
                                                   {"status", "FAIL"},
                                                   {"detail", "INPUT_MISSING"},
                                                   {"what", e.what()}
                                               });
        std::cerr << "{}" << e.what() << '\n';
        return 3;
    } catch (const crsce::common::exceptions::CliOutputExists &e) {
        ::crsce::o11y::O11y::instance().event("compress_end", {
                                                   {"status", "FAIL"},
                                                   {"detail", "OUTPUT_EXISTS"},
                                                   {"what", e.what()}
                                               });
        std::cerr << e.what() << '\n';
        return 3;
    } catch (...) {
        ::crsce::o11y::O11y::instance().event("compress_end", {
                                                   {"status", "FAIL"},
                                                   {"detail", "UNHANDLED_EXCEPTION"}
                                               });
    }
}
