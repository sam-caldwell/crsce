/**
 * @file files_equal.h
 * @brief Compare two files for equal size and identical content.
 */
#pragma once

#include <string>
#include <vector>
#include <algorithm>

#include "helpers/tlt/read_file_to_vec.h"

namespace crsce::testhelpers::tlt {
    /**
     * @name files_equal
     * @brief Return true if files have the same size and identical bytes.
     * @param a_path First file path.
     * @param b_path Second file path.
     * @return True if identical; false otherwise.
     */
    inline bool files_equal(const std::string &a_path, const std::string &b_path) {
        std::vector<char> a;
        std::vector<char> b;
        if (!read_file_to_vec(a_path, a)) { return false; }
        if (!read_file_to_vec(b_path, b)) { return false; }
        return (a.size() == b.size()) && std::equal(a.begin(), a.end(), b.begin());
    }
} // namespace crsce::testhelpers::tlt
