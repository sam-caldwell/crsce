/**
 * @file read_all.h
 * @brief Drain a FileBitSerializer into a vector of bool bits.
 */
#pragma once

#include "common/FileBitSerializer/FileBitSerializer.h"

#include <optional>
#include <vector>

/**
 * @name read_all
 * @brief Pop all available bits from FileBitSerializer into a vector<bool>.
 * @param serializer Source bit serializer (consumed).
 * @return Vector of bits in read order.
 */
inline std::vector<bool> read_all(crsce::common::FileBitSerializer &serializer) {
    std::vector<bool> bits;
    while (true) {
        auto b = serializer.pop();
        if (!b.has_value()) {
            break;
        }
        bits.push_back(*b);
    }
    return bits;
}
