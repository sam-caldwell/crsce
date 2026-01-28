/**
 * @file read_file_to_vec.h
 * @brief Read a whole file into a byte vector.
 */
#pragma once

#include <cstddef>
#include <fstream>
#include <ios>
#include <string>
#include <vector>

namespace crsce::testhelpers::tlt {
    /**
     * @name read_file_to_vec
     * @brief Read entire file contents into vector<char>.
     * @param path File to read.
     * @param out Output vector; resized to file size.
     * @return True on success; false on I/O failure or short read.
     */
    inline bool read_file_to_vec(const std::string &path, std::vector<char> &out) {
        std::ifstream f(path, std::ios::binary);
        if (!f.good()) { return false; }
        f.seekg(0, std::ios::end);
        const auto sz = static_cast<std::size_t>(f.tellg());
        f.seekg(0, std::ios::beg);
        out.resize(sz);
        if (sz > 0) {
            f.read(out.data(), static_cast<std::streamsize>(sz));
            if (f.gcount() != static_cast<std::streamsize>(sz)) { return false; }
        }
        return true;
    }
} // namespace crsce::testhelpers::tlt
