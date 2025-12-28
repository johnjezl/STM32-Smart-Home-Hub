# Cross-compilation toolchain for STM32MP1 (Cortex-A7)
#
# Usage:
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/arm-linux-gnueabihf.cmake \
#         -DBUILDROOT_SYSROOT=/path/to/buildroot/output/staging ..
#

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Buildroot sysroot (set via -DBUILDROOT_SYSROOT=...)
if(NOT DEFINED BUILDROOT_SYSROOT)
    set(BUILDROOT_SYSROOT "$ENV{HOME}/projects/smarthub/buildroot-src/output/staging")
endif()

# Toolchain prefix - Buildroot's external toolchain
set(TOOLCHAIN_PREFIX arm-linux-gnueabihf-)

# Try to find the toolchain in common locations
find_program(CMAKE_C_COMPILER
    NAMES ${TOOLCHAIN_PREFIX}gcc
    PATHS
        ${BUILDROOT_SYSROOT}/../host/bin
        /usr/bin
        /opt/arm-linux-gnueabihf/bin
    NO_DEFAULT_PATH
)

find_program(CMAKE_CXX_COMPILER
    NAMES ${TOOLCHAIN_PREFIX}g++
    PATHS
        ${BUILDROOT_SYSROOT}/../host/bin
        /usr/bin
        /opt/arm-linux-gnueabihf/bin
    NO_DEFAULT_PATH
)

# Fallback to system PATH
if(NOT CMAKE_C_COMPILER)
    set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}gcc)
endif()
if(NOT CMAKE_CXX_COMPILER)
    set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++)
endif()

# Sysroot
set(CMAKE_SYSROOT ${BUILDROOT_SYSROOT})
set(CMAKE_FIND_ROOT_PATH ${BUILDROOT_SYSROOT})

# Search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# Search for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Compiler flags for Cortex-A7
set(CMAKE_C_FLAGS_INIT "-mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard")
set(CMAKE_CXX_FLAGS_INIT "-mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard")

# pkg-config for cross-compilation
set(ENV{PKG_CONFIG_PATH} "${BUILDROOT_SYSROOT}/usr/lib/pkgconfig:${BUILDROOT_SYSROOT}/usr/share/pkgconfig")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "${BUILDROOT_SYSROOT}")
