# cmake/pipeline/test.cmake
# (c) 2026 Sam Caldwell. See LICENSE.txt for details
## Centralized test discovery and registration
##
## Goals
## - Auto-discover tests by category and avoid manual add_test entries
## - Run tests in deterministic order: unit -> integration -> e2e -> tlt
## - Keep GoogleTest wiring local and isolated from clang-tidy

# Ensure this file is only processed once even if included multiple places
include_guard(GLOBAL)

# --- GoogleTest dependency (fetched locally) ---
include(FetchContent)
set(BUILD_GMOCK OFF CACHE BOOL "Build GoogleMock")
set(INSTALL_GTEST OFF CACHE BOOL "Disable gtest installation")
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.14.0
)
FetchContent_MakeAvailable(googletest)

# Do not run clang-tidy on third-party gtest targets
if (TARGET gtest)
  set_target_properties(gtest PROPERTIES CXX_CLANG_TIDY "")
  if (APPLE)
    target_compile_definitions(gtest PRIVATE _LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_NONE)
    target_compile_options(gtest PRIVATE -Wno-character-conversion)
  endif ()
endif ()
if (TARGET gtest_main)
  set_target_properties(gtest_main PROPERTIES CXX_CLANG_TIDY "")
  if (APPLE)
    target_compile_definitions(gtest_main PRIVATE _LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_NONE)
    target_compile_options(gtest_main PRIVATE -Wno-character-conversion)
  endif ()
endif ()

# Shared test helpers
include(cmake/pipeline/test/common.cmake)

# Delegate discovery/registration to category files for readability
include(cmake/pipeline/test/unit.cmake)
include(cmake/pipeline/test/integration.cmake)
include(cmake/pipeline/test/e2e.cmake)
include(cmake/pipeline/test/tlt.cmake)
