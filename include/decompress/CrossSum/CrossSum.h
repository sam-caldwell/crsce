/**
 * @file CrossSum.h
 * @brief Strong type representing a single cross-sum family with helpers.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "decompress/CrossSum/CrossSumFamily.h"
#include "decompress/Csm/Csm.h"
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"

namespace crsce::decompress {
    /**
     * @class CrossSum
     * @brief Maintains target vector and helpers for one cross-sum family.
     */
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

        // Emit aggregate metrics for this family using o11y
        void emit_metrics(const char *prefix, const Csm &csm, const ConstraintState *st = nullptr) const;

    private:
        CrossSumFamily fam_;
        Vec tgt_{};
    };
}
