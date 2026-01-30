/**
 * @file evaluate_hashes.cpp
 * @brief Validate output hash equals input hash; throw on mismatch.
 * @author Sam Caldwell
 * © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "testrunner/Cli/detail/evaluate_hashes.h"

#include "common/exceptions/PossibleCollisionException.h"
#include <string>

namespace crsce::testrunner::cli {
    /**
     * @name evaluate_hashes
     * @brief Throw if hashes mismatch; return otherwise.
     */
    void evaluate_hashes(const std::string &input_sha512, const std::string &output_sha512) {
        if (input_sha512 != output_sha512) {
            throw crsce::common::exceptions::PossibleCollisionException(
                "sha512 mismatch between input and decompressed output");
        }
    }
}
