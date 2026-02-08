/**
 * @file obj_add_f64.cpp
 * @brief Obj::add for double field.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "common/O11y/metric.h"

#include <string>
#include <utility>

namespace crsce::o11y {
    /**
     * @name Obj::add
     * @brief Add double field to object.
     * @param key Field name.
     * @param v Double value.
     * @return Obj& Reference to this for chaining.
     */
    Obj &Obj::add(const std::string &key, double v) {
        ObjField f; f.t = ObjField::Type::F64; f.k = key; f.f64 = v; fields_.push_back(std::move(f)); return *this;
    }
}
