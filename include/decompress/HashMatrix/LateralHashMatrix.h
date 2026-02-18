/**
 * @file LateralHashMatrix.h
 * @brief Strong type for lateral hash (row digest chain) payload.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace crsce::decompress::hashes {
    class LateralHashMatrix {
    public:
        explicit LateralHashMatrix(std::span<const std::uint8_t> bytes) noexcept : bytes_(bytes) {}
        [[nodiscard]] std::size_t size() const noexcept { return bytes_.size(); }
        [[nodiscard]] std::span<const std::uint8_t> bytes() const noexcept { return bytes_; }
    private:
        std::span<const std::uint8_t> bytes_;
    };
}

