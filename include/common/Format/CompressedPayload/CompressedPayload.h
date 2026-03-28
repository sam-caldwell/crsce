/**
 * @file CompressedPayload.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief CompressedPayload class for CRSCE block serialization and deserialization.
 *
 * Each compressed block contains 511 lateral-hash digests (SHA-1, 20 bytes each),
 * a 32-byte block hash (SHA-256), a diagnostics/info byte (DI), and ten cross-sum
 * vectors packed into a fixed-size 16,899-byte payload.
 */
#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace crsce::common::format {

    /**
     * @class CompressedPayload
     * @name CompressedPayload
     * @brief Holds and serializes one CRSCE compressed block (16,899 bytes).
     * @details
     * Block layout (B.27: 6 LTP sub-tables, uniform 511-cell lines, 9 bits each):
     *   Field    Elements  Bits/Element  Total Bits   Encoding
     *   LH       511       160           81,760       20 bytes per SHA-1 digest, sequential
     *   BH       1         256           256          32 bytes SHA-256 block hash
     *   DI       1         8             8            uint8
     *   LSM      511       9             4,599        MSB-first packed bitstream
     *   VSM      511       9             4,599        MSB-first packed bitstream
     *   DSM      1,021     variable      8,185        MSB-first, ceil(log2(len(d)+1))
     *   XSM      1,021     variable      8,185        MSB-first, ceil(log2(len(d)+1))
     *   LTP1SM   511       9             4,599        MSB-first, bit_width(ltp_len(k)) bits
     *   LTP2SM   511       9             4,599        MSB-first, bit_width(ltp_len(k)) bits
     *   LTP3SM   511       9             4,599        MSB-first, bit_width(ltp_len(k)) bits
     *   LTP4SM   511       9             4,599        MSB-first, bit_width(ltp_len(k)) bits
     *   LTP5SM   511       9             4,599        MSB-first, bit_width(ltp_len(k)) bits
     *   LTP6SM   511       9             4,599        MSB-first, bit_width(ltp_len(k)) bits
     *   Total                            135,186 bits = 16,899 bytes (rounded up)
     */
    class CompressedPayload {
    public:
        /**
         * @name kS
         * @brief Matrix dimension (number of rows and columns).
         */
        static constexpr std::uint16_t kS = 127;

        /**
         * @name kBlockPayloadBytes
         * @brief Exact size of one serialized block in bytes (16,899).
         *
         * B.27: 6 LTP sub-tables, each 511 × 9 bits = 4,599 bits.
         * Fixed 107,592 bits (LH+BH+DI+LSM+VSM+DSM+XSM) + 27,594 LTP bits = 135,186 bits.
         * ceil(135,186 / 8) = 16,898.25 → 16,899 bytes.
         */
        static constexpr std::size_t kBlockPayloadBytes = 1369;

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

        /**
         * @name kLHDigestBytes
         * @brief Size of a per-row lateral hash digest (SHA-1) in bytes.
         */
        static constexpr std::size_t kLHDigestBytes = 4;

        /**
         * @name kBHDigestBytes
         * @brief Size of the block hash digest (SHA-256) in bytes.
         */
        static constexpr std::size_t kBHDigestBytes = 32;

        // -- LH accessors --

        /**
         * @name setLH
         * @brief Store a 20-byte SHA-1 digest for row r.
         * @param r Row index in [0, kS).
         * @param digest The 20-byte digest to store.
         * @throws std::out_of_range if r >= kS.
         */
        void setLH(std::uint16_t r, const std::array<std::uint8_t, kLHDigestBytes> &digest);

        /**
         * @name getLH
         * @brief Retrieve the stored 20-byte SHA-1 digest for row r.
         * @param r Row index in [0, kS).
         * @return Copy of the 20-byte digest.
         * @throws std::out_of_range if r >= kS.
         */
        [[nodiscard]] std::array<std::uint8_t, kLHDigestBytes> getLH(std::uint16_t r) const;

        // -- BH accessors --

        /**
         * @name setBH
         * @brief Store the 32-byte SHA-256 block hash.
         * @param digest The 32-byte digest to store.
         * @throws None
         */
        void setBH(const std::array<std::uint8_t, kBHDigestBytes> &digest);

        /**
         * @name getBH
         * @brief Retrieve the stored 32-byte SHA-256 block hash.
         * @return Copy of the 32-byte digest.
         * @throws None
         */
        [[nodiscard]] std::array<std::uint8_t, kBHDigestBytes> getBH() const;

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

        // -- LTP1SM accessors (LTP1 partition) --

        /**
         * @name setLTP1SM
         * @brief Set the LTP1 partition sum at index k.
         * @param k Index in [0, kS).
         * @param value Sum value (max 511, fits in 9 bits).
         * @throws std::out_of_range if k >= kS.
         */
        void setLTP1SM(std::uint16_t k, std::uint16_t value);

        /**
         * @name getLTP1SM
         * @brief Get the LTP1 partition sum at index k.
         * @param k Index in [0, kS).
         * @return The stored sum value.
         * @throws std::out_of_range if k >= kS.
         */
        [[nodiscard]] std::uint16_t getLTP1SM(std::uint16_t k) const;

        // -- LTP2SM accessors (LTP2 partition) --

        /**
         * @name setLTP2SM
         * @brief Set the LTP2 partition sum at index k.
         * @param k Index in [0, kS).
         * @param value Sum value (max 511, fits in 9 bits).
         * @throws std::out_of_range if k >= kS.
         */
        void setLTP2SM(std::uint16_t k, std::uint16_t value);

        /**
         * @name getLTP2SM
         * @brief Get the LTP2 partition sum at index k.
         * @param k Index in [0, kS).
         * @return The stored sum value.
         * @throws std::out_of_range if k >= kS.
         */
        [[nodiscard]] std::uint16_t getLTP2SM(std::uint16_t k) const;

        // -- LTP3SM accessors (LTP3 partition) --

        /**
         * @name setLTP3SM
         * @brief Set the LTP3 partition sum at index k.
         * @param k Index in [0, kS).
         * @param value Sum value (max 511, fits in 9 bits).
         * @throws std::out_of_range if k >= kS.
         */
        void setLTP3SM(std::uint16_t k, std::uint16_t value);

        /**
         * @name getLTP3SM
         * @brief Get the LTP3 partition sum at index k.
         * @param k Index in [0, kS).
         * @return The stored sum value.
         * @throws std::out_of_range if k >= kS.
         */
        [[nodiscard]] std::uint16_t getLTP3SM(std::uint16_t k) const;

        // -- LTP4SM accessors (LTP4 partition) --

        /**
         * @name setLTP4SM
         * @brief Set the LTP4 partition sum at index k.
         * @param k Index in [0, kS).
         * @param value Sum value (max 511, fits in 9 bits).
         * @throws std::out_of_range if k >= kS.
         */
        void setLTP4SM(std::uint16_t k, std::uint16_t value);

        /**
         * @name getLTP4SM
         * @brief Get the LTP4 partition sum at index k.
         * @param k Index in [0, kS).
         * @return The stored sum value.
         * @throws std::out_of_range if k >= kS.
         */
        [[nodiscard]] std::uint16_t getLTP4SM(std::uint16_t k) const;

        // -- LTP5SM accessors (LTP5 partition) --

        /**
         * @name setLTP5SM
         * @brief Set the LTP5 partition sum at index k.
         * @param k Index in [0, kS).
         * @param value Sum value (max 511, fits in 9 bits).
         * @throws std::out_of_range if k >= kS.
         */
        void setLTP5SM(std::uint16_t k, std::uint16_t value);

        /**
         * @name getLTP5SM
         * @brief Get the LTP5 partition sum at index k.
         * @param k Index in [0, kS).
         * @return The stored sum value.
         * @throws std::out_of_range if k >= kS.
         */
        [[nodiscard]] std::uint16_t getLTP5SM(std::uint16_t k) const;

        // -- LTP6SM accessors (LTP6 partition) --

        /**
         * @name setLTP6SM
         * @brief Set the LTP6 partition sum at index k.
         * @param k Index in [0, kS).
         * @param value Sum value (max 511, fits in 9 bits).
         * @throws std::out_of_range if k >= kS.
         */
        void setLTP6SM(std::uint16_t k, std::uint16_t value);

        /**
         * @name getLTP6SM
         * @brief Get the LTP6 partition sum at index k.
         * @param k Index in [0, kS).
         * @return The stored sum value.
         * @throws std::out_of_range if k >= kS.
         */
        [[nodiscard]] std::uint16_t getLTP6SM(std::uint16_t k) const;

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
         * @brief Lateral hash digests: kS SHA-1 digests (20 bytes each).
         */
        std::vector<std::array<std::uint8_t, kLHDigestBytes>> lh_;

        /**
         * @name bh_
         * @brief Block hash: SHA-256 digest of the full CSM (32 bytes).
         */
        std::array<std::uint8_t, kBHDigestBytes> bh_{};

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

        /**
         * @name ltp1sm_
         * @brief LTP1 partition sum vector: kS elements, each in [0, kS].
         */
        std::vector<std::uint16_t> ltp1sm_;

        /**
         * @name ltp2sm_
         * @brief LTP2 partition sum vector: kS elements, each in [0, kS].
         */
        std::vector<std::uint16_t> ltp2sm_;

        /**
         * @name ltp3sm_
         * @brief LTP3 partition sum vector: kS elements, each in [0, kS].
         */
        std::vector<std::uint16_t> ltp3sm_;

        /**
         * @name ltp4sm_
         * @brief LTP4 partition sum vector: kS elements, each in [0, kS].
         */
        std::vector<std::uint16_t> ltp4sm_;

        /**
         * @name ltp5sm_
         * @brief LTP5 partition sum vector: kS elements, each in [0, kS].
         */
        std::vector<std::uint16_t> ltp5sm_;

        /**
         * @name ltp6sm_
         * @brief LTP6 partition sum vector: kS elements, each in [0, kS].
         */
        std::vector<std::uint16_t> ltp6sm_;

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
