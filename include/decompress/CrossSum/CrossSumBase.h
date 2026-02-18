/**
 * @file CrossSumBase.h
 * @brief Common base for strongly-typed cross-sum matrices (LSM/VSM/DSM/XSM).
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace crsce::decompress::xsum {
    /**
     * @class CrossSumBase
     * @brief Provides a read-only view over cross-sum targets with a common interface.
     */
    class CrossSumBase {
    public:
        explicit CrossSumBase(std::span<const std::uint16_t> tgt) noexcept : tgt_(tgt) {}
        [[nodiscard]] std::size_t size() const noexcept { return tgt_.size(); }
        [[nodiscard]] std::uint16_t value(std::size_t i) const { return tgt_[i]; }
        [[nodiscard]] std::span<const std::uint16_t> targets() const noexcept { return tgt_; }
    private:
        std::span<const std::uint16_t> tgt_;
    };
}
