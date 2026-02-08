/**
 * @file metric_obj_emit.h
 * @brief Emit structured object metric (declaration).
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include "common/O11y/Obj.h"

namespace crsce::o11y {
    /**
     * @name metric
     * @brief Emit structured object metric.
     * @param o Object with name and fields.
     * @return void
     */
    void metric(const Obj &o);
}

