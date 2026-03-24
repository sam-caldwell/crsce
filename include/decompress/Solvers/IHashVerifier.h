/**
 * @file IHashVerifier.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Interface for row-hash verification in the solver.
 */
#pragma once

#include <array>
#include <cstdint>

namespace crsce::decompress::solvers {
    /**
     * @class IHashVerifier
     * @name IHashVerifier
     * @brief Abstracts row-hash computation and comparison.
     *
     * The default implementation uses SHA-256 per FIPS 180-4 on 512-bit row messages.
     * The interface admits alternative implementations (e.g., segmented prefix hashes)
     * without changing the solver's control flow.
     */
    class IHashVerifier {
    public:
        IHashVerifier(const IHashVerifier &) = delete;
        IHashVerifier &operator=(const IHashVerifier &) = delete;
        IHashVerifier(IHashVerifier &&) = delete;
        IHashVerifier &operator=(IHashVerifier &&) = delete;

        /**
         * @name ~IHashVerifier
         * @brief Virtual destructor.
         */
        virtual ~IHashVerifier() = default;

        /**
         * @name computeHash
         * @brief Compute the hash digest for a fully-assigned row.
         * @param row The row data as 8 uint64 words (512 bits, MSB-first).
         * @return 32-byte digest.
         * @throws None
         */
        [[nodiscard]] virtual auto computeHash(const std::array<std::uint64_t, 2> &row) const
            -> std::array<std::uint8_t, 32> = 0;

        /**
         * @name verifyRow
         * @brief Verify that a fully-assigned row matches its expected digest.
         * @param r Row index.
         * @param row The row data as 8 uint64 words.
         * @return True if the computed hash matches the stored expected digest.
         * @throws None
         */
        [[nodiscard]] virtual bool verifyRow(std::uint16_t r,
                                             const std::array<std::uint64_t, 2> &row) const = 0;

        /**
         * @name setExpected
         * @brief Store the expected digest for a row.
         * @param r Row index.
         * @param digest The 32-byte expected digest.
         * @throws None
         */
        virtual void setExpected(std::uint16_t r, const std::array<std::uint8_t, 32> &digest) = 0;

        /**
         * @name verifyColumn
         * @brief Verify that a fully-assigned column matches its expected digest.
         * @param c Column index.
         * @param col The column data as 8 uint64 words (MSB-first, same format as row).
         * @return True if the computed hash matches the stored expected column digest.
         * @throws None
         */
        [[nodiscard]] virtual bool verifyColumn(std::uint16_t c,
                                                const std::array<std::uint64_t, 2> &col) const {
            (void)c; (void)col;
            return true; // Default: no column verification (backward compatible)
        }

        /**
         * @name setExpectedColumn
         * @brief Store the expected digest for a column.
         * @param c Column index.
         * @param digest The 32-byte expected digest.
         * @throws None
         */
        virtual void setExpectedColumn(std::uint16_t c, const std::array<std::uint8_t, 32> &digest) {
            (void)c; (void)digest; // Default: no-op (backward compatible)
        }

        /**
         * @name hasRowHash
         * @brief Check whether row r has a hash stored.
         * @param r Row index.
         * @return True if row r has an expected digest.
         */
        [[nodiscard]] virtual bool hasRowHash(std::uint16_t r) const {
            (void)r;
            return true; // Default: all rows hashed (backward compatible with v1)
        }

        /**
         * @name hasColumnHash
         * @brief Check whether column c has a hash stored.
         * @param c Column index.
         * @return True if column c has an expected column digest.
         */
        [[nodiscard]] virtual bool hasColumnHash(std::uint16_t c) const {
            (void)c;
            return false; // Default: no column hashes (backward compatible)
        }

    protected:
        IHashVerifier() = default;
    };
} // namespace crsce::decompress::solvers
