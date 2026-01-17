/**
 * @file FileBitSerializer.h
 * @brief Buffered bitwise reader for files (1 KiB chunked, MSB-first per byte).
 */
#pragma once

#include <cstddef>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace crsce::common {

/**
 * @class FileBitSerializer
 * @brief Provides a bit-by-bit interface over a file.
 *
 * Reads input in 1 KiB chunks and yields bits MSB-first from each byte. The
 * consumer calls pop() until it returns std::nullopt indicating EOF.
 */
class FileBitSerializer {
 public:
  static constexpr std::size_t kChunkSize = 1 * 1024;  ///< Buffer size (1 KiB)

  /**
   * @brief Construct a bit serializer on a given path.
   * @param path File system path to open in binary mode.
   */
  explicit FileBitSerializer(const std::string& path);

  /**
   * @brief Check if at least one more bit can be produced.
   * @return true if another bit is available; false at EOF.
   */
  bool has_next();

  /**
   * @brief Pop the next bit (MSB-first); returns nullopt at EOF.
   */
  std::optional<bool> pop();

  /**
   * @brief True if the underlying file stream opened successfully.
   */
  bool good() const { return in_.good(); }

 private:
  /**
   * @brief Fill the internal buffer by reading from the stream.
   * @return true if data was read; false on EOF or failure.
   */
  bool fill();

  std::ifstream in_;
  std::vector<char> buf_;
  std::size_t buf_len_{0};
  std::size_t byte_pos_{0};
  int bit_pos_{0};   ///< Bit index in current byte [0..7], 0 is MSB
  bool eof_{false};
};

}  // namespace crsce::common
