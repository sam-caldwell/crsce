/**
 * @file Csm_ctor.cpp
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "decompress/Csm/Csm.h"

namespace crsce::decompress {
    Csm::Csm() : bits_(kBytes, 0U), locks_(kTotalBits, 0U), data_(kTotalBits, 0.0) {}
} // namespace crsce::decompress
