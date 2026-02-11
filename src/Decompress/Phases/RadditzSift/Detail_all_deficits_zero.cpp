/**
 * @file Detail_all_deficits_zero.cpp
 * @brief all_deficits_zero for Radditz helpers.
 * @author Sam Caldwell
 */
#include "decompress/Phases/RadditzSift/Detail.h"

namespace crsce::decompress::phases::detail {
    bool all_deficits_zero(const std::vector<int> &deficit) {
        for (const int v : deficit) {
            if (v != 0) { return false; }
        }
        return true;
    }
}

