/**
 * @file seed_initial_beliefs.cpp
 */
#include "decompress/Block/detail/seed_initial_beliefs.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <utility>
#include <vector>
#include <functional>
#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Block/detail/read_seed_or_default.h"
#include "decompress/Utils/detail/calc_d.h"
#include "decompress/Utils/detail/calc_x.h"

namespace crsce::decompress::detail {
    std::uint64_t seed_initial_beliefs(Csm &csm, const ConstraintState &st) {
        constexpr std::size_t S = Csm::kS;
        const std::uint64_t belief_seed_used = read_seed_or_default(0x0BADC0FFEEULL);
        std::mt19937_64 rng(belief_seed_used);
        std::uniform_real_distribution<double> noise(-0.02, 0.02);
        for (std::size_t r = 0; r < S; ++r) {
            if (st.U_row.at(r) == 0) { continue; }
            const auto ones = static_cast<std::size_t>(st.R_row.at(r));
            std::vector<std::pair<double, std::size_t> > cand;
            cand.reserve(S);
            for (std::size_t c = 0; c < S; ++c) {
                if (csm.is_locked(r, c)) { continue; }
                const std::size_t d = ::crsce::decompress::detail::calc_d(r, c);
                const std::size_t x = ::crsce::decompress::detail::calc_x(r, c);
                const double col_need = static_cast<double>(st.R_col.at(c)) /
                                        static_cast<double>(std::max<std::uint16_t>(1, st.U_col.at(c)));
                const double diag_need = static_cast<double>(st.R_diag.at(d)) /
                                         static_cast<double>(std::max<std::uint16_t>(1, st.U_diag.at(d)));
                const double x_need = static_cast<double>(st.R_xdiag.at(x)) /
                                      static_cast<double>(std::max<std::uint16_t>(1, st.U_xdiag.at(x)));
                const double score = col_need + diag_need + x_need;
                cand.emplace_back(score, c);
            }
            if (cand.empty()) { continue; }
            std::ranges::sort(cand, std::greater<>{});
            const std::size_t pick = std::min<std::size_t>(ones, cand.size());
            std::vector<char> chosen(S, 0);
            for (std::size_t i = 0; i < pick; ++i) { chosen[cand[i].second] = 1; }
            for (std::size_t c = 0; c < S; ++c) {
                if (csm.is_locked(r, c)) { continue; }
                constexpr double high = 0.90;
                constexpr double low = 0.10;
                const double base = chosen[c] ? high : low;
                double v = base + noise(rng);
                v = std::clamp(v, 0.0, 1.0);
                csm.set_data(r, c, v);
            }
        }
        return belief_seed_used;
    }
}
