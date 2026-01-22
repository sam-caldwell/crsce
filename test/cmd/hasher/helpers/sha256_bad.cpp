// Helper: ignores stdin and prints a bogus digest
#include <iostream>
#include <string>

int main() {
    std::cout << std::string(64, '0') << "  -\n";
    return 0;
}
