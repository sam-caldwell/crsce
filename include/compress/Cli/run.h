/**
 * @file run.h
 * @brief CLI runner for the compressor binary.
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#pragma once

#include "compress/Cli/detail/run.h"

namespace crsce::compress::cli {
    /**
     * @name CompressCliRunTag
     * @brief Tag type to satisfy one-definition-per-header for CLI run declaration.
     */
    struct CompressCliRunTag {
    };
}
