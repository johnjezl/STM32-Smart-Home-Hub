# Simplified cross-compilation toolchain for CI builds
#
# This toolchain is designed for GitHub Actions CI where:
# - Cross-compiler is installed via apt (gcc-arm-linux-gnueabihf)
# - ARM libraries are installed via apt with :armhf suffix
# - No Buildroot sysroot is available
#
# Usage:
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/arm-linux-gnueabihf-ci.cmake ..

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Toolchain prefix
set(TOOLCHAIN_PREFIX arm-linux-gnueabihf-)

# Compiler
set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++)

# Don't use a sysroot - use system paths for armhf libraries
# Libraries installed via apt are in /usr/lib/arm-linux-gnueabihf/
# Headers are in /usr/include/ (architecture-independent) or /usr/include/arm-linux-gnueabihf/

# Search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# For libraries and headers, search both host (for cross-compile libs) and target paths
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)

# Compiler flags for generic ARM (compatible with most ARM Linux systems)
set(CMAKE_C_FLAGS_INIT "-march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard")
set(CMAKE_CXX_FLAGS_INIT "-march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard")

# Help CMake find armhf libraries
set(CMAKE_LIBRARY_PATH /usr/lib/arm-linux-gnueabihf)
set(CMAKE_INCLUDE_PATH /usr/include/arm-linux-gnueabihf)

# pkg-config for cross-compilation
set(ENV{PKG_CONFIG_PATH} "/usr/lib/arm-linux-gnueabihf/pkgconfig")
set(ENV{PKG_CONFIG_LIBDIR} "/usr/lib/arm-linux-gnueabihf/pkgconfig")
