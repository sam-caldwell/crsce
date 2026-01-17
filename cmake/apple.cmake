# apple.cmake
#
# Apple-specific global settings
#

# Prefer Homebrew LLVM C++ runtime at link/run time on macOS to avoid
# ABI mismatches with the system libc++ when using Homebrew clang++.
if(APPLE)
    if(CRSCE_MACOS_SDK AND NOT DEFINED CMAKE_OSX_SYSROOT)
        set(CMAKE_OSX_SYSROOT "${CRSCE_MACOS_SDK}" CACHE STRING "macOS SDK root" FORCE)
    endif()

    set(_BREW_LLVM_LIB "/opt/homebrew/opt/llvm/lib")
    if(EXISTS "${_BREW_LLVM_LIB}/libc++.1.dylib")
        # Ensure linker finds Homebrew libc++ first and record rpath for runtime
        set(CMAKE_BUILD_RPATH "${CMAKE_BUILD_RPATH};${_BREW_LLVM_LIB}" CACHE STRING "" FORCE)
        set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_RPATH};${_BREW_LLVM_LIB}" CACHE STRING "" FORCE)
        # Also add to link search paths
        link_directories("${_BREW_LLVM_LIB}")
    endif()
    # --- Global link options for macOS/Homebrew LLVM ---

    # Prefer Homebrew libc++ at link and runtime
    add_link_options("-L/opt/homebrew/opt/llvm/lib" "-Wl,-rpath,/opt/homebrew/opt/llvm/lib")
    add_link_options("-L/opt/homebrew/opt/llvm/lib/c++" "-Wl,-rpath,/opt/homebrew/opt/llvm/lib/c++")
    # Disable libc++ hardening (use the stable ABI without requiring newer dylib symbols)
    add_compile_definitions(_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_NONE)
    # Link to libc++ if available via absolute paths; otherwise fall back to c++/c++abi
    if(EXISTS "/opt/homebrew/opt/llvm/lib/c++/libc++.1.dylib" AND EXISTS "/opt/homebrew/opt/llvm/lib/c++/libc++abi.1.dylib")
        link_libraries("/opt/homebrew/opt/llvm/lib/c++/libc++.1.dylib"
                "/opt/homebrew/opt/llvm/lib/c++/libc++abi.1.dylib")
    else()
        link_libraries(c++ c++abi)
    endif()
endif()