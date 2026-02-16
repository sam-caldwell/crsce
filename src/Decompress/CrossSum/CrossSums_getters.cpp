/**
 * @file CrossSums_getters.cpp
 */
#include "decompress/CrossSum/CrossSum.h"

namespace crsce::decompress {
    const CrossSum &CrossSums::lsm() const noexcept { return lsm_; }
    const CrossSum &CrossSums::vsm() const noexcept { return vsm_; }
    const CrossSum &CrossSums::dsm() const noexcept { return dsm_; }
    const CrossSum &CrossSums::xsm() const noexcept { return xsm_; }
}

