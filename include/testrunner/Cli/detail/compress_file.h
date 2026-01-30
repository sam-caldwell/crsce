/**
 * @file compress_file.h
 * @brief Run compress CLI, log JSON, enforce timeout.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include "testrunner/detail/proc_result.h"

namespace crsce::testrunner::cli {
    /**
     * @name compress_file
     * @brief Invoke compress, write stdio artifacts, log, and enforce timeout.
     * @param in_path Input file path.
     * @param cx_path Output container file path.
     * @param input_sha512 SHA-512 of input file.
     * @param timeout_ms Max allowed elapsed time in milliseconds.
     * @return ProcResult describing subprocess execution.
     * @throws CompressTimeoutException on timeout.
     * @throws CompressNonZeroExitException on non-zero exit.
     */
    crsce::testrunner::detail::ProcResult compress_file(const std::filesystem::path &in_path,
                                                        const std::filesystem::path &cx_path,
                                                        const std::string &input_sha512,
                                                        std::int64_t timeout_ms);
}
