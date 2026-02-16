/**
 * @file Csm_ctor.cpp
 * @brief Default constructor for CSM.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "decompress/Csm/Csm.h"
#include <cstddef> // NOLINT

namespace crsce::decompress {
    /**
     * @name Csm
     * @brief Default-construct CSM and reset contents/state.
     * @return Csm
     */
    Csm::Csm() {
        reset();
    }
} // namespace crsce::decompress
