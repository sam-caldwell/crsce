/**
 * @file BitHashBuffer_finalizeRowIfFull.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief Compute chained hash when a 64-byte row completes.
 */
#include "common/BitHashBuffer.h"
#include "common/BitHashBuffer/detail/Sha256.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <iterator>

namespace crsce::common {
    /**
     * @name finalizeRowIfFull
     * @brief If 64 bytes accumulated, compute chained hash and reset the row.
     * @usage Internal: invoked after flushing bytes to check for row completion.
     * @throws None
     * @return N/A
     */
    void BitHashBuffer::finalizeRowIfFull() {
        if (rowIndex_ < kRowSize) {
            return;
        }

        std::array<std::uint8_t, kHashSize> prev = hashVector_.empty() ? seedHash_ : hashVector_.back();
        std::array<std::uint8_t, kHashSize + kRowSize> buf{};
        std::copy_n(prev.begin(), kHashSize, buf.begin());
        auto* out_it = std::ranges::next(buf.begin(), static_cast<typename decltype(buf)::difference_type>(kHashSize));
        std::ranges::copy(rowBuffer_, out_it);

        auto digest = detail::sha256::sha256_digest(buf.data(), buf.size());
        hashVector_.push_back(digest);

        rowBuffer_.fill(0);
        rowIndex_ = 0;
    }
} // namespace crsce::common
