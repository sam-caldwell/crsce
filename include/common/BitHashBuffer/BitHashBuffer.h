/**
 * @file BitHashBuffer.h
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief Bitwise buffer that emits chained SHA-256 hashes per 64-byte row.
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace crsce::common {

    /**
     * @class BitHashBuffer
     * @brief Accumulates bits into bytes (MSB-first), hashing each 64-byte row.
     * @details The constructor hashes the provided seed string to derive a
     * 32-byte seed hash. Bits are packed MSB-first into a working byte. When 8
     * bits are accumulated, the byte is flushed to a 64-byte row buffer. When
     * the row fills, a digest is computed as sha256(prevHash||rowBuffer), where
     * prevHash is the original seed hash for the first row and the previous
     * digest for subsequent rows. Each digest is enqueued; popHash() returns
     * them FIFO.
     */
    class BitHashBuffer {
    public:
        /**
         * @name kRowSize
         * @brief Row size in bytes.
         */
        static constexpr std::size_t kRowSize = 64;   ///< Row size in bytes

        /**
         * @name kHashSize
         * @brief SHA-256 digest length in bytes.
         */
        static constexpr std::size_t kHashSize = 32;  ///< SHA-256 digest length

        /**
         * @name BitHashBuffer
         * @brief Construct with a seed string; initializes seed hash and state.
         * @usage BitHashBuffer buf("seed");
         * @throws None
         * @param seed Arbitrary string used to derive the initial seed hash.
         * @return N/A
         */
        explicit BitHashBuffer(const std::string &seed);

        /**
         * @name pushBit
         * @brief Push a single bit (MSB-first per byte) into the buffer.
         * @usage buf.pushBit(true);
         * @throws None
         * @param v Bit value; true = 1, false = 0.
         * @return N/A
         */
        void pushBit(bool v);

        /**
         * @name popHash
         * @brief Pop the first available hash (FIFO), if any.
         * @usage auto h = buf.popHash();
         * @throws None
         * @return Oldest 32-byte digest if available; std::nullopt otherwise.
         */
        std::optional<std::array<std::uint8_t, kHashSize>> popHash();

        /**
         * @name count
         * @brief Current number of hashes queued.
         * @usage auto n = buf.count();
         * @throws None
         * @return Number of hashes currently queued.
         */
        [[nodiscard]] std::size_t count() const noexcept { return hashVector_.size(); }

        /**
         * @name seedHash
         * @brief Return the current seed hash (32 bytes).
         * @usage const auto &seed = buf.seedHash();
         * @throws None
         * @return Const reference to the 32-byte seed hash.
         */
        [[nodiscard]] const std::array<std::uint8_t, kHashSize> &seedHash() const noexcept { return seedHash_; }

    private:
        /**
         * @name flushByteToRow
         * @brief Flush a full currByte_ into the row buffer.
         * @usage Internal: invoked when bitPos_ reaches 8.
         * @throws None
         * @return N/A
         */
        void flushByteToRow();

        /**
         * @name finalizeRowIfFull
         * @brief If 64 bytes accumulated, compute chained hash and reset the row.
         * @usage Internal: invoked after flushing bytes to check row completion.
         * @throws None
         * @return N/A
         */
        void finalizeRowIfFull();

        /**
         * @name seedHash_
         * @brief Initial/previous digest used for chained row hashing.
         * @usage seedHash_ is computed from the constructor seed; for the first
         * row hash, SHA-256(seedHash_ || rowBuffer_) is used.
         */
        std::array<std::uint8_t, kHashSize> seedHash_{};

        /**
         * @name hashVector_
         * @brief FIFO queue of computed 32-byte digests.
         * @usage push via finalizeRowIfFull(); retrieve via popHash().
         */
        std::vector<std::array<std::uint8_t, kHashSize>> hashVector_;

        /**
         * @name rowBuffer_
         * @brief Active 64-byte accumulation buffer; filled one byte at a time.
         * @usage Written by flushByteToRow(); cleared after finalizeRowIfFull().
         */
        std::array<std::uint8_t, kRowSize> rowBuffer_{};  ///< 64-byte row buffer

        /**
         * @name rowIndex_
         * @brief Next write index into rowBuffer_ [0..64).
         * @usage Incremented by flushByteToRow(); reset to 0 when a row finalizes.
         */
        std::size_t rowIndex_{0};

        /**
         * @name currByte_
         * @brief Working byte accumulator (MSB-first packing).
         * @usage pushBit() sets bits, then flushByteToRow() appends to rowBuffer_.
         */
        std::uint8_t currByte_{0};

        /**
         * @name bitPos_
         * @brief Bit index in currByte_ [0..7], where 0 is MSB position.
         * @usage Incremented by pushBit(); reset to 0 after a flush.
         */
        int bitPos_{0};
    };

}  // namespace crsce::common
