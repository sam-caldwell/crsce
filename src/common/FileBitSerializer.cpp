/**
 * @file FileBitSerializer.cpp
 * @brief Implementation of the 1 KiB-buffered bitwise file reader.
 */
#include "common/FileBitSerializer.h"

#include <string>
#include <ios>
#include <optional>
#include <cstddef>
#include <cstdint>

namespace crsce::common {

/** @brief Open a file and initialize read cursor state. */
FileBitSerializer::FileBitSerializer(const std::string& path) // GCOVR_EXCL_LINE
    : in_(path, std::ios::binary),
      buf_(kChunkSize) {}

/** @brief Refill the buffer from the stream if possible. */
bool FileBitSerializer::fill() { // GCOVR_EXCL_LINE
  // ReSharper disable once CppDFAConstantConditions
  if (!in_.good() || eof_) {
    return false; // GCOVR_EXCL_LINE
  }
  in_.read(buf_.data(), static_cast<std::streamsize>(kChunkSize)); // GCOVR_EXCL_LINE
  const std::streamsize got = in_.gcount(); // GCOVR_EXCL_LINE
  if (got <= 0) { // GCOVR_EXCL_LINE
    eof_ = true;
    buf_len_ = 0;
    return false;
  }
  buf_len_ = static_cast<std::size_t>(got);
  byte_pos_ = 0;
  bit_pos_ = 0;
  return true;
}

/** @brief Return true if another bit is available (may trigger a read). */
bool FileBitSerializer::has_next() {
  if (byte_pos_ < buf_len_) {
    return true;
  }
  if (eof_) {
    return false;
  }
  return fill() && (buf_len_ > 0);
}

/** @brief Pop a single bit from the front of the stream (MSB-first). */
std::optional<bool> FileBitSerializer::pop() {
  if (!has_next()) {
    return std::nullopt;
  }

  // Extract bit MSB-first within the current byte
  const auto byte = static_cast<std::uint8_t>(static_cast<unsigned char>(buf_[byte_pos_]));
  // NOLINTNEXTLINE(readability-magic-numbers)
  bool bit = static_cast<bool>((byte >> (7 - bit_pos_)) & 0x1);

  // Advance bit/byte positions
  ++bit_pos_;
  // NOLINTNEXTLINE(readability-magic-numbers)
  if (bit_pos_ >= 8) {
    bit_pos_ = 0;
    ++byte_pos_;
  }
  return bit;
}

}  // namespace crsce::common
