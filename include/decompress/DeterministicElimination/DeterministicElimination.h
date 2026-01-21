/**
 * @file DeterministicElimination.h
 * @brief Deterministic elimination solver implementing sound forced moves.
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

#include "decompress/Csm/Csm.h"
#include "decompress/Solver/Solver.h"

namespace crsce::decompress {
    struct ConstraintState {
        std::array<std::uint16_t, Csm::kS> R_row{};
        std::array<std::uint16_t, Csm::kS> R_col{};
        std::array<std::uint16_t, Csm::kS> R_diag{};
        std::array<std::uint16_t, Csm::kS> R_xdiag{};
        std::array<std::uint16_t, Csm::kS> U_row{};
        std::array<std::uint16_t, Csm::kS> U_col{};
        std::array<std::uint16_t, Csm::kS> U_diag{};
        std::array<std::uint16_t, Csm::kS> U_xdiag{};
    };

    /**
     * @class DeterministicElimination
     * @brief Enforces forced moves: if R=0 -> all remaining vars become 0; if R=U -> all remaining vars become 1.
     *        Maintains feasibility (0 ≤ R ≤ U) for all four line families; throws on contradictions.
     */
    class DeterministicElimination : public Solver {
    public:
        DeterministicElimination(Csm &csm, ConstraintState &state);

        std::size_t solve_step() override;

        [[nodiscard]] bool solved() const override;

        void reset() override;

        /**
         * @brief Placeholder for hash-based deterministic elimination pass.
         *        Intended to lock rows identified by LH/lookup; currently a no-op.
         * @return Number of newly solved bits (currently always 0).
         */
        std::size_t hash_step();

    private:
        Csm &csm_;
        ConstraintState &st_;

        static constexpr std::size_t S = Csm::kS;

        static constexpr std::size_t diag_index(std::size_t r, std::size_t c) noexcept {
            return (r + c) % S;
        }

        static constexpr std::size_t xdiag_index(std::size_t r, std::size_t c) noexcept {
            return (r >= c) ? (r - c) : (r + S - c);
        }

        static void validate_bounds(const ConstraintState &st);

        void force_row(std::size_t r, bool value, std::size_t &progress);

        void force_col(std::size_t c, bool value, std::size_t &progress);

        void force_diag(std::size_t d, bool value, std::size_t &progress);

        void force_xdiag(std::size_t x, bool value, std::size_t &progress);

        void apply_cell(std::size_t r, std::size_t c, bool value);
    };
} // namespace crsce::decompress
