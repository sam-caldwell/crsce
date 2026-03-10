/**
 * @file Csm_ctor.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Csm default constructor implementation.
 */
#include "common/Csm/Csm.h"

namespace crsce::common {
    /**
     * @name Csm
     * @brief Construct a zero-initialized kS x kS binary matrix.
     * @return N/A
     * @throws None
     */
    Csm::Csm() : rows_{} {
        // Value-initialization zeros every element of the nested std::array.
    }
} // namespace crsce::common
