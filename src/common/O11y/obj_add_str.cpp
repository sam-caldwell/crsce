/**
 * @file obj_add_str.cpp
 * @brief Obj::add for string field.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "common/O11y/metric.h"

#include <string>
#include <utility>

namespace crsce::o11y {
    /**
     * @name Obj::add
     * @brief Add string field to object.
     * @param key Field name.
     * @param v String value.
     * @return Obj& Reference to this for chaining.
     */
    Obj &Obj::add(const std::string &key, const std::string &v) {
        ObjField f; f.t = ObjField::Type::STR; f.k = key; f.str = v; fields_.push_back(std::move(f)); return *this;
    }
}
