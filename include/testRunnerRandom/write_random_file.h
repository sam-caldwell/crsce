/**
 * @file write_random_file.h
 * @brief Random file generation utility.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <cstdint>
#include <filesystem>
#include <random>

namespace crsce::testrunner::detail {
    /**
     * @name write_random_file
     * @brief Write a file of random bytes with given size.
     */
    void write_random_file(const std::filesystem::path &p, std::uint64_t bytes, std::mt19937_64 &rng);
}
