/**
 * @file crc32_table.cpp
 * @brief Definition for accessing the reflected CRC-32 lookup table.
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt
 */
#include "common/Util/detail/crc32_table.h"
#include "common/Util/detail/make_entry_rt.h"
#include <array>
#include <cstdint>

namespace crsce::common::util::detail {
    /**
     * @name crc32_table
     * @brief Access the 256-entry reflected CRC-32 table (initialized on first use).
     * @return const std::array<std::uint32_t, 256>& Process-lifetime lookup table.
     */
    const std::array<std::uint32_t, 256> &crc32_table() {
        static std::array<std::uint32_t, 256> t{};
        static bool init = false;
        if (!init) {
            for (std::uint32_t i = 0; i < 256U; ++i) {
                t.at(i) = make_entry_rt(i);
            }
            init = true;
        }
        return t;
    }
}
