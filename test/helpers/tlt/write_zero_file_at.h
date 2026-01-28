/**
 * @file write_zero_file_at.h
 * @brief Create a file of fixed length filled with zero bytes.
 */
#pragma once

#include <cstddef>
#include <fstream>
#include <ios>
#include <string>
#include <vector>

namespace crsce::testhelpers::tlt {
    /**
     * @name write_zero_file_at
     * @brief Write a file of given size filled with zeros.
     * @param path Destination file path.
     * @param bytes Number of bytes to write.
     * @return True on success; false on I/O failure.
     */
    inline bool write_zero_file_at(const std::string &path, const std::size_t bytes) {
        std::vector<char> buf(bytes, 0);
        std::ofstream f(path, std::ios::binary);
        if (!f.good()) { return false; }
        if (!buf.empty()) {
            f.write(buf.data(), static_cast<std::streamsize>(buf.size()));
        }
        return f.good();
    }
} // namespace crsce::testhelpers::tlt
