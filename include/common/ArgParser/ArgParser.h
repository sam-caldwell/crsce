/**
 * @file ArgParser.h
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief Simple command-line argument parser shared by project binaries.
 * @note Located under include/common/ArgParser.
 *
 * Supports flags: -h/--help, -in <path>, -out <path> and exposes parsed
 * values via a small Options POD. Intended for use by cmd/compress and
 * cmd/decompress to validate required I/O arguments.
 */
#pragma once

#include <span>
#include <string>

namespace crsce::common {

/**
 * @class ArgParser
 * @brief Minimal flag/value parser for project CLIs.
 * @details Parses a limited set of flags from argv and provides accessors
 * for the parsed state. Produces a usage string that can be displayed on
 * invalid input or when help is requested.
 */
class ArgParser {
 public:
  /**
   * @struct Options
   * @brief Parsed arguments for input, output, and help.
   */
  struct Options {
    /**
     * @name input
     * @brief Input file path parsed from -in <path>.
     */
    std::string input;

    /**
     * @name output
     * @brief Output file path parsed from -out <path>.
     */
    std::string output;

    /**
     * @name help
     * @brief True if -h/--help was provided by the user.
     */
    bool help{false};
  };

  /**
   * @name ArgParser
   * @brief Construct a parser bound to a program name (used in usage strings).
   * @usage ArgParser p("compress");
   * @throws None
   * @param programName Human-friendly program name (e.g., "compress").
   * @return N/A
   */
  explicit ArgParser(std::string programName);

  /**
   * @name parse
   * @brief Parse argv for -h/--help, -in <path>, -out <path>.
   * @usage if (!parser.parse({argv, argv+argc})) { show_usage(); }
   * @throws None
   * @param args Span of C-strings (argv slice) to parse.
   * @return true if parsing succeeded; false on an unknown flag or missing value.
   */
  auto parse(std::span<char*> args) -> bool;

  /**
   * @name options
   * @brief Access parsed options.
   * @usage const auto &o = parser.options();
   * @throws None
   * @return Const reference to the current Options.
   */
  [[nodiscard]] auto options() const -> const Options& { return opts_; }

  /**
   * @name usage
   * @brief Create a usage string: "<program> -in <file> -out <file>".
   * @usage std::string u = parser.usage();
   * @throws None
   * @return Human-readable usage string.
   */
  [[nodiscard]] auto usage() const -> std::string;

 private:
  /**
   * @name programName_
   * @brief Program name used in usage text.
   * @usage Set by the constructor; immutable thereafter.
   */
  std::string programName_;

  /**
   * @name opts_
   * @brief Parsed flags and values.
   * @usage Updated by parse(); observed via options().
   */
  Options opts_;
};

}  // namespace crsce::common
/**
 * @file ArgParser.h
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief Simple command-line argument parser shared by project binaries.
 */
