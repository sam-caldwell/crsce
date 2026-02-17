/**
 * @file CrossSum_emit_metrics.cpp
 * @brief Emit CrossSum family metrics via O11y.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "decompress/CrossSum/CrossSumType.h"  // CrossSum
#include "decompress/Csm/Csm.h"  // Csm
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"
#include "common/O11y/O11y.h"
#include <string>
#include <cstdint>
#include "decompress/CrossSum/detail/family_label.h"

namespace crsce::decompress {
    /**
     * @name emit_metrics
     * @brief Emit min/max error, satisfaction percent, and optional unknown total.
     * @param prefix Metric name prefix (e.g., "crosssum").
     * @param csm Reference to CSM for computing error/satisfaction.
     * @param st Optional constraint state; if provided, emits unknown_total.
     * @return void
     */
    void CrossSum::emit_metrics(const char *prefix, const Csm &csm, const ConstraintState *st) const {
        const char *fam = family_label(fam_);
        const auto min_e = static_cast<std::int64_t>(error_min(csm));
        const auto max_e = static_cast<std::int64_t>(error_max(csm));
        const auto pct   = static_cast<std::int64_t>(satisfied_pct(csm));
        ::crsce::o11y::O11y::instance().metric(std::string(prefix) + ".min_err", static_cast<std::int64_t>(min_e), {{"family", fam}});
        ::crsce::o11y::O11y::instance().metric(std::string(prefix) + ".max_err", static_cast<std::int64_t>(max_e), {{"family", fam}});
        ::crsce::o11y::O11y::instance().metric(std::string(prefix) + ".sat_pct", static_cast<std::int64_t>(pct), {{"family", fam}});
        if (st != nullptr) {
            const auto U = extract_unknowns(*st);
            std::uint64_t sumU = 0ULL; for (auto v : U) { sumU += v; }
            ::crsce::o11y::O11y::instance().metric(std::string(prefix) + ".unknown_total",
                                  static_cast<std::int64_t>(sumU), {{"family", fam}});
        }
    }
}
