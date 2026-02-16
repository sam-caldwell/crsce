/**
 * @file CrossSum_emit_metrics.cpp
 */
#include "decompress/CrossSum/CrossSum.h"
#include "decompress/Csm/Csm.h"
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"
#include "common/O11y/metric_i64.h"
#include <string>
#include <cstdint>

namespace crsce::decompress {
    namespace {
    const char *family_label(const CrossSumFamily f) {
        switch (f) {
            case CrossSumFamily::Row: return "lsm";
            case CrossSumFamily::Col: return "vsm";
            case CrossSumFamily::Diag: return "dsm";
            case CrossSumFamily::XDiag: return "xsm";
        }
        return "cs";
    }
    } // namespace

    void CrossSum::emit_metrics(const char *prefix, const Csm &csm, const ConstraintState *st) const {
        const char *fam = family_label(fam_);
        const auto min_e = static_cast<std::int64_t>(error_min(csm));
        const auto max_e = static_cast<std::int64_t>(error_max(csm));
        const auto pct   = static_cast<std::int64_t>(satisfied_pct(csm));
        ::crsce::o11y::metric(std::string(prefix) + ".min_err", min_e, {{"family", fam}});
        ::crsce::o11y::metric(std::string(prefix) + ".max_err", max_e, {{"family", fam}});
        ::crsce::o11y::metric(std::string(prefix) + ".sat_pct", pct, {{"family", fam}});
        if (st != nullptr) {
            const auto U = extract_unknowns(*st);
            std::uint64_t sumU = 0ULL; for (auto v : U) { sumU += v; }
            ::crsce::o11y::metric(std::string(prefix) + ".unknown_total",
                                  static_cast<std::int64_t>(sumU),
                                  {{"family", fam}});
        }
    }
}
