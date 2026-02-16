/**
 * @file family_label.h
 * @brief Map CrossSumFamily to short string label.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#pragma once

#include "decompress/CrossSum/CrossSumFamily.h"

namespace crsce::decompress {
    /**
     * @name family_label
     * @brief Return short label for a cross-sum family (lsm/vsm/dsm/xsm).
     * @param f CrossSumFamily enum value.
     * @return const char* Label string.
     */
    inline const char *family_label(const CrossSumFamily f) {
        switch (f) {
            case CrossSumFamily::Row: return "lsm";
            case CrossSumFamily::Col: return "vsm";
            case CrossSumFamily::Diag: return "dsm";
            case CrossSumFamily::XDiag: return "xsm";
        }
        return "cs";
    }
}

