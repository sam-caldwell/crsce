/**
 * @file ArgParser_ctor_parse.cpp
 * @brief ArgParser constructor overload that parses argv and throws on help/parse/no-arg.
 */
#include "common/ArgParser/ArgParser.h"

#include "common/exceptions/CliHelpRequested.h"
#include "common/exceptions/CliNoArgs.h"
#include "common/exceptions/CliParseError.h"

#include <span>
#include <string>
#include <utility>

namespace crsce::common {
    ArgParser::ArgParser(std::string programName, std::span<char *> args)
        : programName_(std::move(programName)) {
        if (programName_.empty()) { programName_ = "program"; }
        if (args.size() <= 1) {
            throw crsce::common::exceptions::CliNoArgs(usage());
        }
        // Perform parse; detect help flag and general parse errors
        const bool ok = parse(args);
        if (opts_.help) {
            throw crsce::common::exceptions::CliHelpRequested(usage());
        }
        if (!ok) {
            throw crsce::common::exceptions::CliParseError(usage());
        }
    }
}
