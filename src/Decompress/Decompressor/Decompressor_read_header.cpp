/**
 * @file Decompressor_read_header.cpp
 * @brief Implementation of Decompressor::read_header.
 * © Sam Caldwell. See LICENSE.txt for details.
 */
#include "decompress/Decompressor/Decompressor.h"
#include "decompress/Decompressor/HeaderV1Fields.h"
#include <array>
#include <cstdint> // NOLINT
#include <ios> // NOLINT

namespace crsce::decompress {
    /**
     * @name Decompressor::read_header
     * @brief Read and parse the header into the fields structure.
     * @param out Parsed fields on success.
     * @return bool True on success; false on I/O or parse error.
     */
    bool Decompressor::read_header(HeaderV1Fields &out) {
        std::array<std::uint8_t, kHeaderSize> hdr{};
        in_.read(reinterpret_cast<char *>(hdr.data()), static_cast<std::streamsize>(hdr.size())); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        if (in_.gcount() != static_cast<std::streamsize>(hdr.size())) {
            return false;
        }
        if (!parse_header(hdr, out)) {
            return false;
        }
        blocks_remaining_ = out.block_count;
        return true;
    }
} // namespace crsce::decompress
