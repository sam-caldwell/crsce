/**
 * @file Compress_compress_file.cpp
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "compress/Compress/Compress.h"
#include "common/Format/HeaderV1.h"
#include "common/FileBitSerializer/FileBitSerializer.h"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <cassert>

namespace crsce::compress {

/**
 * @name Compress::compress_file
 * @brief Implementation detail for writing the CRSCE v1 container.
 * @return bool True on success; false on any I/O or serialization error.
 */

bool Compress::compress_file() {
  namespace fs = std::filesystem;
  // Determine original size
  std::uint64_t orig_sz = 0;
  try {
    orig_sz = static_cast<std::uint64_t>(fs::file_size(input_path_));
  } catch (...) {
    return false;
  }
  const std::uint64_t total_bits = orig_sz * 8U;
  const auto bits_per_block = static_cast<std::uint64_t>(kBitsPerBlock);
  const std::uint64_t block_count = (total_bits == 0)
                                        ? 0
                                        : ((total_bits + bits_per_block - 1) / bits_per_block);

  std::ofstream out(output_path_, std::ios::binary);
  if (!out.good()) {
    return false;
  }

  // Write header
  const auto hdr = crsce::common::format::HeaderV1::pack(orig_sz, block_count);
  out.write(reinterpret_cast<const char *>(hdr.data()), static_cast<std::streamsize>(hdr.size())); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

  if (block_count == 0) {
    return out.good();
  }

  crsce::common::FileBitSerializer bits(input_path_);
  if (!bits.good()) {
    return false;
  }

  for (std::uint64_t b = 0; b < block_count; ++b) {
    reset_block();
    // Feed up to kBitsPerBlock data bits into this block
    std::uint64_t fed = 0;
    while (fed < bits_per_block && bits.has_next()) {
      const auto v = bits.pop();
      if (!v.has_value()) { break; } // GCOVR_EXCL_LINE (safety; has_next() guarantees value)
      push_bit(*v);
      ++fed;
    }
    // Finalize partial row if any
    pad_and_finalize_row_if_needed();
    // Finish remaining rows in the block with zeros
    while (r_ < kS) {
      push_zero_row();
    }
    // LH: must have 511 digests
    auto lh_bytes = pop_all_lh_bytes();
    assert(lh_bytes.size() == 511U * 32U && "LH digest size mismatch"); // GCOVR_EXCL_LINE
    out.write(reinterpret_cast<const char *>(lh_bytes.data()), static_cast<std::streamsize>(lh_bytes.size())); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

    // Cross-sums serialization: 4 * 575 bytes = 2300 bytes
    const auto sums = serialize_cross_sums();
    out.write(reinterpret_cast<const char *>(sums.data()), static_cast<std::streamsize>(sums.size())); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
  }

  return out.good();
}

} // namespace crsce::compress
