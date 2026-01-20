/**
 * @file Compress_reset_block.cpp
 */
#include "compress/Compress/Compress.h"
#include <cstddef>

namespace crsce::compress {

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

void Compress::push_zero_row() {
  for (std::size_t i = 0; i < kBitsPerRow; ++i) {
    push_bit(false);
  }
}

} // namespace crsce::compress
