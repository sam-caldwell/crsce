# cmake/pipeline/build.cmake
# This file is intentionally empty.
# The build process is handled by 'cmake --build'.

# Trigger CMake reconfigure when any src/*.cpp changes (recursive)
file(GLOB_RECURSE PROJECT_SRC_CPP CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/src/*.cpp"
)
