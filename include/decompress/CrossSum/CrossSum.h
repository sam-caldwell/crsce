/**
 * @file CrossSum.h
 * @brief Strong types for decompressor cross-sum families with basic metrics.
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "decompress/Csm/Csm.h"
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"

namespace crsce::decompress {

    enum class CrossSumFamily : std::uint8_t { Row, Col, Diag, XDiag };

    class CrossSum {
    public:
        static constexpr std::size_t kS = Csm::kS;
        using Vec = std::array<std::uint16_t, kS>;

        CrossSum(CrossSumFamily fam, const Vec &targets);

        [[nodiscard]] CrossSumFamily family() const noexcept;
        [[nodiscard]] const Vec &targets() const noexcept;

        // Optional helpers (not required in hot paths today)
        [[nodiscard]] Vec compute_counts(const Csm &csm) const;
        [[nodiscard]] Vec compute_deficit(const Csm &csm) const;
        [[nodiscard]] std::uint16_t error_min(const Csm &csm) const;
        [[nodiscard]] std::uint16_t error_max(const Csm &csm) const;
        [[nodiscard]] int satisfied_pct(const Csm &csm) const;
        [[nodiscard]] Vec extract_unknowns(const ConstraintState &st) const;

        // Emit aggregate metrics for this family using o11y (min/max error, sat pct, unknown sum if provided)
        void emit_metrics(const char *prefix, const Csm &csm, const ConstraintState *st = nullptr) const;

    private:
        CrossSumFamily fam_;
        Vec tgt_{};
    };

    // Aggregate of four families for a block
    class CrossSums {
    public:
        static constexpr std::size_t kS = Csm::kS;

        static CrossSums from_packed(std::span<const std::uint8_t> packed);

        CrossSums(const CrossSum &lsm, const CrossSum &vsm, const CrossSum &dsm, const CrossSum &xsm);

        [[nodiscard]] const CrossSum &lsm() const noexcept;
        [[nodiscard]] const CrossSum &vsm() const noexcept;
        [[nodiscard]] const CrossSum &dsm() const noexcept;
        [[nodiscard]] const CrossSum &xsm() const noexcept;

        // Emit aggregate metrics for all families using a common prefix
        void emit_metrics(const char *prefix, const Csm &csm, const ConstraintState *st = nullptr) const;

    private:
        CrossSum lsm_;
        CrossSum vsm_;
        CrossSum dsm_;
        CrossSum xsm_;
    };
} // namespace crsce::decompress
