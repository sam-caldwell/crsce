/**
 * @file run.h
 * @brief CLI runner for the decompressor binary.
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#pragma once

#include "decompress/Cli/detail/run.h"

namespace crsce::decompress::cli {
    /**
     * @name DecompressCliRunTag
     * @brief Tag type to satisfy one-definition-per-header for CLI run declaration.
     */
    struct DecompressCliRunTag {
    };
}
