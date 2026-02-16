/**
 * @file CrossSums_ctor.cpp
 */
#include "decompress/CrossSum/CrossSum.h"

namespace crsce::decompress {
    CrossSums::CrossSums(const CrossSum &lsm, const CrossSum &vsm, const CrossSum &dsm, const CrossSum &xsm)
        : lsm_(lsm), vsm_(vsm), dsm_(dsm), xsm_(xsm) {}
}

