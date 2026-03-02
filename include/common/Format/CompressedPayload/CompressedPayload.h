/**
 * @file CompressedPayload.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief CompressedPayload class for CRSCE block serialization and deserialization.
 *
 * Each compressed block contains 511 lateral-hash digests, a diagnostics/info byte (DI),
 * and four cross-sum vectors (LSM, VSM, DSM, XSM) packed into a fixed-size
 * 19,549-byte payload.  LSM and VSM use 9-bit fixed-width encoding; DSM and XSM
 * use variable-width encoding based on the diagonal length at each index.
 */
#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace crsce::common::format {

    /**
     * @class CompressedPayload
     * @name CompressedPayload
     * @brief Holds and serializes one CRSCE compressed block (19,549 bytes).
     * @details
     * Block layout:
     *   Field   Elements  Bits/Element  Total Bits   Encoding
     *   LH      511       256           130,816      32 bytes per digest, sequential
     *   DI      1         8             8            uint8
     *   LSM     511       9             4,599        MSB-first packed bitstream
     *   VSM     511       9             4,599        MSB-first packed bitstream
     *   DSM     1,021     variable      8,185        MSB-first, ceil(log2(len(d)+1))
     *   XSM     1,021     variable      8,185        MSB-first, ceil(log2(len(d)+1))
     *   Total                           156,392 bits = 19,549 bytes
     */
    class CompressedPayload {
    public:
        /**
         * @name kS
         * @brief Matrix dimension (number of rows and columns).
         */
        static constexpr std::uint16_t kS = 511;

        /**
         * @name kBlockPayloadBytes
         * @brief Exact size of one serialized block in bytes.
         */
        static constexpr std::size_t kBlockPayloadBytes = 19549;

        /**
         * @name kDiagCount
         * @brief Number of diagonals (and anti-diagonals) in a kS x kS matrix: 2*kS - 1.
         */
        static constexpr std::uint16_t kDiagCount = (2 * kS) - 1;

        /**
         * @name CompressedPayload
         * @brief Construct a zero-initialized payload with kS LH slots and cross-sum vectors.
         * @throws None
         */
        CompressedPayload();

        // -- LH accessors --

        /**
         * @name setLH
         * @brief Store a 32-byte SHA-256 digest for row r.
         * @param r Row index in [0, kS).
         * @param digest The 32-byte digest to store.
         * @throws std::out_of_range if r >= kS.
         */
        void setLH(std::uint16_t r, const std::array<std::uint8_t, 32> &digest);

        /**
         * @name getLH
         * @brief Retrieve the stored 32-byte SHA-256 digest for row r.
         * @param r Row index in [0, kS).
         * @return Copy of the 32-byte digest.
         * @throws std::out_of_range if r >= kS.
         */
        [[nodiscard]] std::array<std::uint8_t, 32> getLH(std::uint16_t r) const;

        // -- DI accessor --

        /**
         * @name setDI
         * @brief Set the diagnostics/info byte.
         * @param di The value to store.
         * @throws None
         */
        void setDI(std::uint8_t di);

        /**
         * @name getDI
         * @brief Get the diagnostics/info byte.
         * @return The stored DI value.
         * @throws None
         */
        [[nodiscard]] std::uint8_t getDI() const;

        // -- LSM accessors --

        /**
         * @name setLSM
         * @brief Set the lateral (row) sum at index k.
         * @param k Index in [0, kS).
         * @param value Sum value (max 511, fits in 9 bits).
         * @throws std::out_of_range if k >= kS.
         */
        void setLSM(std::uint16_t k, std::uint16_t value);

        /**
         * @name getLSM
         * @brief Get the lateral (row) sum at index k.
         * @param k Index in [0, kS).
         * @return The stored sum value.
         * @throws std::out_of_range if k >= kS.
         */
        [[nodiscard]] std::uint16_t getLSM(std::uint16_t k) const;

        // -- VSM accessors --

        /**
         * @name setVSM
         * @brief Set the vertical (column) sum at index k.
         * @param k Index in [0, kS).
         * @param value Sum value (max 511, fits in 9 bits).
         * @throws std::out_of_range if k >= kS.
         */
        void setVSM(std::uint16_t k, std::uint16_t value);

        /**
         * @name getVSM
         * @brief Get the vertical (column) sum at index k.
         * @param k Index in [0, kS).
         * @return The stored sum value.
         * @throws std::out_of_range if k >= kS.
         */
        [[nodiscard]] std::uint16_t getVSM(std::uint16_t k) const;

        // -- DSM accessors --

        /**
         * @name setDSM
         * @brief Set the diagonal sum at index k.
         * @param k Index in [0, kDiagCount).
         * @param value Sum value.
         * @throws std::out_of_range if k >= kDiagCount.
         */
        void setDSM(std::uint16_t k, std::uint16_t value);

        /**
         * @name getDSM
         * @brief Get the diagonal sum at index k.
         * @param k Index in [0, kDiagCount).
         * @return The stored sum value.
         * @throws std::out_of_range if k >= kDiagCount.
         */
        [[nodiscard]] std::uint16_t getDSM(std::uint16_t k) const;

        // -- XSM accessors --

        /**
         * @name setXSM
         * @brief Set the anti-diagonal sum at index k.
         * @param k Index in [0, kDiagCount).
         * @param value Sum value.
         * @throws std::out_of_range if k >= kDiagCount.
         */
        void setXSM(std::uint16_t k, std::uint16_t value);

        /**
         * @name getXSM
         * @brief Get the anti-diagonal sum at index k.
         * @param k Index in [0, kDiagCount).
         * @return The stored sum value.
         * @throws std::out_of_range if k >= kDiagCount.
         */
        [[nodiscard]] std::uint16_t getXSM(std::uint16_t k) const;

        // -- Serialization --

        /**
         * @name serializeBlock
         * @brief Serialize this payload into a kBlockPayloadBytes-byte buffer.
         * @return Vector of exactly kBlockPayloadBytes bytes.
         * @throws None
         */
        [[nodiscard]] std::vector<std::uint8_t> serializeBlock() const;

        /**
         * @name deserializeBlock
         * @brief Deserialize a kBlockPayloadBytes-byte buffer into this payload.
         * @param data Pointer to at least kBlockPayloadBytes bytes.
         * @param len Length of the buffer (must be >= kBlockPayloadBytes).
         * @throws DecompressHeaderInvalid if len < kBlockPayloadBytes.
         */
        void deserializeBlock(const std::uint8_t *data, std::size_t len);

    private:
        /**
         * @name lh_
         * @brief Lateral hash digests: kS SHA-256 digests (32 bytes each).
         */
        std::vector<std::array<std::uint8_t, 32>> lh_;

        /**
         * @name di_
         * @brief Diagnostics/info byte.
         */
        std::uint8_t di_{0};

        /**
         * @name lsm_
         * @brief Lateral (row) sum vector: kS elements, each in [0, kS].
         */
        std::vector<std::uint16_t> lsm_;

        /**
         * @name vsm_
         * @brief Vertical (column) sum vector: kS elements, each in [0, kS].
         */
        std::vector<std::uint16_t> vsm_;

        /**
         * @name dsm_
         * @brief Diagonal sum vector: kDiagCount elements.
         */
        std::vector<std::uint16_t> dsm_;

        /**
         * @name xsm_
         * @brief Anti-diagonal sum vector: kDiagCount elements.
         */
        std::vector<std::uint16_t> xsm_;

        // -- Private bit-packing helpers --

        /**
         * @name diagLen
         * @brief Compute the number of cells on diagonal (or anti-diagonal) k.
         * @param k Diagonal index in [0, kDiagCount).
         * @return min(k+1, kS, kDiagCount - k).
         */
        static std::uint16_t diagLen(std::uint16_t k);

        /**
         * @name diagBits
         * @brief Compute the number of bits needed to encode a value on diagonal k.
         * @param k Diagonal index in [0, kDiagCount).
         * @return ceil(log2(diagLen(k) + 1)).
         */
        static std::uint8_t diagBits(std::uint16_t k);

        /**
         * @name packBits
         * @brief Write n bits of value into buf starting at the given bit offset (MSB-first).
         * @param buf Output byte buffer.
         * @param bitOffset Current bit offset into buf (updated on return).
         * @param value The value to write (only the lowest n bits are used).
         * @param n Number of bits to write (1..16).
         */
        static void packBits(std::vector<std::uint8_t> &buf, std::size_t &bitOffset,
                             std::uint16_t value, std::uint8_t n);

        /**
         * @name unpackBits
         * @brief Read n bits from data starting at the given bit offset (MSB-first).
         * @param data Input byte buffer.
         * @param bitOffset Current bit offset into data (updated on return).
         * @param n Number of bits to read (1..16).
         * @return The extracted value.
         */
        static std::uint16_t unpackBits(const std::uint8_t *data, std::size_t &bitOffset, std::uint8_t n);
    };

} // namespace crsce::common::format
