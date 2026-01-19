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
 * @brief Read all bits from a FileBitSerializer until EOF.
 * @usage auto bits = read_all(serializer);
 * @throws None
 * @param serializer Reference to an initialized FileBitSerializer.
 * @return std::vector<bool> containing all bits in order.
 */
inline std::vector<bool> read_all(crsce::common::FileBitSerializer& serializer) {
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
