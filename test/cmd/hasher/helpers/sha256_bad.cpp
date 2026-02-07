/*
 * @file sha256_bad.cpp
 * @brief Helper: ignores stdin and prints a bogus digest
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include <iostream>
#include <string>

int main() {
    std::cout << std::string(64, '0') << "  -\n";
    return 0;
}
