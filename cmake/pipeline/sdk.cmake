#[[
  File: cmake/pipeline/sdk.cmake
  Brief: Reusable helpers to detect and apply macOS SDK settings (SDKROOT)
         for DRY handling across lint, coverage, and other pipeline scripts.
]]

include_guard(GLOBAL)

# Detect macOS SDK path once per configure/script execution
if(APPLE AND NOT DEFINED CRSCE_MACOS_SDK)
  find_program(CRSCE_XCRUN_EXE xcrun)
  if(CRSCE_XCRUN_EXE)
    execute_process(
      COMMAND ${CRSCE_XCRUN_EXE} --show-sdk-path
      OUTPUT_VARIABLE CRSCE_MACOS_SDK
      OUTPUT_STRIP_TRAILING_WHITESPACE
      RESULT_VARIABLE _sdk_rc
    )
    if(NOT _sdk_rc EQUAL 0)
      set(CRSCE_MACOS_SDK "")
    endif()
  else()
    set(CRSCE_MACOS_SDK "")
  endif()
endif()

# Function: crsce_sdk_env_prefix(<outVar>)
#  Sets <outVar> to "SDKROOT=<path>" or empty string if not applicable.
function(crsce_sdk_env_prefix OUT_VAR)
  if(APPLE AND CRSCE_MACOS_SDK)
    set(_val "SDKROOT=${CRSCE_MACOS_SDK}")
  else()
    set(_val "")
  endif()
  set(${OUT_VAR} "${_val}" PARENT_SCOPE)
endfunction()

# Function: crsce_sdk_tidy_extra_args(<outVar>)
#  Provides clang-tidy extra args for SDK and libc++ when on macOS/Homebrew LLVM.
function(crsce_sdk_tidy_extra_args OUT_VAR)
  if(APPLE AND CRSCE_MACOS_SDK)
    set(_args "-extra-arg=-isysroot${CRSCE_MACOS_SDK};-extra-arg=-stdlib=libc++")
  else()
    set(_args "")
  endif()
  set(${OUT_VAR} "${_args}" PARENT_SCOPE)
endfunction()

# Function: crsce_sdk_configure_args(<outVar>)
#  Returns a list of -D flags to inject SDK into CMake configure calls.
function(crsce_sdk_configure_args OUT_VAR)
  if(APPLE AND CRSCE_MACOS_SDK)
    set(_cfg "-DCMAKE_OSX_SYSROOT=${CRSCE_MACOS_SDK}")
  else()
    set(_cfg "")
  endif()
  set(${OUT_VAR} "${_cfg}" PARENT_SCOPE)
endfunction()
