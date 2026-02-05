/**
 * @file RowHashVerifier_verify_all.cpp
 * @brief Implementation of RowHashVerifier::verify_all.
 */
#include "decompress/RowHashVerifier/RowHashVerifier.h"
#include "decompress/Csm/detail/Csm.h"

#include <cstdint>
#include <span>

#include <span>

namespace crsce::decompress {
    bool RowHashVerifier::verify_all(const Csm &csm,
                                     const std::span<const std::uint8_t> lh_bytes) const {
        return verify_rows(csm, lh_bytes, kS);
    }
}
