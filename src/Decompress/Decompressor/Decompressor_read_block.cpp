/**
 * @file Decompressor_read_block.cpp
 * @brief Implementation of Decompressor::read_block.
 * © Sam Caldwell. See LICENSE.txt for details.
 */
#include "decompress/Decompressor/Decompressor.h"
#include <cstdint>
#include <ios>
#include <optional>
#include <vector>

namespace crsce::decompress {
    /**
     * @name Decompressor::read_block
     * @brief Read the next block payload (LH + sums) from the stream.
     * @return std::optional<std::vector<std::uint8_t>> Block bytes or nullopt on EOF/short read.
     */
    std::optional<std::vector<std::uint8_t> > Decompressor::read_block() {
        if (blocks_remaining_ == 0) {
            return std::nullopt;
        }
        std::vector<std::uint8_t> buf(kBlockBytes);
        in_.read(reinterpret_cast<char *>(buf.data()), static_cast<std::streamsize>(buf.size())); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        if (in_.gcount() != static_cast<std::streamsize>(buf.size())) {
            return std::nullopt;
        }
        --blocks_remaining_;
        return buf;
    }
} // namespace crsce::decompress
