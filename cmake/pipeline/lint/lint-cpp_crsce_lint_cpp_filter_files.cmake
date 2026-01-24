
# Converts a list of absolute or relative file paths into the subset that
# matches the given FILTER_REGEX (relative to repo root).
function(_crsce_lint_cpp_filter_files FILTER_REGEX OUT_VAR)
  set(_out "")
  foreach (_f IN LISTS ARGN)
    # Ensure we have an absolute path before computing RELATIVE_PATH
    get_filename_component(_abs "${_f}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
    file(RELATIVE_PATH _rel "${CMAKE_SOURCE_DIR}" "${_abs}")
    file(TO_CMAKE_PATH "${_rel}" _rel) # normalize \ -> /
    if (_rel MATCHES "${FILTER_REGEX}" )
      list(APPEND _out "${_abs}")
    endif ()
  endforeach ()
  set(${OUT_VAR} "${_out}" PARENT_SCOPE)
endfunction()
