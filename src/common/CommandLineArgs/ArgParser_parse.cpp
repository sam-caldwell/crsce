/**
 * @file ArgParser_parse.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief ArgParser::parse implementation.
 */
#include "CommandLineArgs/ArgParser.h"
#include <cstddef>
#include <span>
#include <string>

namespace crsce::common {

auto ArgParser::parse(std::span<char *> args) -> bool { // GCOVR_EXCL_LINE
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
      const char *val = args[++i];
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
  return args.size() <= 1 || opts_.help ||
         (!opts_.input.empty() && !opts_.output.empty());
}

} // namespace crsce::common
