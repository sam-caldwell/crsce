/**
 * @file Compressor_loadCsm.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Compressor::loadCsm -- load raw bytes into a CSM, row-major, MSB-first per byte.
 */
#include "compress/Compressor/Compressor.h"

#include <cstddef>
#include <cstdint>

#include "common/Csm/Csm.h"

namespace crsce::compress {

    /**
     * @name loadCsm
     * @brief Load raw bytes into a CSM, row-major, MSB-first per byte.
     * @param data Pointer to the raw byte data.
     * @param bitCount Number of valid bits to load (may be less than kBlockBits for the last block).
     * @return A populated Csm instance (unaddressed cells remain zero).
     * @throws None
     */
    common::Csm Compressor::loadCsm(const std::uint8_t *data, const std::size_t bitCount) {
        common::Csm csm;
        for (std::size_t i = 0; i < bitCount; ++i) {
            const std::size_t byteIdx = i / 8;
            const auto bitInByte = static_cast<std::uint8_t>(7 - (i % 8));
            const auto r = static_cast<std::uint16_t>(i / kS);
            const auto c = static_cast<std::uint16_t>(i % kS);
            const auto v = static_cast<std::uint8_t>((data[byteIdx] >> bitInByte) & 1); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            csm.set(r, c, v);
        }
        return csm;
    }

} // namespace crsce::compress
