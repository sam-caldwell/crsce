/**
 * @file Csm_ctor.cpp
 * @brief Implementation
 * @author Sam Caldwell
 * @copyright (c) 2026 Sam Caldwell. See License.txt for details.
 */
#include "decompress/Csm/Csm.h"
#include <cstddef>
#include <atomic>

namespace crsce::decompress {
    /**
     * @name Csm
     * @brief class constructor
     */
    Csm::Csm() {
        reset();
    }
} // namespace crsce::decompress
