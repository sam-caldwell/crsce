/**
 * @file obj_add_bool.cpp
 * @brief Obj::add for boolean field.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "common/O11y/metric.h"

#include <string>
#include <utility>

namespace crsce::o11y {
    /**
     * @name Obj::add
     * @brief Add boolean field to object.
     * @param key Field name.
     * @param v Boolean value.
     * @return Obj& Reference to this for chaining.
     */
    Obj &Obj::add(const std::string &key, bool v) {
        ObjField f; f.t = ObjField::Type::BOOL; f.k = key; f.b = v; fields_.push_back(std::move(f)); return *this;
    }
}
