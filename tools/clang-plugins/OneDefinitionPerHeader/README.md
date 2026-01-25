Build: integrated by repository tooling under tools/clang-plugins.
Usage example:

  clang++ -std=c++23 -c \
    -Xclang -load -Xclang build/tools/clang-plugins/OneDefinitionPerHeader/libOneDefinitionPerHeader.dylib \
    -Xclang -plugin -Xclang one-definition-per-header \
    include/path/to/header.h
