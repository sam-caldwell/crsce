/**
 * @file metric.h
 * @brief Structured metric emission (JSONL) with standardized timestamping.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

// Aggregator header for O11y metrics API. This file intentionally defines no
// constructs to satisfy the OneDefinitionPerHeader rule. Include the specific
// single-construct headers below to access the full API surface.

#include "common/O11y/ObjField.h"
#include "common/O11y/Obj.h"
#include "common/O11y/metric_i64.h"
#include "common/O11y/metric_f64.h"
#include "common/O11y/metric_str.h"
#include "common/O11y/metric_bool.h"
#include "common/O11y/metric_obj_emit.h"
#include "common/O11y/counter.h"
#include "common/O11y/event.h"
#include "common/O11y/gobp_debug_enabled.h"
#include "common/O11y/debug_event_gobp.h"

namespace crsce::o11y {
    /**
     * @name MetricTag
     * @brief Tag type to satisfy OneDefinitionPerHeader for metrics API aggregation.
     */
    struct MetricTag { };
}
