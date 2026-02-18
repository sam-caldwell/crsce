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
