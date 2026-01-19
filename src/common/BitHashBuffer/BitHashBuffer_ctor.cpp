/**
 * @file BitHashBuffer_ctor.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief Constructor for BitHashBuffer: derive seed hash and reset state.
 */
#include "../../../include/common/BitHashBuffer/BitHashBuffer.h"
#include "common/BitHashBuffer/detail/Sha256.h"

#include <cstdint>
#include <string>
#include <vector>

namespace crsce::common {

/**
 * @name BitHashBuffer
 * @brief Construct with a seed string and initialize state.
 * @usage BitHashBuffer buf("seed");
 * @throws None
 * @param seed Seed string; hashed to produce the initial seedHash.
 * @return N/A
 */
BitHashBuffer::BitHashBuffer(const std::string &seed) {
  const std::vector<std::uint8_t> seed_bytes(seed.begin(), seed.end());
  seedHash_ =
      detail::sha256::sha256_digest(seed_bytes.data(), seed_bytes.size());
}

} // namespace crsce::common
