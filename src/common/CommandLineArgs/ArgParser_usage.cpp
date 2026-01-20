/**
 * @file ArgParser_usage.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief ArgParser::usage implementation.
 */
#include "common/ArgParser/ArgParser.h"
#include <string>

namespace crsce::common {

auto ArgParser::usage() const -> std::string {
  return programName_ + " -in <file> -out <file>";
}

} // namespace crsce::common
