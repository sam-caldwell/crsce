/**
 * @file obj_add_i64.cpp
 * @brief Obj::add for integral field.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "common/O11y/metric.h"

#include <cstdint>
#include <string>
#include <utility>

namespace crsce::o11y {
    /**
     * @name Obj::add
     * @brief Add integer field to object.
     * @param key Field name.
     * @param v Integer value.
     * @return Obj& Reference to this for chaining.
     */
    Obj &Obj::add(const std::string &key, std::int64_t v) {
        ObjField f; f.t = ObjField::Type::I64; f.k = key; f.i64 = v; fields_.push_back(std::move(f)); return *this;
    }
}
