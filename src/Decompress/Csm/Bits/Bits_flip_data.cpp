/**
 * @file Bits_flip_data.cpp
 * @author Sam Caldwell
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "decompress/Csm/detail/Bits.h"
#include <atomic>
#include <cstdint>

namespace crsce::decompress {
    /**
     * @name flip_data
     * @brief Perform a xor bit-flip
     * @return void
     */
    void Bits::flip_data() noexcept {
        const std::atomic_ref<std::uint8_t> a(b_);
        (void)a.fetch_xor(kData, std::memory_order_acq_rel);
    }
}
