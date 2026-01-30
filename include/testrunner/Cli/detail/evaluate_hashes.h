/**
 * @file evaluate_hashes.h
 * @brief Validate input/output hash equality or throw.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <string>

namespace crsce::testrunner::cli {
    /**
     * @name evaluate_hashes
     * @brief Throw if hashes mismatch; return otherwise.
     * @param input_sha512 Expected input hash.
     * @param output_sha512 Reconstructed output hash.
     */
    void evaluate_hashes(const std::string &input_sha512, const std::string &output_sha512);
}
