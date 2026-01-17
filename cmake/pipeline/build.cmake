# cmake/pipeline/build.cmake
# (c) 2026 Sam Caldwell. See LICENSE.txt for details
#
# This file is intentionally empty.
# The build process is handled by 'cmake --build'.

# Trigger CMake reconfigure when any src/*.cpp changes (recursive)
file(GLOB_RECURSE PROJECT_SRC_CPP CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/src/*.cpp"
)
