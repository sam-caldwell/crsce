# cmake/pipeline/test.cmake
# (c) 2026 Sam Caldwell. See LICENSE.txt for details
#
# Centralized test pipeline helpers.
# - Discovers test sources to trigger reconfigure
# - Auto-registers unit tests under test/**/unit/*.cpp
# - Defines e2e tests for CLI binaries

# Ensure this file is only processed once even if included multiple places
include_guard(GLOBAL)

# Discover all test sources (recursive) to trigger reconfigure on changes
file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
  "${PROJECT_SOURCE_DIR}/test/*.cpp"
)

# Auto-register unit test executables for files under test/**/unit/*.cpp
set(UNIT_TEST_SOURCES)
foreach(TEST_CPP IN LISTS TEST_SOURCES)
  if(TEST_CPP MATCHES "/unit/.*\\.cpp$")
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
  target_include_directories(${TGT} PRIVATE "${PROJECT_SOURCE_DIR}/include")
  # Link project sources (object library)
  target_sources(${TGT} PRIVATE $<TARGET_OBJECTS:crsce_sources>)
  add_test(NAME ${TGT} COMMAND ${TGT})
endforeach()

# Define end-to-end tests for existing binaries
if(TARGET hello_world)
  add_test(NAME hello_world_e2e_prints_message COMMAND $<TARGET_FILE:hello_world>)
  set_tests_properties(hello_world_e2e_prints_message PROPERTIES PASS_REGULAR_EXPRESSION "hello world")
endif()

if(TARGET compress)
  add_test(NAME compress_e2e_prints_message COMMAND $<TARGET_FILE:compress>)
  set_tests_properties(compress_e2e_prints_message PROPERTIES PASS_REGULAR_EXPRESSION "Hello, World")

  add_test(NAME compress_e2e_happy_args
    COMMAND /bin/sh -c "$<TARGET_FILE:compress> -in in.tmp -out out.tmp"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
  add_test(NAME compress_e2e_happy_args_setup
    COMMAND /bin/sh -c "touch in.tmp; rm -f out.tmp"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
  set_tests_properties(compress_e2e_happy_args PROPERTIES DEPENDS compress_e2e_happy_args_setup)
  set_tests_properties(compress_e2e_happy_args PROPERTIES WORKING_DIRECTORY ${CMAKE_BINARY_DIR} PASS_REGULAR_EXPRESSION "Hello, World")

  add_test(NAME compress_e2e_missing_input
    COMMAND /bin/sh -c "$<TARGET_FILE:compress> -in missing.bin -out out.tmp; test $$? -ne 0"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
  set_tests_properties(compress_e2e_missing_input PROPERTIES WORKING_DIRECTORY ${CMAKE_BINARY_DIR} PASS_REGULAR_EXPRESSION "error: input file does not exist")

  add_test(NAME compress_e2e_output_exists
    COMMAND /bin/sh -c "touch in.tmp; touch out.tmp; $<TARGET_FILE:compress> -in in.tmp -out out.tmp; test $$? -ne 0"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
  set_tests_properties(compress_e2e_output_exists PROPERTIES WORKING_DIRECTORY ${CMAKE_BINARY_DIR} PASS_REGULAR_EXPRESSION "error: output file already exists")

  add_test(NAME compress_e2e_help_usage COMMAND $<TARGET_FILE:compress> -h)
  set_tests_properties(compress_e2e_help_usage PROPERTIES PASS_REGULAR_EXPRESSION "usage: compress")

  add_test(NAME compress_e2e_missing_value_usage
    COMMAND /bin/sh -c "$<TARGET_FILE:compress> -in; test $$? -ne 0")
  set_tests_properties(compress_e2e_missing_value_usage PROPERTIES PASS_REGULAR_EXPRESSION "usage: compress")

  add_test(NAME compress_e2e_unknown_flag_usage
    COMMAND /bin/sh -c "$<TARGET_FILE:compress> --bogus; test $$? -ne 0")
  set_tests_properties(compress_e2e_unknown_flag_usage PROPERTIES PASS_REGULAR_EXPRESSION "usage: compress")
endif()

if(TARGET decompress)
  add_test(NAME decompress_e2e_prints_message COMMAND $<TARGET_FILE:decompress>)
  set_tests_properties(decompress_e2e_prints_message PROPERTIES PASS_REGULAR_EXPRESSION "Hello, World")

  add_test(NAME decompress_e2e_happy_args
    COMMAND /bin/sh -c "$<TARGET_FILE:decompress> -in din.tmp -out dout.tmp"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
  add_test(NAME decompress_e2e_happy_args_setup
    COMMAND /bin/sh -c "touch din.tmp; rm -f dout.tmp"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
  set_tests_properties(decompress_e2e_happy_args PROPERTIES DEPENDS decompress_e2e_happy_args_setup)
  set_tests_properties(decompress_e2e_happy_args PROPERTIES WORKING_DIRECTORY ${CMAKE_BINARY_DIR} PASS_REGULAR_EXPRESSION "Hello, World")

  add_test(NAME decompress_e2e_missing_input
    COMMAND /bin/sh -c "$<TARGET_FILE:decompress> -in missing.bin -out dout.tmp; test $$? -ne 0"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
  set_tests_properties(decompress_e2e_missing_input PROPERTIES WORKING_DIRECTORY ${CMAKE_BINARY_DIR} PASS_REGULAR_EXPRESSION "error: input file does not exist")

  add_test(NAME decompress_e2e_output_exists
    COMMAND /bin/sh -c "touch din.tmp; touch dout.tmp; $<TARGET_FILE:decompress> -in din.tmp -out dout.tmp; test $$? -ne 0"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
  set_tests_properties(decompress_e2e_output_exists PROPERTIES WORKING_DIRECTORY ${CMAKE_BINARY_DIR} PASS_REGULAR_EXPRESSION "error: output file already exists")

  add_test(NAME decompress_e2e_help_usage COMMAND $<TARGET_FILE:decompress> -h)
  set_tests_properties(decompress_e2e_help_usage PROPERTIES PASS_REGULAR_EXPRESSION "usage: decompress")

  add_test(NAME decompress_e2e_missing_value_usage
    COMMAND /bin/sh -c "$<TARGET_FILE:decompress> -out; test $$? -ne 0")
  set_tests_properties(decompress_e2e_missing_value_usage PROPERTIES PASS_REGULAR_EXPRESSION "usage: decompress")

  add_test(NAME decompress_e2e_unknown_flag_usage
    COMMAND /bin/sh -c "$<TARGET_FILE:decompress> --bogus; test $$? -ne 0")
  set_tests_properties(decompress_e2e_unknown_flag_usage PROPERTIES PASS_REGULAR_EXPRESSION "usage: decompress")
endif()
