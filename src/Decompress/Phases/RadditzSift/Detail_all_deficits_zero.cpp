/**
 * @file Detail_all_deficits_zero.cpp
 * @brief all_deficits_zero for Radditz helpers.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include <vector>
#include "decompress/Phases/RadditzSift/all_deficits_zero.h"

namespace crsce::decompress::phases::detail {
    /**
     * @name all_deficits_zero
     * @brief Return true if every entry in the deficit vector is zero.
     * @param deficit Column deficit values.
     * @return bool True if all are zero; otherwise false.
     */
    bool all_deficits_zero(const std::vector<int> &deficit) {
        for (const int v : deficit) { // NOLINT(readability-use-anyofallof,modernize-use-ranges)
            if (v != 0) { return false; }
        }
        return true;
    }
}
