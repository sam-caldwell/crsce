/**
 * @file ObjField.h
 * @brief Field type for structured metrics.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <cstdint>
#include <string>

namespace crsce::o11y {
    /**
     * @name ObjField
     * @brief Tagged union holding one named field value.
     */
    struct ObjField {
        enum class Type : std::uint8_t { I64, F64, STR, BOOL };
        Type t{Type::STR};
        std::string k;
        std::int64_t i64{0};
        double f64{0.0};
        std::string str;
        bool b{false};
    };
}

