/**
 * @file event.h
 * @brief Emit named events with optional tags (declaration).
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <initializer_list>
#include <string>
#include <utility>

namespace crsce::o11y {
    /**
     * @name event
     * @brief Emit a one-off event with optional tags.
     * @param name Event name.
     * @param tags Optional key/value tags.
     * @return void
     */
    void event(const std::string &name,
               std::initializer_list<std::pair<std::string, std::string>> tags = {});
}

