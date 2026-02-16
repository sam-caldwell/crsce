/**
 * @file CrossSum.h
 * @brief Aggregate CrossSums type; includes CrossSum and enum family.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <cstddef>
#include <span>

#include "decompress/CrossSum/CrossSumFamily.h"
#include "decompress/CrossSum/CrossSumType.h"
#include "decompress/Csm/Csm.h"
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"

namespace crsce::decompress {
    // Aggregate of four families for a block
    /**
     * @class CrossSums
     * @brief Aggregates four CrossSum families and exposes convenience helpers.
     */
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
