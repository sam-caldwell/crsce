/**
 * @file decompress_file.h
 * @brief Run decompress CLI, compute output hash, log, enforce timeout.
 * @author Sam Caldwell
 * © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace crsce::testrunner::cli {
    /**
     * @name decompress_file
     * @brief Invoke decompress, write stdio artifacts, compute SHA-512 of output, and log.
     * @param cx_path Input container path.
     * @param dx_path Reconstructed output file path.
     * @param input_sha512 Expected hash of original input.
     * @param timeout_ms Max allowed elapsed time in milliseconds.
     * @return SHA-512 of the reconstructed output.
     * @throws DecompressTimeoutException on timeout.
     * @throws DecompressNonZeroExitException on non-zero exit.
     */
    std::string decompress_file(const std::filesystem::path &cx_path,
                                const std::filesystem::path &dx_path,
                                const std::string &input_sha512,
                                std::int64_t timeout_ms);
}
