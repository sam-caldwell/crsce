/**
 * @file ArgParser_ctor_parse.cpp
 * @brief ArgParser constructor overload that parses argv and throws on help/parse/no-arg.
 */
#include "common/ArgParser/ArgParser.h"

#include "common/exceptions/CliHelpRequested.h"
#include "common/exceptions/CliNoArgs.h"
#include "common/exceptions/CliParseError.h"
#include "common/exceptions/CliInputMissing.h"
#include "common/exceptions/CliOutputExists.h"

#include <span>
#include <string>
#include <utility>
#include <sys/stat.h>

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
        // Filesystem validation: input must exist; output must not exist
        struct stat sb{};
        if (opts_.input.empty() || opts_.output.empty()) {
            // parse ok implies both were provided, but guard anyway
            throw crsce::common::exceptions::CliParseError(usage());
        }
        if (stat(opts_.input.c_str(), &sb) != 0) {
            throw crsce::common::exceptions::CliInputMissing(std::string("error: input file does not exist: ") + opts_.input);
        }
        if (stat(opts_.output.c_str(), &sb) == 0) {
            throw crsce::common::exceptions::CliOutputExists(std::string("error: output file already exists: ") + opts_.output);
        }
    }
}
