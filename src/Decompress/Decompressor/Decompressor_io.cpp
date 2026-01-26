/**
 * @file Decompressor_io.cpp
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "decompress/Decompressor/Decompressor.h"
#include "decompress/Decompressor/HeaderV1Fields.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <ios>
#include <optional>
#include <string>
#include <vector>

namespace crsce::decompress {
    /**
     * @brief Implementation detail.
     */
    Decompressor::Decompressor(const std::string &input_path)
        : in_(input_path, std::ios::binary) {
    }

    Decompressor::Decompressor(const std::string &input_path, const std::string &output_path)
        : in_(input_path, std::ios::binary), output_path_(output_path) {
    }

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
