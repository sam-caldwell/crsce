
# Converts a list of absolute file paths into the subset that matches test/**/*.cpp (relative to repo root).
function(_crsce_lint_cpp_filter_test_cpp_files OUT_VAR)
  set(_out "")
  foreach (_f IN LISTS ARGN)
    file(RELATIVE_PATH _rel "${CMAKE_SOURCE_DIR}" "${_f}")
    file(TO_CMAKE_PATH "${_rel}" _rel) # normalize \ -> /
    if (_rel MATCHES "^test/.*\\.cpp$")
      list(APPEND _out "${_f}")
    endif ()
  endforeach ()
  set(${OUT_VAR} "${_out}" PARENT_SCOPE)
endfunction()
