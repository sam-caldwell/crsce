# cmake/pipeline/test.cmake
# (c) 2026 Sam Caldwell. See LICENSE.txt for details
#
# Centralized test pipeline helpers.
# - Fetches and wires GoogleTest for unit tests
# - Discovers test sources to trigger reconfigure
# - Auto-registers unit tests under test/**/unit/*.cpp
# - Defines e2e tests for CLI binaries

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
if(TARGET gtest)
  set_target_properties(gtest PROPERTIES CXX_CLANG_TIDY "")
  if(APPLE)
    target_compile_definitions(gtest PRIVATE _LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_NONE)
    target_compile_options(gtest PRIVATE -Wno-character-conversion)
  endif()
endif()

if(TARGET gtest_main)
  set_target_properties(gtest_main PROPERTIES CXX_CLANG_TIDY "")
  if(APPLE)
    target_compile_definitions(gtest_main PRIVATE _LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_NONE)
    target_compile_options(gtest_main PRIVATE -Wno-character-conversion)
  endif()
endif()

# Discover all test sources (recursive) to trigger reconfigure on changes
file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
  "${PROJECT_SOURCE_DIR}/test/*/*/unit/*_test.cpp"
  "${PROJECT_SOURCE_DIR}/test/*/*/*/unit/*_test.cpp"
  "${PROJECT_SOURCE_DIR}/test/*/integration/*_test.cpp"
  "${PROJECT_SOURCE_DIR}/test/*/*/integration/*_test.cpp"
  "${PROJECT_SOURCE_DIR}/test/*/*/*/integration/*_test.cpp"
  "${PROJECT_SOURCE_DIR}/test/*/e2e/*_test.cpp"
  "${PROJECT_SOURCE_DIR}/test/*/*/e2e/*_test.cpp"
  "${PROJECT_SOURCE_DIR}/test/*/*/*/e2e/*_test.cpp"
)

# Auto-register unit test executables for files under test/**/unit/*.cpp
set(UNIT_TEST_SOURCES)
foreach(TEST_CPP IN LISTS TEST_SOURCES)
  if(TEST_CPP MATCHES "/(unit|integration|e2e)/.*_test\\.cpp$")
    list(APPEND UNIT_TEST_SOURCES "${TEST_CPP}")
  endif()
endforeach()

foreach(UNIT_SRC IN LISTS UNIT_TEST_SOURCES)
  # Create a safe target name based on relative path
  file(RELATIVE_PATH REL_PATH "${PROJECT_SOURCE_DIR}/test" "${UNIT_SRC}")
  string(REPLACE "/" "_" TGT_NAME "${REL_PATH}")
  string(REPLACE ".cpp" "" TGT_NAME "${TGT_NAME}")
  set(TGT "test_${TGT_NAME}")

  add_executable(${TGT} "${UNIT_SRC}")
  target_include_directories(${TGT} PRIVATE "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/test")
  target_compile_definitions(${TGT} PRIVATE TEST_BINARY_DIR="${CMAKE_BINARY_DIR}")
  # Link project sources via static library to improve coverage attribution on macOS/LLVM
  target_link_libraries(${TGT} PRIVATE crsce_static)
  # Link with GoogleTest main
  target_link_libraries(${TGT} PRIVATE GTest::gtest GTest::gtest_main)
  # Link/runtime paths and libc++ linkage are handled globally in cmake/root.cmake
  add_test(NAME ${TGT} COMMAND ${TGT})
endforeach()

# --- Hello World E2E (gtest gate) ---
# Build and register a gtest-based end-to-end canary for the hello_world CLI.
set(HELLO_WORLD_GATE_TEST "")
if(TARGET hello_world)
  add_executable(test_hello_world_e2e_canary
    "${PROJECT_SOURCE_DIR}/test/hello_world/e2e/hello_world_canary.cpp")
  target_include_directories(test_hello_world_e2e_canary PRIVATE
    "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/test")
  target_link_libraries(test_hello_world_e2e_canary PRIVATE crsce_static GTest::gtest GTest::gtest_main)
  add_test(NAME hello_world_e2e_gtest COMMAND test_hello_world_e2e_canary)
  set(HELLO_WORLD_GATE_TEST hello_world_e2e_gtest)
endif()

# Define end-to-end tests for existing binaries
if(TARGET hello_world)
  add_test(NAME hello_world_e2e_prints_message COMMAND $<TARGET_FILE:hello_world>)
  set_tests_properties(hello_world_e2e_prints_message PROPERTIES PASS_REGULAR_EXPRESSION "hello world")
endif()

