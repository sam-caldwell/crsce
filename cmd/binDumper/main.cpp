/**
 * @file cmd/binDumper/main.cpp
 * @brief Dump a file as binary bits, 511 bits per line (MSB-first per byte).
 */
#include <cstddef>
#include <fstream>
#include <iostream>
#include <span>

/**
 * @brief Program entry point for binDumper.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Process exit code (0 on success).
 */
auto main(const int argc, char *argv[]) -> int {
    const std::span<char *> args{argv, static_cast<std::size_t>(argc)};
    if (args.size() != 2U) {
        std::cerr << "usage: binDumper <file>\n";
        return 2;
    }

    const char *path = args[1];
    std::ifstream is(path, std::ios::binary);
    if (!is) {
        std::cerr << "error: cannot open '" << path << "'\n";
        return 1;
    }

    constexpr std::size_t kBitsPerRow = 511U;
    constexpr std::size_t kRowsPerBlock = 511U;
    constexpr std::size_t kGroup = 128U;
    std::size_t col = 0;           // column within current row (bits printed in this row)
    std::size_t bit_offset = 0;    // total bits emitted so far
    std::size_t row_in_block = 0;  // current row index within 511x511 block
    char ch = 0;
    while (is.get(ch)) {
        const auto b = static_cast<unsigned char>(ch);
        for (int bit = 7; bit >= 0; --bit) {
            if (col == 0) {
                // Row prefix: 4-digit hexadecimal bit offset followed by ':'
                const auto v = static_cast<unsigned>(bit_offset & 0xFFFFU);
                const std::ios::fmtflags f(std::cout.flags());
                std::cout << std::hex;
                std::cout.width(4);
                std::cout.fill('0');
                std::cout << v;
                std::cout.flags(f);
                std::cout.put(':');
            }
            const char out = ((b >> bit) & 0x1U) ? '1' : '0';
            std::cout.put(out);
            ++col;
            ++bit_offset;
            if ((col % kGroup) == 0 && col < kBitsPerRow) {
                std::cout.put(' ');
            }
            if (col == kBitsPerRow) {
                std::cout.put('\n');
                col = 0;
                ++row_in_block;
                if (row_in_block == kRowsPerBlock) {
                    // Separate 511x511 blocks with a blank line
                    std::cout.put('\n');
                    row_in_block = 0;
                }
            }
        }
    }
    if (col != 0) {
        std::cout.put('\n');
    }
    return 0;
}
