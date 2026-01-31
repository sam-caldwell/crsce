/**
 * @file run.h
 * @brief Entry for TestRunnerAlternating01 CLI pipeline.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

// Aggregator header for Alternating01 runner; declares a tag and includes detail declarations.

namespace crsce::testrunner_alternating01::cli {
    /**
     * @struct RunTag
     * @brief Tag type for Alternating01 runner aggregator header.
     */
    struct RunTag { };
}

#include "testrunnerAlternating01/Cli/detail/run_default.h"
#include "testrunnerAlternating01/Cli/detail/run_single.h"
