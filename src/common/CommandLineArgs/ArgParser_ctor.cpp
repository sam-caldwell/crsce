/**
 * @file ArgParser_ctor.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief ArgParser constructor implementation.
 */
#include "CommandLineArgs/ArgParser.h"
#include <utility>
#include <string>

namespace crsce::common {

ArgParser::ArgParser(std::string programName) // GCOVR_EXCL_LINE
    : programName_(std::move(programName)) {  // GCOVR_EXCL_LINE
  // Ensure the program name is non-empty for usage strings
  if (programName_.empty()) {
    programName_ = "program";
  }
}

}  // namespace crsce::common
