/**
 * @file FileBitSerializer_fill.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief Implementation of FileBitSerializer::fill (refills the byte buffer).
 */
#include "common/FileBitSerializer.h"
#include <cstddef>
#include <ios>

namespace crsce::common {

/**
 * @name fill
 * @brief Fill the internal buffer by reading from the stream.
 * @usage Call fill(); returns false on EOF or error.
 * @throws None
 * @return true if data was read; false on EOF or failure.
 */
bool FileBitSerializer::fill() { // GCOVR_EXCL_LINE
  // ReSharper disable once CppDFAConstantConditions
  if (!in_.good() || eof_) {
    return false; // GCOVR_EXCL_LINE
  }
  in_.read(buf_.data(),
           static_cast<std::streamsize>(kChunkSize)); // GCOVR_EXCL_LINE
  const std::streamsize got = in_.gcount();           // GCOVR_EXCL_LINE
  if (got <= 0) {                                     // GCOVR_EXCL_LINE
    eof_ = true;
    buf_len_ = 0;
    return false;
  }
  buf_len_ = static_cast<std::size_t>(got);
  byte_pos_ = 0;
  bit_pos_ = 0;
  return true;
}

} // namespace crsce::common
