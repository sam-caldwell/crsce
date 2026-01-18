include("${CMAKE_CURRENT_LIST_DIR}/num_cpus.cmake")
crsce_num_cpus(_N)
# Print only the number, no prefixes
message(STATUS "${_N}")
