#[[
  File: cmake/tools/num_cpus.cmake
  Brief: Provides crsce_num_cpus(<outVar> [LOGICAL|PHYSICAL]) to detect
         CPU core count (logical by default) in a cross‑platform way.

  Usage:
    include(cmake/tools/num_cpus.cmake)
    crsce_num_cpus(NPROC)           # logical cores
    crsce_num_cpus(NPHYS PHYSICAL)  # physical cores (best effort)
]]

include_guard(GLOBAL)

function(crsce_num_cpus OUT_VAR)
  set(_mode "LOGICAL")
  foreach(_arg IN LISTS ARGN)
    if(_arg STREQUAL "PHYSICAL")
      set(_mode "PHYSICAL")
    elseif(_arg STREQUAL "LOGICAL")
      set(_mode "LOGICAL")
    endif()
  endforeach()

  # Prefer CMake’s built-in host system information
  set(_n "")
  if(_mode STREQUAL "LOGICAL")
    cmake_host_system_information(RESULT _n QUERY NUMBER_OF_LOGICAL_CORES)
  else()
    cmake_host_system_information(RESULT _n QUERY NUMBER_OF_PHYSICAL_CORES)
  endif()

  # Fallbacks if CMake couldn’t determine a value
  if(NOT _n OR _n EQUAL 0)
    set(_out "")
    if(APPLE)
      if(_mode STREQUAL "PHYSICAL")
        execute_process(COMMAND /usr/sbin/sysctl -n hw.physicalcpu
                        OUTPUT_VARIABLE _out OUTPUT_STRIP_TRAILING_WHITESPACE
                        RESULT_VARIABLE _rc)
      else()
        execute_process(COMMAND /usr/sbin/sysctl -n hw.logicalcpu
                        OUTPUT_VARIABLE _out OUTPUT_STRIP_TRAILING_WHITESPACE
                        RESULT_VARIABLE _rc)
      endif()
      if(NOT _out)
        execute_process(COMMAND /usr/sbin/sysctl -n hw.ncpu
                        OUTPUT_VARIABLE _out OUTPUT_STRIP_TRAILING_WHITESPACE
                        RESULT_VARIABLE _rc)
      endif()
    elseif(UNIX)
      # Logical cores via nproc/getconf; physical is best-effort and may equal logical
      find_program(NPROC_EXE nproc)
      if(NPROC_EXE)
        if(_mode STREQUAL "PHYSICAL")
          execute_process(COMMAND ${NPROC_EXE} --all
                          OUTPUT_VARIABLE _out OUTPUT_STRIP_TRAILING_WHITESPACE
                          RESULT_VARIABLE _rc)
        else()
          execute_process(COMMAND ${NPROC_EXE}
                          OUTPUT_VARIABLE _out OUTPUT_STRIP_TRAILING_WHITESPACE
                          RESULT_VARIABLE _rc)
        endif()
      endif()
      if(NOT _out)
        execute_process(COMMAND getconf _NPROCESSORS_ONLN
                        OUTPUT_VARIABLE _out OUTPUT_STRIP_TRAILING_WHITESPACE
                        RESULT_VARIABLE _rc)
      endif()
      if(NOT _out AND EXISTS "/proc/cpuinfo")
        file(STRINGS "/proc/cpuinfo" _cpuinfo REGEX "^processor\\s*:.*$")
        list(LENGTH _cpuinfo _len)
        set(_out "${_len}")
      endif()
    elseif(WIN32)
      if(DEFINED ENV{NUMBER_OF_PROCESSORS})
        set(_out "$ENV{NUMBER_OF_PROCESSORS}")
      endif()
    endif()

    if(_out)
      # Extract leading digits to sanitize any trailing text
      string(REGEX MATCH "^[0-9]+" _digits "${_out}")
      if(_digits)
        set(_n "${_digits}")
      endif()
    endif()
  endif()

  if(NOT _n OR _n EQUAL 0)
    set(_n 1)
  endif()

  set(${OUT_VAR} "${_n}" PARENT_SCOPE)
endfunction()
