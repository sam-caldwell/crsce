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

    # --- Metal GPU acceleration ---
    if(CRSCE_ENABLE_METAL)
        find_library(METAL_FRAMEWORK Metal)
        find_library(FOUNDATION_FRAMEWORK Foundation)
        if(NOT METAL_FRAMEWORK OR NOT FOUNDATION_FRAMEWORK)
            message(FATAL_ERROR "Metal or Foundation framework not found; set CRSCE_ENABLE_METAL=OFF")
        endif()
        message(STATUS "Metal GPU acceleration enabled")
        message(STATUS "  Metal framework: ${METAL_FRAMEWORK}")
        message(STATUS "  Foundation framework: ${FOUNDATION_FRAMEWORK}")

        # Compile .metal -> .air -> .metallib via xcrun (optional: requires Metal Toolchain)
        # If the Metal compiler is not available, shaders are compiled from source at runtime.
        execute_process(
            COMMAND xcrun -sdk macosx -f metal
            OUTPUT_VARIABLE METAL_COMPILER
            ERROR_QUIET
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE METAL_COMPILER_RESULT
        )
        # Verify the compiler actually works (xcrun may find it but it may fail)
        if(METAL_COMPILER_RESULT EQUAL 0 AND METAL_COMPILER)
            execute_process(
                COMMAND "${METAL_COMPILER}" --version
                RESULT_VARIABLE METAL_COMPILER_TEST
                OUTPUT_QUIET
                ERROR_QUIET
            )
        else()
            set(METAL_COMPILER_TEST 1)
        endif()

        if(METAL_COMPILER_TEST EQUAL 0)
            message(STATUS "  Metal compiler: ${METAL_COMPILER} (offline shader compilation enabled)")
            file(GLOB_RECURSE METAL_SHADER_SOURCES CONFIGURE_DEPENDS
                "${PROJECT_SOURCE_DIR}/src/**/*.metal"
            )
            set(METAL_AIR_FILES "")
            set(METALLIB_OUTPUT "${PROJECT_SOURCE_DIR}/bin/crsce_shaders.metallib")
            foreach(SHADER_SRC IN LISTS METAL_SHADER_SOURCES)
                get_filename_component(SHADER_NAME "${SHADER_SRC}" NAME_WE)
                set(AIR_FILE "${CMAKE_BINARY_DIR}/metal/${SHADER_NAME}.air")
                add_custom_command(
                    OUTPUT "${AIR_FILE}"
                    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/metal"
                    COMMAND xcrun -sdk macosx metal -c "${SHADER_SRC}" -o "${AIR_FILE}"
                    DEPENDS "${SHADER_SRC}"
                    COMMENT "Compiling Metal shader ${SHADER_NAME}.metal -> ${SHADER_NAME}.air"
                    VERBATIM
                )
                list(APPEND METAL_AIR_FILES "${AIR_FILE}")
            endforeach()
            if(METAL_AIR_FILES)
                add_custom_command(
                    OUTPUT "${METALLIB_OUTPUT}"
                    COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_SOURCE_DIR}/bin"
                    COMMAND xcrun -sdk macosx metallib ${METAL_AIR_FILES} -o "${METALLIB_OUTPUT}"
                    DEPENDS ${METAL_AIR_FILES}
                    COMMENT "Linking Metal shaders -> crsce_shaders.metallib"
                    VERBATIM
                )
                add_custom_target(metal_shaders ALL DEPENDS "${METALLIB_OUTPUT}")
            else()
                add_custom_target(metal_shaders)
            endif()
        else()
            message(STATUS "  Metal compiler not available; shaders will be compiled from source at runtime")
            message(STATUS "  To enable offline compilation: xcodebuild -downloadComponent MetalToolchain")
            add_custom_target(metal_shaders)
        endif()
    endif()
endif()
