# File CMakeLists.txt
# Brief Root file for the cmake build system

cmake_minimum_required(VERSION 4.2)

project(crsce VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Disable C++ modules scanning to avoid generating module maps that confuse
# external tools like clang-tidy for this project (we do not use modules).
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_EXPERIMENTAL_CXX_MODULE_DYNDEP 0)

# Generate compile_commands.json for editor tooling
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Include the main cmake logic
include(cmake/root.cmake)
