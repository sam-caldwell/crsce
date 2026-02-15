/**
 * @file Csm_row_version.cpp
 * @author Sam Caldwell
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "decompress/Csm/Csm.h"
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace crsce::decompress {
    /**
     * @name row_version
     * @brief return row versions at row r
     * @param r row
     * @return uint64
     */
    std::uint64_t Csm::row_version(const std::size_t r) const {
        return row_versions_.at(r).load(std::memory_order_relaxed);
    }
}
