/**
 * @file ArgParser.cpp
 * @brief Implementation of the minimal command-line argument parser.
 */
#include "CommandLineArgs/ArgParser.h"

#include <utility>
#include <span>
#include <string>
#include <cstddef>

namespace crsce::common {
    /**
     * @brief Construct the parser and normalize the program name.
     */
    ArgParser::ArgParser(std::string programName) // GCOVR_EXCL_LINE
        : programName_(std::move(programName)) { // GCOVR_EXCL_LINE
        // Ensure the program name is non-empty for usage strings
        if (programName_.empty()) {
            programName_ = "program";
        }
    }

    /**
     * @brief Parse argv and populate Options with recognized flags.
     */
    auto ArgParser::parse(std::span<char*> args) -> bool { // GCOVR_EXCL_LINE
        // Reset options each parse
        opts_ = Options{};
        // Use manual index to avoid modifying the loop counter in-place
        std::size_t i = 1; // NOLINT(*-identifier-length)
        while (i < args.size()) {
            const std::string arg = args[i];
            if (arg == "-h" || arg == "--help") {
                opts_.help = true;
                ++i;
                continue;
            }
            if (arg == "-in" || arg == "-out") {
                if (i + 1 >= args.size()) {
                    return false; // missing value
                }
                const char* val = args[++i];
                if (arg == "-in") {
                    opts_.input = val;
                } else {
                    opts_.output = val;
                }
                ++i;
                continue;
            }
            // Unknown flag/token
            return false;
        }
        return args.size() <= 1 || opts_.help || (!opts_.input.empty() && !opts_.output.empty());
    }

    /**
     * @brief Build usage string showing required -in/-out flags.
     */
    auto ArgParser::usage() const -> std::string {
        return programName_ + " -in <file> -out <file>";
    }
} // namespace crsce::common
