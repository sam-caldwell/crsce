/**
 * @file FileBitSerializer.h
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief Buffered bitwise reader for files (1 KiB chunked, MSB-first per byte).
 */
#pragma once

#include <cstddef>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace crsce::common {
    /**
     * @class FileBitSerializer
     * @name FileBitSerializer
     * @brief Provides a bit-by-bit interface over a file.
     *
     * Reads input in 1 KiB chunks and yields bits MSB-first from each byte. The
     * consumer calls pop() until it returns std::nullopt indicating EOF.
     */
    class FileBitSerializer {
    public:
        /**
         * @name kChunkSize
         * @brief Buffer size in bytes for each refill (1 KiB).
         */
        static constexpr std::size_t kChunkSize = 1 * 1024; ///< Buffer size (1 KiB)

        /**
         * @name FileBitSerializer
         * @brief Construct a bit serializer on a given path.
         * @usage FileBitSerializer s("/path.bin");
         * @throws None (check good() to verify open succeeded)
         * @param path File system path to open in binary mode.
         * @return N/A
         */
        explicit FileBitSerializer(const std::string &path);

        /**
         * @name has_next
         * @brief Check if at least one more bit can be produced.
         * @usage while (s.has_next()) { auto b = s.pop(); }
         * @throws None
         * @return true if another bit is available; false at EOF.
         */
        bool has_next();

        /**
         * @name pop
         * @brief Pop the next bit (MSB-first); returns nullopt at EOF.
         * @usage auto bit = s.pop();
         * @throws None
         * @return std::optional<bool> of the next bit or std::nullopt at EOF.
         */
        std::optional<bool> pop();

        /**
         * @name good
         * @brief True if the underlying file stream opened successfully.
         * @usage if (!s.good()) { handle_open_error(); }
         * @throws None
         * @return true if the stream is in a good state; false otherwise.
         */
        [[nodiscard]] bool good() const { return in_.good(); }

    private:
        /**
         * @name fill
         * @brief Fill the internal buffer by reading from the stream.
         * @usage if (!fill()) { handle_eof_or_error(); }
         * @throws None
         * @return true if data was read; false on EOF or failure.
         */
        bool fill();

        /**
         * @name in_
         * @brief File stream opened in binary mode.
         */
        std::ifstream in_;

        /**
         * @name buf_
         * @brief Byte buffer for the current chunk.
         */
        std::vector<char> buf_;

        /**
         * @name buf_len_
         * @brief Number of valid bytes currently in buf_.
         */
        std::size_t buf_len_{0};

        /**
         * @name byte_pos_
         * @brief Index of the next byte to read from buf_.
         */
        std::size_t byte_pos_{0};

        /**
         * @name bit_pos_
         * @brief Bit index in the current byte [0..7], where 0 is the MSB.
         */
        int bit_pos_{0};

        /**
         * @name eof_
         * @brief True once EOF has been reached during refill.
         */
        bool eof_{false};
    };
} // namespace crsce::common
