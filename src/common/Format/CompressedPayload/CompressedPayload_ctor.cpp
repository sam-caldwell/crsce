/**
 * @file CompressedPayload_ctor.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief CompressedPayload default constructor implementation.
 */
#include "common/Format/CompressedPayload/CompressedPayload.h"

#include <array>
#include <cstdint>

namespace crsce::common::format {

    /**
     * @name CompressedPayload
     * @brief Construct a zero-initialized payload with kS LH slots and cross-sum vectors.
     * @return N/A
     * @throws None
     */
    CompressedPayload::CompressedPayload()
        : lh_(kS, std::array<std::uint8_t, 32>{}),
          lsm_(kS, 0),
          vsm_(kS, 0),
          dsm_(kDiagCount, 0),
          xsm_(kDiagCount, 0) {
    }

} // namespace crsce::common::format
