/**
 * @file ArgParser.h
 * @brief Simple command-line argument parser shared by project binaries.
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
 */
class ArgParser {
 public:
  /**
   * @struct Options
   * @brief Parsed arguments for input, output, and help.
   */
  struct Options {
    std::string input;   ///< Input file path (from -in)
    std::string output;  ///< Output file path (from -out)
    bool help{false};    ///< True if -h/--help was provided
  };

  /**
   * @brief Construct a parser bound to a program name (used in usage strings).
   * @param programName Human-friendly program name (e.g., "compress").
   */
  explicit ArgParser(std::string programName);

  /**
   * @brief Parse argv for -h/--help, -in <path>, -out <path>.
   * @param args
   * @return true if parsing succeeded; false on an unknown flag or missing value
   */
  auto parse(std::span<char*> args) -> bool;

  /**
   * @brief Access parsed options.
   */
  [[nodiscard]] auto options() const -> const Options& { return opts_; }

  /**
   * @brief Create a usage string: "<program> -in <file> -out <file>".
   */
  [[nodiscard]] auto usage() const -> std::string;

 private:
  std::string programName_;
  Options opts_;
};

}  // namespace crsce::common
