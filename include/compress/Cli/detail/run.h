/**
 * @file detail/run.h
 * @brief Back-compat adapter for tests: parse argv and dispatch to public run(input, output).
 *        Not intended for production use; prefer constructing ArgParser in main().
 */
#pragma once

#include <span>
#include <string>

#include "common/ArgParser/ArgParser.h"
#include "common/exceptions/CliNoArgs.h"
#include "common/exceptions/CliHelpRequested.h"
#include "common/exceptions/CliParseError.h"
#include "common/exceptions/CliInputMissing.h"
#include "common/exceptions/CliOutputExists.h"
#include "compress/Cli/run.h"

namespace crsce::compress::cli {
    inline int run(std::span<char*> argv) {
        using crsce::common::ArgParser;
        // Special case: treat missing -in value as insufficient-args (4)
        if (argv.size() > 1) {
            for (std::size_t i = 1; i < argv.size(); ++i) {
                const std::string tok = argv[i];
                if (tok == "-in") {
                    bool followed_is_flag = false;
                    if (i + 1 < argv.size()) {
                        const char *raw = argv[i + 1];
                        const std::string next = (raw ? std::string(raw) : std::string());
                        followed_is_flag = (!next.empty() && next.front() == '-');
                    }
                    const bool missing = (i + 1 >= argv.size()) || followed_is_flag;
                    if (missing) { return 4; }
                }
            }
        }
        try {
            const ArgParser parser("compress", argv);
            const auto &[input, output, help] = parser.options();
            return crsce::compress::cli::run(input, output);
        } catch (const crsce::common::exceptions::CliNoArgs & /*e*/) { return 0; }
          catch (const crsce::common::exceptions::CliHelpRequested & /*e*/) { return 0; }
          catch (const crsce::common::exceptions::CliParseError & /*e*/) { return 2; }
          catch (const crsce::common::exceptions::CliInputMissing & /*e*/) { return 3; }
          catch (const crsce::common::exceptions::CliOutputExists & /*e*/) { return 3; }
    }
}
