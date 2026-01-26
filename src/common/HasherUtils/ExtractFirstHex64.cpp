/**
 * @file ExtractFirstHex64.cpp
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "common/HasherUtils/ExtractFirstHex64.h"

#include <cctype>
#include <string>

namespace crsce::common::hasher {
    /**
     * @brief Implementation detail.
     */
    bool extract_first_hex64(const std::string &src, std::string &hex_out) {
        hex_out.clear();
        std::string run;
        run.reserve(64);
        for (const char c: src) {
            const bool is_hex =
                    (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
            if (is_hex) {
                run.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
                if (run.size() == 64) {
                    hex_out = run;
                    return true;
                }
            } else if (!run.empty()) {
                run.clear();
            }
        }
        return false;
    }
} // namespace crsce::common::hasher
