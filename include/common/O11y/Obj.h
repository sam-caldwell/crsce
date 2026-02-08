/**
 * @file Obj.h
 * @brief Structured metric object type and builder API.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <string>
#include <utility>
#include <vector>
#include "common/O11y/ObjField.h"

namespace crsce::o11y {
    /**
     * @name Obj
     * @brief Structured object for metrics with typed fields.
     */
    struct Obj {
    private:
        std::string name_;
        std::vector<ObjField> fields_;
    public:
        explicit Obj(std::string n) : name_(std::move(n)) {}
        Obj &add(const std::string &key, std::int64_t v);
        Obj &add(const std::string &key, double v);
        Obj &add(const std::string &key, const std::string &v);
        Obj &add(const std::string &key, bool v);
        [[nodiscard]] const std::string &name() const noexcept { return name_; }
        [[nodiscard]] const std::vector<ObjField> &fields() const noexcept { return fields_; }
    };
}

