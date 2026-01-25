/**
 * @file BitHashBuffer_popHash.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief Pop the oldest queued hash if available.
 */
#include "../../../include/common/BitHashBuffer/BitHashBuffer.h"
#include <array>
#include <cstdint>
#include <optional>

namespace crsce::common {
    /**
     * @name popHash
     * @brief Pop the oldest queued hash if available.
     * @usage auto h = buf.popHash();
     * @throws None
     * @return Oldest 32-byte digest or std::nullopt if empty.
     */
    std::optional<std::array<std::uint8_t, BitHashBuffer::kHashSize> > BitHashBuffer::popHash() {
        if (hashVector_.empty()) {
            return std::nullopt;
        }
        auto front = hashVector_.front();
        hashVector_.erase(hashVector_.begin());
        return front;
    }
} // namespace crsce::common
