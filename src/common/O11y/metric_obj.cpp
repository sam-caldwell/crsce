/**
 * @file metric_obj.cpp
 * @brief Emit structured object metric.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "common/O11y/Obj.h"
#include "common/O11y/detail/escape_json.h"
#include "common/O11y/detail/now_ms.h"
#include "common/O11y/detail/write_line_sync.h"

#include <ios>
#include <sstream>

namespace crsce::o11y {
    /**
     * @name metric
     * @brief Emit structured object metric.
     * @param o Object with name and fields.
     * @return void
     */
    void metric(const Obj &o) { // NOLINT(misc-use-internal-linkage)
        std::ostringstream oss; oss.setf(std::ios::fixed); oss.precision(6);
        oss << '{';
        oss << "\"ts_ms\":" << detail::now_ms() << ',';
        oss << "\"name\":\"" << detail::escape_json(o.name()) << "\",";
        oss << "\"fields\":{";
        bool first = true;
        for (const auto &f : o.fields()) {
            if (!first) { oss << ','; }
            first = false;
            oss << "\"" << detail::escape_json(f.k) << "\":";
            switch (f.t) {
                case ObjField::Type::I64: oss << f.i64; break;
                case ObjField::Type::F64: oss << f.f64; break;
                case ObjField::Type::STR: oss << '"' << detail::escape_json(f.str) << '"'; break;
                case ObjField::Type::BOOL: oss << (f.b ? "true" : "false"); break;
            }
        }
        oss << "}}";
        detail::write_line_sync(oss.str());
    }
}