if(TARGET compress)
  add_test(NAME compress_e2e_prints_message COMMAND $<TARGET_FILE:compress>)
  set_tests_properties(compress_e2e_prints_message PROPERTIES PASS_REGULAR_EXPRESSION "Hello, World")

  add_test(NAME compress_e2e_happy_args
    COMMAND /bin/sh -c "rm -f out_ok.tmp; $<TARGET_FILE:compress> -in in_ok.tmp -out out_ok.tmp"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
  add_test(NAME compress_e2e_happy_args_setup
    COMMAND /bin/sh -c "touch in_ok.tmp; rm -f out_ok.tmp"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
  set_tests_properties(compress_e2e_happy_args PROPERTIES DEPENDS compress_e2e_happy_args_setup)
  set_tests_properties(compress_e2e_happy_args PROPERTIES WORKING_DIRECTORY ${CMAKE_BINARY_DIR} PASS_REGULAR_EXPRESSION "Hello, World")

  add_test(NAME compress_e2e_missing_input
    COMMAND /bin/sh -c "$<TARGET_FILE:compress> -in missing.bin -out out.tmp || true"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
  set_tests_properties(compress_e2e_missing_input PROPERTIES WORKING_DIRECTORY ${CMAKE_BINARY_DIR} PASS_REGULAR_EXPRESSION "error: input file does not exist")

  add_test(NAME compress_e2e_output_exists
    COMMAND /bin/sh -c "touch in.tmp; touch out.tmp; $<TARGET_FILE:compress> -in in.tmp -out out.tmp || true"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
  set_tests_properties(compress_e2e_output_exists PROPERTIES WORKING_DIRECTORY ${CMAKE_BINARY_DIR} PASS_REGULAR_EXPRESSION "error: output file already exists")

  add_test(NAME compress_e2e_help_usage COMMAND $<TARGET_FILE:compress> -h)
  set_tests_properties(compress_e2e_help_usage PROPERTIES PASS_REGULAR_EXPRESSION "usage: compress")

  add_test(NAME compress_e2e_missing_value_usage
    COMMAND /bin/sh -c "$<TARGET_FILE:compress> -in || true")
  set_tests_properties(compress_e2e_missing_value_usage PROPERTIES PASS_REGULAR_EXPRESSION "usage: compress")

  add_test(NAME compress_e2e_unknown_flag_usage
    COMMAND /bin/sh -c "$<TARGET_FILE:compress> --bogus || true")
  set_tests_properties(compress_e2e_unknown_flag_usage PROPERTIES PASS_REGULAR_EXPRESSION "usage: compress")
  # Prevent parallel races on shared temp files by locking a common resource
  set_tests_properties(
    compress_e2e_prints_message
    compress_e2e_happy_args_setup
    compress_e2e_happy_args
    compress_e2e_missing_input
    compress_e2e_output_exists
    compress_e2e_help_usage
    compress_e2e_missing_value_usage
    compress_e2e_unknown_flag_usage
    PROPERTIES RESOURCE_LOCK iofiles)
endif()

if(TARGET decompress)
  add_test(NAME decompress_e2e_prints_message COMMAND $<TARGET_FILE:decompress>)

  add_test(NAME decompress_e2e_happy_args_setup
    COMMAND /bin/sh -c ": > dsrc_ok.tmp; rm -f din_ok.tmp; $<TARGET_FILE:compress> -in dsrc_ok.tmp -out din_ok.tmp; rm -f dout_ok.tmp"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
  add_test(NAME decompress_e2e_happy_args
    COMMAND /bin/sh -c "$<TARGET_FILE:decompress> -in din_ok.tmp -out dout_ok.tmp"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
  set_tests_properties(decompress_e2e_happy_args PROPERTIES DEPENDS decompress_e2e_happy_args_setup)
  # Verify roundtrip matches original bytes
  add_test(NAME decompress_e2e_roundtrip_matches
    COMMAND /bin/sh -c "cmp -s dsrc_ok.tmp dout_ok.tmp"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
  set_tests_properties(decompress_e2e_roundtrip_matches PROPERTIES DEPENDS decompress_e2e_happy_args)

  add_test(NAME decompress_e2e_missing_input
    COMMAND /bin/sh -c "$<TARGET_FILE:decompress> -in missing.bin -out dout.tmp || true"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
  set_tests_properties(decompress_e2e_missing_input PROPERTIES WORKING_DIRECTORY ${CMAKE_BINARY_DIR} PASS_REGULAR_EXPRESSION "error: input file does not exist")

  add_test(NAME decompress_e2e_output_exists
    COMMAND /bin/sh -c "touch din.tmp; touch dout.tmp; $<TARGET_FILE:decompress> -in din.tmp -out dout.tmp || true"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
  set_tests_properties(decompress_e2e_output_exists PROPERTIES WORKING_DIRECTORY ${CMAKE_BINARY_DIR} PASS_REGULAR_EXPRESSION "error: output file already exists")

  add_test(NAME decompress_e2e_help_usage COMMAND $<TARGET_FILE:decompress> -h)
  set_tests_properties(decompress_e2e_help_usage PROPERTIES PASS_REGULAR_EXPRESSION "usage: decompress")

  add_test(NAME decompress_e2e_missing_value_usage
    COMMAND /bin/sh -c "$<TARGET_FILE:decompress> -out || true")
  set_tests_properties(decompress_e2e_missing_value_usage PROPERTIES PASS_REGULAR_EXPRESSION "usage: decompress")

  add_test(NAME decompress_e2e_unknown_flag_usage
    COMMAND /bin/sh -c "$<TARGET_FILE:decompress> --bogus || true")
  set_tests_properties(decompress_e2e_unknown_flag_usage PROPERTIES PASS_REGULAR_EXPRESSION "usage: decompress")
  # Prevent parallel races on shared temp files by locking a common resource
  set_tests_properties(
    decompress_e2e_prints_message
    decompress_e2e_happy_args_setup
    decompress_e2e_happy_args
    decompress_e2e_roundtrip_matches
    decompress_e2e_missing_input
    decompress_e2e_output_exists
    decompress_e2e_help_usage
    decompress_e2e_missing_value_usage
    decompress_e2e_unknown_flag_usage
    PROPERTIES RESOURCE_LOCK iofiles)
endif()

# End-to-end roundtrip for a multi-block (>=2) zero-filled file
if(TARGET compress AND TARGET decompress)
  # Create a zero file large enough to span >=2 blocks and run roundtrip
  # 2-block threshold: kBitsPerBlock = 511*511 = 261,121 bits -> ~32,640.125 bytes
  # Choose 40,000 bytes (>32,641) to ensure at least two blocks
  add_test(NAME roundtrip_e2e_twoblocks_setup
    COMMAND /bin/sh -c ": > twoblk_src.bin; python3 - <<'PY'\nimport sys\nf=open('twoblk_src.bin','wb')\nf.write(b'\\x00'*40000)\nf.close()\nPY\nrm -f twoblk.crsc twoblk_out.bin; $<TARGET_FILE:compress> -in twoblk_src.bin -out twoblk.crsc"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

  add_test(NAME roundtrip_e2e_twoblocks_decompress
    COMMAND /bin/sh -c "rm -f twoblk_out.bin; $<TARGET_FILE:decompress> -in twoblk.crsc -out twoblk_out.bin"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
  set_tests_properties(roundtrip_e2e_twoblocks_decompress PROPERTIES DEPENDS roundtrip_e2e_twoblocks_setup)

  add_test(NAME roundtrip_e2e_twoblocks_cmp
    COMMAND /bin/sh -c "rm -f twoblk_out.bin; $<TARGET_FILE:decompress> -in twoblk.crsc -out twoblk_out.bin; cmp -s twoblk_src.bin twoblk_out.bin"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
  set_tests_properties(roundtrip_e2e_twoblocks_cmp PROPERTIES DEPENDS roundtrip_e2e_twoblocks_setup)

  # Prevent parallel races on shared temp files
  set_tests_properties(
    roundtrip_e2e_twoblocks_setup
    roundtrip_e2e_twoblocks_decompress
    roundtrip_e2e_twoblocks_cmp
    PROPERTIES RESOURCE_LOCK iofiles)
endif()

# E2E tests for hasher using helper tools
if(TARGET hasher AND TARGET sha256_ok_helper AND TARGET sha256_bad_helper)
  add_test(NAME hasher_e2e_success_cmd
    COMMAND /bin/sh -c "CRSCE_HASHER_CMD=$<TARGET_FILE:sha256_ok_helper> $<TARGET_FILE:hasher>")

  add_test(NAME hasher_e2e_mismatch_cmd
    COMMAND /bin/sh -c "CRSCE_HASHER_CMD=$<TARGET_FILE:sha256_bad_helper> $<TARGET_FILE:hasher> || true")
  set_tests_properties(hasher_e2e_mismatch_cmd PROPERTIES PASS_REGULAR_EXPRESSION "candidate:")

  add_test(NAME hasher_e2e_no_tool
    COMMAND /bin/sh -c "CRSCE_HASHER_CMD=/nonexistent/sha256 $<TARGET_FILE:hasher> || true")
  set_tests_properties(hasher_e2e_no_tool PROPERTIES PASS_REGULAR_EXPRESSION "failed to run system tool")

  add_test(NAME hasher_e2e_candidate_success
    COMMAND /bin/sh -c "CRSCE_HASHER_CANDIDATE=$<TARGET_FILE:sha256_ok_helper> $<TARGET_FILE:hasher>")
endif()

# Fallback candidate test using system shasum if available
if(TARGET hasher)
  find_program(SYS_SHASUM shasum PATHS /usr/bin /opt/homebrew/bin NO_DEFAULT_PATH)
  if(SYS_SHASUM)
    add_test(NAME hasher_e2e_fallback_shasum
      COMMAND $<TARGET_FILE:hasher>)
  endif()
endif()

# --- Gate ordering: run hello_world e2e first ---
if(HELLO_WORLD_GATE_TEST)
  # Make all non-hello_world tests depend on the hello_world gtest E2E.
  get_property(ALL_TESTS DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} PROPERTY TESTS)
  foreach(T IN LISTS ALL_TESTS)
    if(NOT T STREQUAL "${HELLO_WORLD_GATE_TEST}" AND NOT T MATCHES "^hello_world")
      set_tests_properties(${T} PROPERTIES DEPENDS ${HELLO_WORLD_GATE_TEST})
    endif()
  endforeach()
endif()
