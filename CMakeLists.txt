cmake_minimum_required(VERSION 3.16)
project(SmartHub VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build options
option(BUILD_A7_APP "Build the A7 Linux application" ON)
option(BUILD_M4_FIRMWARE "Build the M4 bare-metal firmware" OFF)
option(BUILD_TESTS "Build unit tests" OFF)

if(BUILD_A7_APP)
    add_subdirectory(app)
endif()

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# M4 firmware uses separate toolchain, built independently
# Use: cmake -DBUILD_M4_FIRMWARE=ON -DCMAKE_TOOLCHAIN_FILE=m4-firmware/toolchain.cmake
