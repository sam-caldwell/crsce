/**
 * @file Csm.h (forwarder)
 * @brief Compatibility forwarder to public CSM header.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once
#include "decompress/Csm/Csm.h"

namespace crsce::decompress {
    /**
     * @brief ODPH sentinel (inline, internal linkage when included elsewhere).
     */
    inline void odph_sentinel_csm_header() {}
}
