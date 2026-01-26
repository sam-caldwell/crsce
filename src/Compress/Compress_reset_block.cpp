/**
 * @file Compress_reset_block.cpp
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "compress/Compress/Compress.h"
#include <cstddef>

namespace crsce::compress {

/**
 * @name Compress::reset_block
 * @brief Reset all accumulators and state for a new block.
 * @return void
 */
void Compress::reset_block() {
  lsm_.reset();
  vsm_.reset();
  dsm_.reset();
  xsm_.reset();
  lh_ = crsce::common::BitHashBuffer(seed_);
  r_ = 0;
  c_ = 0;
  row_bit_count_ = 0;
  total_bits_ = 0;
}

} // namespace crsce::compress
